# extended_gaussian 수정 및 검증 기록

작성일: 2026-04-03

> 주의: 본 문서는 2026-04-03 시점 스냅샷에서 시작했으며, 아래에 후속 메모를 계속 덧붙인다. 현재 상태 판단은 코드와 `AGENTS.md`를 우선한다.

## 0. 2026-04-08 후속 메모

### 0.1 `develop/phase1-manifest-swap -> main` 머지 전 known issue

`develop/phase1-manifest-swap` 브랜치는 Phase 1 manifest / swap 구현과 후속 정리 커밋들을 포함한 상태이며, `main` 머지 후보로 취급하고 있다.  
다만 머지 시점에 아래 known issue를 반드시 함께 기록한다.

- 증상
  - viewer에서 카메라를 계속 이동하면 OOM이 발생할 수 있다.
- 관측 로그 위치
  - `src/projects/extended_gaussian/renderer/subsystem/rendering_system/GaussianView.cpp:81`
- 관측 함수
  - `sibr::resizeFunctional::<lambda>::operator()`
- 실패 지점
  - scratch buffer 재할당 경로의 `cudaMalloc(ptr, 2 * N)`

해당 코드는 현재 다음 형태다.

- `GaussianView.cpp:71`
  - `resizeFunctional(void** ptr, size_t& S)`
- `GaussianView.cpp:81`
  - `CUDA_SAFE_CALL(cudaMalloc(ptr, 2 * N));`

현재 판단은 다음과 같다.

- 이 이슈는 `develop/phase1-manifest-swap`의 `main` 머지를 막는 블로커로 취급하지 않는다.
- 대신 `main` 머지 시점의 **baseline known issue**로 남긴다.
- 이후 Windows 이식성, Ubuntu 24.04 포팅, Ubuntu Server 원격 스트리밍 브랜치에서는 모두 이 이슈를 추적 대상으로 유지한다.
- 특히 headless EGL 경로와 remote server mode에서도 장시간 카메라 이동 시 재현 여부를 별도로 다시 확인해야 한다.

## 1. 목적

이번 작업의 목적은 두 가지였다.

- `docs/sibr_gaussian_swap_detailed_design.md` 기준의 **Phase 0 리팩터링**을 실제 코드에 반영한다.
- Windows 로컬 환경에서 `extended_gaussian` 프로젝트가 **다시 빌드되고 실행되도록** 막고 있던 오류를 정리한다.

이번 문서는 위 두 축에서 어떤 수정을 했는지와, 어디까지 검증했는지를 기록한다.

---

## 2. 구조 리팩터링

### 2.1 자산 identity를 raw pointer에서 `asset_id`로 전환

동적 swap 설계의 전제 조건은 CPU 메모리 상의 `GaussianField*`가 사라져도 scene instance와 GPU cache의 identity가 유지되는 것이다. 이를 위해 다음 변경을 적용했다.

- `GaussianInstance`가 더 이상 `const GaussianField*`를 보관하지 않고 `std::string asset_id`를 보관하도록 변경했다.
- `GaussianScene`의 인스턴스 생성, 조회, 교체 흐름을 `asset_id` 기준으로 바꿨다.
- `GPUResourceManager`의 캐시 키를 CPU pointer가 아니라 `asset_id`로 변경했다.
- `RenderingSystem`이 렌더 직전에 `asset_id`를 통해 `ResourceManager`에서 CPU asset을 찾아 GPU 업로드를 보장하도록 바꿨다.
- `RenderGaussianInstance`가 장기적인 GPU ownership을 직접 쥐지 않고 manager 기반 흐름에 맞게 정리했다.

이 변경으로 CPU asset unload / reload 이후에도 논리적 자산 identity를 유지할 수 있는 기반이 생겼다.

### 2.2 Viewer / UI 레이어 동작 정리

구조 변경에 맞춰 viewer 쪽도 함께 수정했다.

- `ExtendedGaussianViewer`의 asset 추가, 인스턴스 생성, 자산 교체 UI가 pointer 대신 asset 이름 기반으로 동작하도록 변경했다.
- scene이 참조 중인 asset은 삭제하지 못하게 막았다.
- 리소스 브라우저에서 맵을 순회하면서 즉시 삭제하던 위험한 흐름을 제거했다.

### 2.3 구조 리팩터링 관련 수정 파일

