# AGENTS.md

This file provides guidance for any coding agent or human contributor working with code in this repository.

## 프로젝트 요약

**SIBR 기반 C++/CUDA Gaussian Splatting 뷰어/에디터**. 모델을 **학습하지 않습니다**. 사전 학습된 Gaussian Splatting 결과 디렉터리를 로드하여 CPU 에셋으로 보관하고, 그 에셋을 참조하는 씬 인스턴스를 만들고, 필요할 때 공유 GPU 복사본을 업로드한 뒤, 합성된 씬을 외부 CUDA 래스터라이저로 렌더링합니다.

호스트 환경: Windows 10/11. 기본 셸은 PowerShell 또는 `cmd` 를 가정하고, bash 전용 문법이나 `/dev/null` 같은 POSIX 전용 경로는 전제하지 마세요.

## 건드리기 전에 알아야 할 제약 (가장 중요)

코드만 봐서는 알 수 없거나, "버그처럼 보이지만 의도된" 것들. 자동으로 "고치지" 마세요.

- `archive_system` 과 `UI_system` 은 placeholder 입니다. 실제 구현된 서브시스템은 `rendering_system` 뿐.
- 현재 코드 기준 서브시스템 enum 값은 `ARCHIVE_SYSTEM` 이고, 씬-렌더 동기화 콜백은 `onInstanceCreated` / `onInstanceUpdated` 를 사용합니다.
- 오래된 문서나 과거 커밋에는 이전 typo 표기가 남아 있을 수 있습니다. 이 이름들을 다시 바꿔야 한다면 부분 수정이 아니라 인터페이스-구현-호출부를 포함한 일괄 리네이밍으로 처리하세요.
- `GaussianLoader` 는 SH degree 0~3 을 파싱하지만, 다운스트림 월드 버퍼는 degree-3 레이아웃으로 하드코딩되어 있습니다. 더 낮은 degree 의 모델 로딩도 동작은 하지만 가정이 취약함.
- 뷰어는 `"Gaussian View"` 라는 이름의 단일 1차 IBR subview 를 전제로 만들어져 있습니다. `RenderingSystem` / `GaussianView` 가 이 이름을 가정.

## 외부 의존성 경계

실제 Gaussian 래스터라이즈 커널은 **이 저장소에 없습니다**. `src/projects/M_GStream/renderer/CMakeLists.txt` 가 configure 시점에 `https://github.com/graphdeco-inria/diff-gaussian-rasterization.git` 를 fetch 합니다.

본 저장소의 코드는 버퍼를 준비하고 그 래스터라이저를 호출만 합니다. forward/backward CUDA 커널 소스가 로컬에 보이지 않더라도 정상 — 저장소 안에서 찾으려 하지 마세요.

## 빌드와 실행

툴체인: Visual Studio 2019, CMake ≥ 3.22 (top-level `CMakeLists.txt` 가 강제), CUDA ≥ 10.1, Python 3.8+, Doxygen, 7zip.

이미 configure 된 두 개의 out-of-source 빌드 트리가 저장소에 존재합니다. 새로 configure 하지 말고 그대로 재사용하세요:

- `build/` — Visual Studio generator
- `build-ninja/` — Ninja generator

대표 명령:

```sh
# Ninja 로 빌드 + 설치
cmake --build build-ninja --target M_GStreamViewer_app
cmake --build build-ninja --target install --parallel
# app만 install하고 싶으면 아래를 사용
cmake --build build-ninja --target M_GStreamViewer_app_install --parallel
# 실행
install/bin/M_GStreamViewer_app.exe
# 문서
cmake --build build-ninja --target DOCUMENTATION   # 출력: install/docs/index.html
```

Visual Studio 쪽은 `build/sibr_projects.sln` 을 열어 `ALL_BUILD` → `INSTALL` 순으로 빌드.

**테스트 스위트도, lint 타깃도 없습니다.** `ctest`, 단위 테스트 러너, 정적 분석 명령 모두 존재하지 않습니다. 변경 검증은 `M_GStreamViewer_app` 을 직접 실행해서 합니다. Linux에서는 `cmake --install build-ninja` 가 누락된 타깃을 빌드하지 않으므로, 먼저 `cmake --build build-ninja --target install --parallel` 또는 `cmake --build build-ninja --target M_GStreamViewer_app_install --parallel` 을 사용하세요. 필요한 타깃을 이미 빌드한 뒤에는 `cmake --install build-ninja` 도 동작합니다.

