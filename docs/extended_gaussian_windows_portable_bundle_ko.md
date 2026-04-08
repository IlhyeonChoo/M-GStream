# Extended Gaussian Windows 설치 번들 가이드

작성일: 2026-04-08  
대상 브랜치: `develop/windows-portable-bundle`

## 1. 목적

이 문서는 `extended_gaussian`를 **다른 Windows 10/11 PC에서도 비교적 바로 실행**하기 위한 기준 절차를 정리한다.

이번 가이드는 다음을 목표로 한다.

- 새 Windows PC에서 source build / install / run
- 현재 머신에서 만들어 둔 `install/` 결과를 다른 PC로 전달하는 설치 번들 생성
- GPU가 장착된 다른 Windows PC에서 CUDA Toolkit 없이도 번들만으로 실행 가능하도록 필요한 런타임 포함
- manifest는 유지하되, 저장소에 없는 데이터셋은 사용자 제공 데이터 또는 번들 포함 데이터로 연결

이번 범위에 포함되지 않는 것은 다음과 같다.

- 인스톨러 제작
- OOM 수정
- Ubuntu 관련 변경

## 2. 권장 환경

- Windows 10 또는 Windows 11 x64
- NVIDIA GPU
- NVIDIA 드라이버 설치 완료
- Visual Studio 2022 C++ 워크로드
- CMake
- CUDA Toolkit 12.8 이상
- Python

권장 빌드 구성은 `RelWithDebInfo`다.

이유:

- `Debug`는 의존 DLL과 CRT 제약이 더 크다.
- `Release`보다 디버깅 정보가 남아 문제 추적이 쉽다.
- 현재 로컬 검증과 번들 기준도 `*_rwdi.exe` 중심이다.

권장 실행 파일도 `extended_gaussianViewer_app_rwdi.exe` 기준이다.

## 3. 새 Windows PC에서 직접 빌드하는 방법

저장소 루트에서 다음 순서로 진행한다.

```powershell
cmd.exe /d /s /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 && cmake -S . -B build-portable -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DEXTENDED_GAUSSIAN_CUDA_ARCHITECTURES=86-real;89-real;90-real;120'
```

```powershell
cmd.exe /d /s /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 && cmake --build build-portable --target extended_gaussianViewer_app'
```

```powershell
cmd.exe /d /s /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 && cmake --build build-portable --target extended_gaussianViewer_app_install'
```

`cmake --install build-portable` 로 전체 설치를 시도하면, 현재 viewer에 직접 필요하지 않은 다른 타깃이 아직 안 빌드된 경우 실패할 수 있다.  
Windows에서는 `extended_gaussianViewer_app_install` 타깃을 표준 설치 경로로 사용하는 것을 권장한다.

`RTX 50` 계열을 포함한 다른 Windows PC로 실행 파일을 옮길 계획이면, configure 시 CUDA 아키텍처를 최소 `86-real;89-real;90-real;120`으로 잡는 것을 기본값으로 유지하는 편이 안전하다.

설치 후 기본 실행은 아래 셋 중 하나로 한다.

```powershell
tools\windows\run_installed_viewer.cmd
```

```powershell
.\install\scripts\extended_gaussian\run_installed_viewer.cmd
```

```powershell
& ".\install\bin\extended_gaussianViewer_app_rwdi.exe" --appPath ".\install"
```

manifest를 같이 쓰려면:

```powershell
tools\windows\run_installed_viewer.cmd --manifest ".\manifests\mc_small_aerial_c36_neighbors_3x3.json"
```

`run_installed_viewer.cmd`는 인자 없이 실행해도, 아래 둘이 동시에 존재하면 sample manifest를 자동으로 붙인다.

- `manifests\mc_small_aerial_c36_neighbors_3x3.json`
- `swaptest\mc_small_aerial_c36`

## 4. 원클릭 빌드 + 설치 + 번들 생성

가장 쉬운 방법은 아래 스크립트 하나를 사용하는 것이다.

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\windows\build_windows_portable_bundle.ps1 -ZipBundle
```

이 스크립트는 순서대로 아래를 수행한다.

- 기본 build tree로 `build/`를 사용
- single-config build tree를 명시적으로 넘겼을 때 `CMAKE_BUILD_TYPE`와 `-Config`가 다르면 즉시 실패
- `extended_gaussianViewer_app` 빌드
- 빌드 직후 build tree의 CUDA 아키텍처가 `86-real;89-real;90-real;120` 범위를 포함하는지 검증
- `extended_gaussianViewer_app_install` 실행
- Windows portable bundle 생성
- 번들에 대해 runtime preflight 실행
- package 단계에서 수행되는 preflight는 artifact completeness만 확인하고, NVIDIA GPU 검사는 건너뜀
- sample manifest와 sample data가 같이 있으면 full preflight 추가 실행
- 필요 시 zip 생성

sample data까지 함께 묶어 바로 실행 가능한 번들을 만들고 싶으면:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\windows\build_windows_portable_bundle.ps1 -ZipBundle -IncludeSwaptestData
```