- `src/projects/extended_gaussian/renderer/ExtendedGaussianViewer.hpp`
- `src/projects/extended_gaussian/renderer/ExtendedGaussianViewer.cpp`
- `src/projects/extended_gaussian/renderer/scene/GaussianInstance.hpp`
- `src/projects/extended_gaussian/renderer/scene/GaussianInstance.cpp`
- `src/projects/extended_gaussian/renderer/scene/GaussianScene.hpp`
- `src/projects/extended_gaussian/renderer/scene/GaussianScene.cpp`
- `src/projects/extended_gaussian/renderer/subsystem/rendering_system/GaussianView.cpp`
- `src/projects/extended_gaussian/renderer/subsystem/rendering_system/RenderGaussianInstance.hpp`
- `src/projects/extended_gaussian/renderer/subsystem/rendering_system/RenderGaussianInstance.cpp`
- `src/projects/extended_gaussian/renderer/subsystem/rendering_system/RenderingSystem.hpp`
- `src/projects/extended_gaussian/renderer/subsystem/rendering_system/RenderingSystem.cpp`
- `src/projects/extended_gaussian/renderer/subsystem/rendering_system/gpu_resource_manager/GPUGaussianField.hpp`
- `src/projects/extended_gaussian/renderer/subsystem/rendering_system/gpu_resource_manager/GPUGaussianField.cpp`
- `src/projects/extended_gaussian/renderer/subsystem/rendering_system/gpu_resource_manager/GPUResourceManager.hpp`
- `src/projects/extended_gaussian/renderer/subsystem/rendering_system/gpu_resource_manager/GPUResourceManager.cpp`

---

## 3. 빌드 및 실행 복구 작업

### 3.1 Windows `std::max` / `windows.h` 충돌 수정

전체 빌드는 `src/core/graphics/Window.cpp`의 `std::max(0, key)`에서 먼저 멈췄다. 원인은 Windows 헤더의 `max` 매크로 충돌이었다.

적용한 수정은 다음과 같다.

- `src/core/system/Config.hpp`에서 Windows 환경에 `NOMINMAX`를 정의했다.
- `src/core/graphics/Window.cpp`의 호출을 `(std::max)(0, key)`로 변경했다.

이 수정으로 `Window.cpp` 컴파일 오류가 제거됐다.

### 3.2 CUDA Rasterizer 툴체인 정리

그 다음 블로커는 외부 의존성 `extlibs/CudaRasterizer`였다. CUDA 13 계열 로컬 툴체인과 기존 설정이 맞지 않아 세 종류의 오류가 연속으로 발생했다.

- `Unsupported gpu architecture 'compute_70'`
- MSVC traditional preprocessor 사용 오류
- `libcu++ requires at least C++ 17`

이를 해결하기 위해 `extlibs/CudaRasterizer/CudaRasterizer/CMakeLists.txt`와 `src/projects/extended_gaussian/renderer/CMakeLists.txt`를 다음과 같이 수정했다.

- `CudaRasterizer`의 CUDA 아키텍처를 상위 `CMAKE_CUDA_ARCHITECTURES`를 따르도록 변경했다.
- 상위 값이 없을 때만 fallback으로 `"75;86"`을 사용하게 했다.
- `CudaRasterizer`의 `CMAKE_CUDA_STANDARD`를 14에서 17로 올렸다.
- MSVC + CUDA 조합에서 `--compiler-options=/Zc:preprocessor`를 적용했다.
- `extended_gaussian` 본체 CUDA 타깃에도 같은 host compiler 옵션을 적용했다.
- `src/projects/extended_gaussian/renderer/CMakeLists.txt`의 `CMAKE_CUDA_ARCHITECTURES native` 설정을 그대로 활용하도록 맞췄다.

이 수정으로 CUDA 관련 컴파일 오류가 제거됐다.

### 3.3 실행 시 install/resource/shader 탐색 오류 수정

빌드가 끝난 뒤 실행 파일은 바로 종료됐는데, 원인은 크래시가 아니라 경로 해석 문제였다. 순서대로 다음 오류가 발생했다.

- `Can't find subfolder resources ...`
- `File not found: texture.fp`

문제의 핵심은 `src/core/system/Utils.cpp`의 `getInstallDirectory()`가 빌드 트리에서 실행될 때 실제 `install` 루트를 찾지 못하고, 에러 메시지에 적힌 `--appPath`도 실제로 반영하지 않는다는 점이었다.

적용한 수정은 다음과 같다.

- `getInstallDirectory()`가 `--appPath`를 실제로 읽고 사용하도록 수정했다.
- 경로 후보에서 상위 디렉터리로 올라가며 `bin`, `resources`, `shaders`를 찾도록 정리했다.
- 루트 바로 아래가 아니라 `<root>/install/resources`, `<root>/install/shaders` 구조만 존재하는 경우에는 반환 경로를 `<root>/install`으로 맞추도록 수정했다.