빌드 타깃:
- `M_GStream` — 프로젝트 공유 라이브러리 (렌더러 코드)
- `M_GStreamViewer_app` — 사용자가 실제로 실행하는 실행 파일

## 런타임 흐름

```text
main
 -> create Window
 -> create ExtendedGaussianViewer
    -> create GaussianScene
    -> create ResourceManager
    -> create RenderingSystem
       -> create GaussianView
       -> register "Gaussian View" IBR subview
       -> create camera handler and bind it to the view
       -> create RenderGaussianScene
 -> main loop
    -> Input::poll()
    -> viewer.onUpdate()
    -> viewer.onRender()
       -> MultiViewBase::onRender()
          -> renderSubView()
             -> MonoRdrMode::render()
                -> GaussianView::onRenderIBR()
                   -> build world buffers
                   -> call CUDA rasterizer
                   -> copy result to render target
    -> viewer.onSwapBuffer()
```

렌더링 버그를 추적할 때 진입 순서:

1. `src/projects/M_GStream/apps/M_GStreamViewer/main.cpp`
2. `src/projects/M_GStream/renderer/ExtendedGaussianViewer.cpp`
3. `src/projects/M_GStream/renderer/subsystem/rendering_system/RenderingSystem.cpp`
4. `src/core/view/MultiViewManager.cpp`
5. `src/core/view/RenderingMode.cpp`
6. `src/projects/M_GStream/renderer/subsystem/rendering_system/GaussianView.cpp`

## 데이터 모델: 4-객체 분리 (혼동 주의)

| 객체 | 의미 | 소유자 |
|---|---|---|
| `GaussianField` | CPU 에셋 | `ResourceManager` |
| `GaussianInstance` | 에셋의 씬 배치 | `GaussianScene` |
| `GPUGaussianField` | 에셋의 공유 GPU 복사본 (에셋당 하나, 인스턴스 간 공유) | `GPUResourceManager` |
| `RenderGaussianInstance` | 씬 인스턴스 ↔ GPU 필드 브릿지 | `RenderGaussianScene` |

메모리/변환 버그를 디버깅할 때는 편집 전에 이 네 계층 중 어느 쪽이 잘못되었는지부터 식별하세요.

또한 렌더러는 두 종류의 GPU 버퍼를 유지합니다:
- `GaussianField` 당 **공유 GPU 에셋 버퍼**
- 합성된 씬을 위해 프레임 간 재사용되는 **영구 월드 버퍼** (재할당이 아니라 필요 시 *확장*)

## 에셋 임포트 계약

기대되는 모델 디렉터리 형태:

```text
<modelPath>/
  cfg_args
  point_cloud/
    iteration_XXXX/
      point_cloud.ply
```

임포트 경로:

```text
UI "Import PLY" (이름과 달리 디렉터리 picker)
 -> GaussianLoader::load(modelPath)
 -> ResourceManager::addField(...)
```

`GaussianLoader` 의 처리:
- 선택된 모델 디렉터리 이름에서 에셋 id/이름 도출
- `cfg_args` 를 읽어 `sh_degree` 추출
- 가장 최신 `iteration_*` 폴더 선택
- `point_cloud.ply` 로드
- 쿼터니언 정규화 / 스케일 exp / opacity sigmoid / SH 계수 내부 레이아웃으로 재정렬
- 점들을 Morton 정렬로 공간 정렬

`ResourceManager` 는 에셋을 모델 디렉터리 basename 으로 키잉하며 **중복을 거부**합니다 — basename 이 같은 두 개의 서로 다른 디렉터리를 임포트하면 실패.

씬 인스턴스 생성 경로:

```text
UI "Create New Instance"
 -> GaussianScene::createInstance(...)
 -> RenderingSystem::onInstanceCreated(...)
 -> RenderGaussianScene::createInstance(...)
 -> RenderGaussianInstance(...)
 -> GPUResourceManager lookup/create
```

## 프레임 단위 합성 (`GaussianView::onRenderIBR()`)

핫패스. 순서:

1. 출력 버퍼 클리어
2. GPU 필드를 가진 인스턴스들의 Gaussian 총 개수 카운트
3. 영구 월드 버퍼를 필요할 때만 확장
4. 프레임당 카메라 데이터 업로드
5. 모든 인스턴스를 월드 공간 버퍼에 append
6. CUDA 커널로 인스턴스별 변환 적용
7. `CudaRasterizer::Rasterizer::forward(...)` 호출
8. 결과를 활성 렌더 타깃에 복사
9. 다음 프레임을 위한 append 카운터 리셋

## 편집 대상 룩업

작업 시작 시 가장 자주 참조하는 표:

| 바꾸려는 것 | 편집 대상 |
|---|---|
| 시작 시점 / 최상위 UI | `ExtendedGaussianViewer.{hpp,cpp}` |
| 임포트 동작, 모델 디렉터리 규칙 | `GaussianLoader.{hpp,cpp}` |
| 에셋 이름, 중복 처리, CPU 에셋 레지스트리 | `ResourceManager.{hpp,cpp}` |
| 씬 객체 동작 | `GaussianInstance.*`, `GaussianScene.*` |
| GPU 캐싱 | `GPUResourceManager.*`, `GPUGaussianField.*` |
| 프레임당 합성 / 래스터라이저 입력 | `GaussianView.{hpp,cpp}` |
| 인스턴스→월드 변환 (CUDA) | `TransformKernels.{cuh,cu}` |
| 프레임워크 수준 subview / 카메라 오케스트레이션 | `src/core/view/*` (탐색만) |

## Key Files By Responsibility

모든 경로는 `src/projects/M_GStream/` 기준. 디렉터리만 표시된 항목은 해당 디렉터리 전체를 하나의 응집된 단위로 다루세요 — 개별 파일은 `ls` / `Glob` 으로 확인.

### Entry / App Shell
- `apps/M_GStreamViewer/main.cpp`
- `renderer/ExtendedGaussianViewer.{hpp,cpp}`

### CPU Asset / Scene
- `renderer/resource/` — CPU 에셋 레이어 (`GaussianField`, `GaussianLoader`, `ResourceManager`)
  - `renderer/resource/GaussianSet.hpp` — 데이터 모델엔 있으나 렌더링에 미연결인 spatial hierarchy/container
- `renderer/scene/` — 씬 배치 (`GaussianInstance`, `GaussianScene`)

### Render-Side
- `renderer/subsystem/Subsystem.hpp` — subsystem base/enum
- `renderer/subsystem/rendering_system/RenderingSystem.{hpp,cpp}` — 서브시스템 라이프사이클
- `renderer/subsystem/rendering_system/GaussianView.{hpp,cpp}` — 프레임 핫패스
- `renderer/subsystem/rendering_system/RenderGaussianScene.{hpp,cpp}`
- `renderer/subsystem/rendering_system/RenderGaussianInstance.{hpp,cpp}`
- `renderer/subsystem/rendering_system/gpu_resource_manager/` — 공유 GPU 에셋 캐시 (`GPUResourceManager`, `GPUGaussianField`)

### CUDA
- `renderer/cuda/` — 인스턴스→월드 변환 커널 (`TransformKernels.{cuh,cu}`)

### Shared SIBR Framework
- `src/core/{system,graphics,view,renderer,assets,scene,video}` — 공유 SIBR 코드. 프레임워크 수준의 subview/camera/window 오케스트레이션을 바꿀 때만 진입. 그 외에는 직접 grep 으로 탐색.

## 추가 참고 문서

더 깊은 한국어 워크스루와 설계 컨텍스트가 필요할 때:

- `docs/M_GStream_branch_commit_rules_ko.md` — 브랜치 전략, 커밋 prefix, PR/merge 규칙
- `docs/M_GStream_code_flow_phase0_ko.md` — 현재 Phase 0 기준 런타임 워크스루
- `docs/M_GStream_modification_log_ko.md` — 변경 이력
- `docs/M_GStream_vs_sibr_viewer_ko.md` — 본 뷰어와 upstream SIBR 뷰어의 차이
- `docs/sibr_gaussian_swap_detailed_design.md` — PLY swap 설계

`README.md` 는 upstream SIBR Core 문서이지 본 프로젝트 전용 문서가 아닙니다. SIBR 빌드 사전 요구사항 확인 용도로만 사용하세요.