`-IncludeSwaptestData`는 로컬 `swaptest/`를 번들 안에 같이 복사한다.

기존 build tree가 예전에 `CMAKE_CUDA_ARCHITECTURES=52` 같은 값으로 configure되어 있었다면, 이 스크립트는 install/package 전에 중단된다. 이 경우 한 번 다시 configure해서 `EXTENDED_GAUSSIAN_CUDA_ARCHITECTURES=86-real;89-real;90-real;120`을 cache에 반영한 뒤 재실행한다.

### 4.1 가장 짧은 배포 실험 절차

다른 Windows PC에서 portable bundle만 빠르게 검증하고 싶다면 아래 순서로 진행한다.

현재 개발 PC의 **저장소 루트**에서:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\windows\build_windows_portable_bundle.ps1 -ZipBundle
```

sample data까지 같이 넣어 다른 PC에서 바로 sample manifest 렌더링까지 보고 싶으면:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\windows\build_windows_portable_bundle.ps1 -ZipBundle -IncludeSwaptestData
```

이 명령이 끝나면 아래 zip이 생성된다.

- `build\windows-portable-bundle\extended_gaussian-windows-portable.zip`

그 다음 순서는 아래와 같다.

1. 위 zip을 다른 Windows PC로 복사한다.
2. 다른 PC에서 zip을 압축 해제한다.
3. 압축을 푼 **번들 루트**에서 아래 preflight를 실행한다.

```powershell
powershell -ExecutionPolicy Bypass -File .\check_windows_runtime.ps1 -AppRoot .\install
```

4. preflight가 통과하면 같은 **번들 루트**에서 아래 런처를 실행한다.

```cmd
run_extended_gaussian_viewer.cmd
```

중요:

- 저장소 루트에서 preflight를 실행할 때는 `.\check_windows_runtime.ps1`가 아니라 `.\tools\windows\check_windows_runtime.ps1`를 사용해야 한다.
- `.\check_windows_runtime.ps1`는 압축을 푼 **번들 루트**에서만 맞는 경로다.

## 5. 설치 번들만 생성

현재 머신에서 설치 결과를 다른 Windows PC로 전달하려면 아래 스크립트를 사용한다.

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\windows\package_windows_portable_bundle.ps1
```

기본 출력 위치:

- `build\windows-portable-bundle\extended_gaussian-windows-portable`

특정 build config 산출물만 번들에 넣고 싶으면:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\windows\package_windows_portable_bundle.ps1 -Config Debug
```

지원하는 config 값은 `Debug`, `RelWithDebInfo`, `Release`, `MinSizeRel` 이다.
이 값을 주면 package 단계는 해당 config에 맞는 viewer executable만 선택하고, 기본적으로 다른 config executable로 fallback 하지 않는다.

실제 viewer executable postfix 규칙은 아래와 같다.

- `Debug` -> `extended_gaussianViewer_app_d.exe`
- `RelWithDebInfo` -> `extended_gaussianViewer_app_rwdi.exe`
- `Release` -> `extended_gaussianViewer_app.exe`
- `MinSizeRel` -> `extended_gaussianViewer_app_msr.exe`

zip까지 같이 만들려면:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\windows\package_windows_portable_bundle.ps1 -ZipBundle
```

sample data까지 같이 복사하려면:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\windows\package_windows_portable_bundle.ps1 -ZipBundle -IncludeSwaptestData
```

이 스크립트는 다음을 묶는다.

- `install/`
- `manifests/`
- 이 문서 사본
- runtime preflight 스크립트
- 번들 루트 실행 스크립트
- 선택된 viewer executable 이름을 기록한 `selected_viewer_exe.txt`
- `swaptest/README.txt` 또는 실제 `swaptest/` 데이터

이 스크립트는 `install/`을 수정하지 않는다.
즉 runtime DLL 포함 책임은 package 단계가 아니라 `extended_gaussianViewer_app_install` 단계에 있다.

package 단계는 이미 만들어진 `install/`을 복사하고, 번들 완성도를 확인하는 preflight를 수행한다.
이때 package-time preflight는 `-SkipGpuCheck`로 실행되므로, bundle을 조립하는 호스트에 NVIDIA GPU가 없어도 packaging 자체는 가능하다.
반대로 최종 실행 대상 PC에서는 아래 수동 preflight를 기본 인자 그대로 다시 실행해 GPU까지 포함한 최종 검증을 하는 편이 맞다.

`cmake --install` 이후에는 아래도 같이 설치된다.

- `install/scripts/extended_gaussian/run_installed_viewer.cmd`
- `install/scripts/extended_gaussian/check_windows_runtime.ps1`
- `install/docs/extended_gaussian_windows_portable_bundle_ko.md`
- `install/bin` 아래 viewer 실행에 필요한 runtime DLL

