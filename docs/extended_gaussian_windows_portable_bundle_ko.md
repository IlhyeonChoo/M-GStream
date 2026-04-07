# Extended Gaussian Windows 설치 번들 가이드

작성일: 2026-04-08  
대상 브랜치: `develop/windows-portable-bundle`

## 1. 목적

이 문서는 `extended_gaussian`를 **다른 Windows 10/11 PC에서도 비교적 쉽게 실행**하기 위한 기준 절차를 정리한다.

이번 가이드는 다음을 목표로 한다.

- 새 Windows PC에서 source build / install / run
- 현재 머신에서 만들어 둔 `install/` 결과를 다른 PC로 전달하는 설치 번들 생성
- manifest는 유지하되, 저장소에 없는 데이터셋은 사용자 제공 데이터로 연결

이번 범위에 포함되지 않는 것은 다음과 같다.

- 인스톨러 제작
- 예제 데이터 포함 배포
- OOM 수정

## 2. 권장 환경

- Windows 10 또는 Windows 11 x64
- NVIDIA GPU
- NVIDIA 드라이버 설치 완료
- Visual Studio 2022 C++ 워크로드
- CMake
- CUDA Toolkit
- Python

권장 빌드 구성은 `RelWithDebInfo`다.

이유:

- `Debug`는 의존 DLL과 CRT 제약이 더 크다.
- `Release`보다 디버깅 정보가 남아 문제 추적이 쉽다.
- 현재 로컬 검증과 번들 기준도 `*_rwdi.exe` 중심이다.

## 3. 새 Windows PC에서 직접 빌드하는 방법

저장소 루트에서 다음 순서로 진행한다.

```powershell
cmd.exe /d /s /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 && cmake -S . -B build-portable -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo'
```

```powershell
cmd.exe /d /s /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 && cmake --build build-portable --target extended_gaussianViewer_app'
```

```powershell
cmd.exe /d /s /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 && cmake --install build-portable'
```

설치 후 기본 실행은 아래 둘 중 하나로 한다.

```powershell
tools\windows\run_installed_viewer.cmd
```

```powershell
& ".\install\bin\extended_gaussianViewer_app_rwdi.exe" --appPath ".\install"
```

manifest를 같이 쓰려면:

```powershell
tools\windows\run_installed_viewer.cmd --manifest ".\manifests\mc_small_aerial_c36_neighbors_3x3.json"
```

## 4. 설치 번들 생성

현재 머신에서 설치 결과를 다른 Windows PC로 전달하려면 아래 스크립트를 사용한다.

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\windows\package_windows_portable_bundle.ps1
```

기본 출력 위치:

- `build\windows-portable-bundle\extended_gaussian-windows-portable`

zip까지 같이 만들려면:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\windows\package_windows_portable_bundle.ps1 -ZipBundle
```

이 스크립트는 다음을 묶는다.

- `install/`
- `manifests/`
- 이 문서 사본
- 번들 루트 실행 스크립트
- `swaptest/README.txt`

## 5. 번들 전달 후 다른 PC에서 실행하는 방법

번들을 압축 해제한 뒤, 번들 루트에서 아래 스크립트를 실행한다.

```cmd
run_extended_gaussian_viewer.cmd
```

manifest를 지정하려면:

```cmd
run_extended_gaussian_viewer.cmd --manifest ".\manifests\mc_small_aerial_c36_neighbors_3x3.json"
```

## 6. 데이터 배치 규칙

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

## 7. baseline known issue

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

## 8. 권장 검증 순서

다른 Windows PC에서 최소한 아래 순서까지 확인하는 것을 권장한다.

1. `run_extended_gaussian_viewer.cmd` 실행
2. manifest 없이 UI 진입 확인
3. manifest 포함 실행
4. `Resource Browser`에 asset이 나타나는지 확인
5. `Scene Outliner`에서 manifest instance가 생성되는지 확인
6. `Current Phase` 변경 시 residency 통계가 변하는지 확인

실제 렌더링까지 보려면 사용자 제공 데이터가 올바른 위치에 있어야 한다.