이 수정 후에는 빌드 트리에서 실행해도 resources / shaders를 정상적으로 찾는다.

### 3.4 로컬 CUDA 툴체인 준비

환경 제약 때문에 시스템 전역 CUDA 설치 대신 로컬 툴체인을 구성했다.

- 7-Zip 설치 후 외부 의존성 다운로드가 정상 동작했고, ASSIMP는 `extlibs/assimp` 아래에 자동 배치됐다.
- NVIDIA CUDA Toolkit 시스템 설치는 UAC 단계에서 중단되어, 설치 프로그램에서 필요한 패키지를 추출해 `local_cuda` 디렉터리를 구성했다.
- configure / build는 Visual Studio Developer Command Prompt 환경과 `local_cuda/bin/nvcc.exe`를 이용하는 방식으로 진행했다.
- bring-up 과정에서 host compiler 우회용 helper로 `tools/cl-wrapper.cmd` 파일도 추가했다.

### 3.5 실행 wrapper 및 런타임 DLL 배치 보완

빌드 성공 이후에도 사용자 입장에서는 `build-ninja/.../extended_gaussianViewer_app_rwdi.exe`를 직접 실행했을 때 다음 문제가 있었다.

- `sibr_system_rwdi.dll`, `sibr_graphics_rwdi.dll`, `extended_gaussian_rwdi.dll` 등 런타임 DLL을 찾지 못함
- 초기 `run_extended_gaussian_viewer.cmd` 스크립트가 프로젝트 루트를 한 단계 잘못 계산하여 잘못된 `install` 경로를 참조함

이를 해결하기 위해 다음 보완을 적용했다.

- `build-ninja/src/projects/extended_gaussian/apps/extended_gaussianViewer/run_extended_gaussian_viewer.cmd`를 추가했다.
- wrapper는 실행 전 `PATH`에 필요한 build / extlibs 디렉터리를 넣고, `--appPath`를 현재 프로젝트의 `install` 경로로 전달하도록 구성했다.
- wrapper의 루트 경로 계산 버그를 수정해 `C:\\Users\\...\\extended_gaussian\\install`를 정확히 찾도록 고쳤다.
- 사용 편의를 위해 build 출력 exe 폴더에도 필요한 DLL들을 복사해 두었다.

다만 이 wrapper는 `build-ninja/` generator 출력 디렉터리 안에 생기는 산출물이므로, 저장소의 영구 추적 파일이라기보다 각 환경에서 재생성되는 실행 보조 파일로 보는 편이 맞다.

현재 기준으로는 wrapper 실행과 direct exe 실행이 모두 가능한 상태다.

---

## 4. 추가/수정된 핵심 파일

빌드/실행 복구와 직접 관련된 주요 파일은 다음과 같다.

- `src/core/system/Config.hpp`
- `src/core/graphics/Window.cpp`
- `src/core/system/Utils.cpp`
- `src/projects/extended_gaussian/renderer/CMakeLists.txt`
- `extlibs/CudaRasterizer/CudaRasterizer/CMakeLists.txt`
- `tools/cl-wrapper.cmd`
- `build-ninja/src/projects/extended_gaussian/apps/extended_gaussianViewer/run_extended_gaussian_viewer.cmd`

---

## 5. 검증 결과

### 5.1 빌드 검증

다음 타깃 빌드 성공을 확인했다.

- `extended_gaussian`
- `extended_gaussianViewer_app`

또한 구조 리팩터링의 직접 수정 대상인 다음 오브젝트들이 실제로 컴파일됨을 확인했다.

- `ExtendedGaussianViewer.cpp.obj`
- `GaussianInstance.cpp.obj`
- `GaussianScene.cpp.obj`
- `GaussianView.cpp.obj`
- `RenderGaussianInstance.cpp.obj`
- `RenderGaussianScene.cpp.obj`
- `RenderingSystem.cpp.obj`
- `GPUGaussianField.cpp.obj`
- `GPUResourceManager.cpp.obj`

### 5.2 실행 스모크 테스트

다음 실행 파일이 빌드되었음을 확인했다.

- `build-ninja/src/projects/extended_gaussian/apps/extended_gaussianViewer/extended_gaussianViewer_app_rwdi.exe`

다음 두 실행 경로를 각각 검증했다.

- `build-ninja/src/projects/extended_gaussian/apps/extended_gaussianViewer/extended_gaussianViewer_app_rwdi.exe`
- `build-ninja/src/projects/extended_gaussian/apps/extended_gaussianViewer/run_extended_gaussian_viewer.cmd`