## 6. 번들 전달 후 다른 PC에서 실행하는 방법

번들을 압축 해제한 뒤, 번들 루트에서 아래 스크립트를 실행한다.

```cmd
run_extended_gaussian_viewer.cmd
```

이 런처는 번들 생성 시 기록된 `selected_viewer_exe.txt`를 먼저 읽고, 그 executable을 우선 실행한다.
같은 번들 루트에서 `check_windows_runtime.ps1`를 실행할 때도, `selected_viewer_exe.txt`가 있으면 같은 executable을 우선 검사한다.

manifest를 지정하려면:

```cmd
run_extended_gaussian_viewer.cmd --manifest ".\manifests\mc_small_aerial_c36_neighbors_3x3.json"
```

번들 루트에 아래 둘이 동시에 존재하면, 인자 없이 실행해도 sample manifest가 자동으로 붙는다.

- `manifests\mc_small_aerial_c36_neighbors_3x3.json`
- `swaptest\mc_small_aerial_c36`

실행 전에 번들 상태를 확인하려면:

```powershell
powershell -ExecutionPolicy Bypass -File .\check_windows_runtime.ps1 -AppRoot ".\install"
```

이 기본 호출은 runtime DLL뿐 아니라 NVIDIA GPU 존재도 같이 확인한다.
만약 packaging host처럼 GPU 없는 PC에서 artifact completeness만 먼저 보고 싶다면 아래처럼 `-SkipGpuCheck`를 붙일 수 있다.

```powershell
powershell -ExecutionPolicy Bypass -File .\check_windows_runtime.ps1 -AppRoot ".\install" -SkipGpuCheck
```

반대로 현재 개발 PC의 **저장소 루트**에서 install tree만 바로 검사하고 싶다면:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\windows\check_windows_runtime.ps1 -AppRoot ".\install"
```

## 7. 데이터 배치 규칙

저장소에는 `swaptest/` 데이터가 포함되어 있지 않다.  
즉, manifest만 복사해도 바로 렌더링되지는 않는다.

현재 예제 manifest들은 상대 경로 `../swaptest/...`를 사용한다.  
따라서 번들 루트 구조는 아래처럼 맞추는 것이 가장 쉽다.

```text
extended_gaussian-windows-portable/
  install/
  manifests/
  swaptest/
    mc_small_aerial_c36/
      cells/
      ...
  run_extended_gaussian_viewer.cmd
```

즉 사용자 제공 데이터는 **번들 루트 아래 `swaptest/`**에 두면 된다.

데이터를 다른 위치에 둘 경우에는 manifest의 `model_dir`를 직접 수정해야 한다.

## 8. 다른 Windows PC에서 바로 실행되기 위한 조건

이 번들은 아래 조건이 충족되면, CUDA Toolkit이 설치되지 않은 다른 Windows PC에서도 바로 실행하는 것을 목표로 한다.

- Windows 10/11 x64
- NVIDIA GPU
- NVIDIA 드라이버 설치 완료
- 번들에 포함된 `install/`, `manifests/`, 필요한 경우 `swaptest/` 유지

즉 **필수 조건은 NVIDIA 드라이버와 실제 GPU**이고, CUDA Toolkit 자체는 번들 사용 조건이 아니다.

다만 sample manifest로 실제 렌더링까지 보려면 해당 데이터가 번들에 있거나, 사용자가 `swaptest/`에 넣어야 한다.

## 9. baseline known issue

현재 baseline known issue는 다음과 같다.

- 증상:
  - viewer에서 카메라를 계속 이동하면 OOM이 발생할 수 있다.
- 로그 위치:
  - `src/projects/extended_gaussian/renderer/subsystem/rendering_system/GaussianView.cpp:81`
- 함수:
  - `sibr::resizeFunctional::<lambda>::operator()`
- 실패 지점:
  - scratch buffer 재할당 경로의 `cudaMalloc(ptr, 2 * N)`

이번 Windows portability 작업은 이 이슈를 수정하지 않는다.  
대신 다른 Windows PC에서도 동일 증상이 재현되는지 확인 항목으로 유지한다.

## 10. 권장 검증 순서

다른 Windows PC에서 최소한 아래 순서까지 확인하는 것을 권장한다.

1. `check_windows_runtime.ps1` 실행
2. `run_extended_gaussian_viewer.cmd` 실행
3. manifest 없이 UI 진입 확인
4. manifest 포함 실행
5. `Resource Browser`에 asset이 나타나는지 확인
6. `Scene Outliner`에서 manifest instance가 생성되는지 확인
7. `Current Phase` 변경 시 residency 통계가 변하는지 확인

실제 렌더링까지 보려면 사용자 제공 데이터가 올바른 위치에 있어야 한다.