두 경우 모두 5초 스모크 테스트에서 프로세스가 정상 실행 상태를 유지하는 것을 확인한 뒤 강제 종료했다. 즉, 현재 기준으로는 “실행 직후 즉시 종료하는 오류”까지는 해소된 상태다.

### 5.3 현재 권장 사용 방법

이번 수정 이후 사용 방법은 다음과 같다.

- 가장 안전한 실행 방법은 `build-ninja/src/projects/extended_gaussian/apps/extended_gaussianViewer` 폴더에서 `run_extended_gaussian_viewer.cmd`를 실행하는 것이다.
- 현재 워크스페이스에서는 같은 폴더의 `extended_gaussianViewer_app_rwdi.exe`를 직접 실행해도 동작하도록 필요한 DLL을 함께 배치해 두었다.
- 반면 `install/bin` 실행 경로는 의존 DLL 번들링이 완전히 정리된 상태는 아니므로, 당장은 build 출력 폴더 기준 실행을 권장한다.

### 5.4 뷰어 실행 후 실제 사용 순서

현재 viewer는 실행 직후 자동으로 Gaussian asset을 로드하거나 scene instance를 만들지 않는다. 따라서 사용 순서는 아래와 같다.

1. viewer를 실행한다.
2. 상단 메뉴에서 `Panels`를 열고 `Resource Browser`와 `Scene Outliner`를 켠다.
3. `Resource Browser`에서 `Import PLY`를 누른다.
4. 파일 하나가 아니라 **Gaussian 모델 디렉터리**를 선택한다.

현재 로더가 기대하는 디렉터리 구조는 대략 다음과 같다.

- `<model_dir>/cfg_args`
- `<model_dir>/point_cloud/iteration_xxxxx/point_cloud.ply`

즉 버튼 이름은 `Import PLY`이지만, 실제 입력은 `.ply` 파일 하나가 아니라 **훈련 결과 폴더**다.

5. asset이 `Resource Browser` 타일 목록에 나타나면, `Scene Outliner`에서 `Create New Instance`를 누른다.
6. 인스턴스 이름을 넣고 `Gaussian Asset` 콤보에서 방금 import한 asset 이름을 선택한다.
7. 필요하면 `Position`, `Rotation`, `Scale`를 조정한 뒤 `Create`를 누른다.
8. 생성된 instance를 선택하면 우측 상세 패널에서 transform과 연결 asset을 계속 수정할 수 있다.

현재 렌더링은 scene에 instance가 있어야만 보인다. 즉 `Import PLY`만 하고 instance를 만들지 않으면 화면에는 아무 것도 나타나지 않는다.

### 5.5 현재 조작 키와 UI 동작

현재 기준으로 바로 알아야 할 조작은 다음과 같다.

- `Ctrl + Alt + G`: GUI 숨김/복구
- `Esc`: 종료
- 기본 카메라 이동: `W`, `A`, `S`, `D`, `Q`, `E`
- 카메라 회전: `I`, `J`, `K`, `L`, `U`, `O`
- 카메라 모드 전환: `y`는 trackball mode, `b`는 orbit mode, `v`는 interpolation mode

패널 동작은 다음과 같다.

- `Scene Outliner`: instance 생성, 선택, transform 수정, asset 교체, instance 삭제
- `Resource Browser`: asset import, asset 선택, 우클릭 삭제

asset 삭제에는 제약이 있다.

- 어떤 instance라도 해당 asset을 참조 중이면 삭제되지 않는다.
- 먼저 instance에서 다른 asset으로 바꾸거나 instance 자체를 삭제해야 asset 삭제가 가능하다.

---

## 6. 남아 있는 경고 및 후속 과제

현재 기준에서 빌드/실행을 막지는 않지만 다음 항목은 남아 있다.

- 기존 소스와 CUDA 헤더에서 발생하는 `C4819` 코드페이지 경고
- 일부 기존 소스에서의 `C4244`, `C4267`, `C4834` 경고
- 실제 Gaussian 데이터셋을 로드해서 swap 흐름까지 확인하는 기능 검증은 아직 수행하지 않음
- 시스템 전역 CUDA Toolkit 설치가 아니므로, 다른 개발자가 같은 방식으로 재현하려면 `local_cuda` 기반 절차를 공유해야 함

다음 단계로는 manifest / residency / swap policy를 추가하는 Phase 1 구현과, 실제 샘플 데이터셋 기반 기능 검증이 자연스럽다.
