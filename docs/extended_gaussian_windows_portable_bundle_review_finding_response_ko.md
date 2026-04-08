# Windows Portable Bundle Review Finding Response

작성일: 2026-04-08  
기준 리뷰:

- `tools/windows/package_windows_portable_bundle.ps1:186-190`
- `tools/windows/package_windows_portable_bundle.ps1:36-40`

## 1. 목적

이번 문서는 최신 리뷰에서 추가로 지적된 두 가지 packaging 문제를 정리하고, 구현 전에 수정 방향을 명확히 고정하기 위해 작성한다.

대상 문제는 아래 두 가지다.

1. package 단계가 로컬 NVIDIA GPU 존재를 전제로 동작하는 문제
2. `MinSizeRel` config가 실제 산출물 postfix 규칙과 다르게 매핑되는 문제

## 2. 문제 1: package 단계가 로컬 NVIDIA GPU를 요구한다

### 2.1 현재 동작

- `package_windows_portable_bundle.ps1`는 번들 복사 직후 `check_windows_runtime.ps1`를 항상 호출한다.
- `check_windows_runtime.ps1`는 기본적으로 `Win32_VideoController`에서 NVIDIA GPU를 찾지 못하면 `exit 1`로 실패한다.
- 따라서 package 호스트가 GPU 없는 CI, staging PC, build worker여도 번들 조립 자체가 실패한다.

### 2.2 왜 문제인지

- bundle packaging의 핵심 책임은 전달물 구성을 완료하고, 번들 내부 파일이 빠지지 않았는지 확인하는 것이다.
- package 호스트는 실제 viewer 실행 대상 PC와 다를 수 있다.
- 즉 package 단계는 artifact completeness 검증과 target-machine hardware 검증을 분리해야 한다.

현재처럼 package 단계가 GPU까지 강제하면 아래가 모두 막힌다.

- GPU 없는 CI에서 zip만 생성하는 파이프라인
- build 전용 Windows VM에서 artifact만 모으는 단계
- 실제 viewer 실행은 다른 PC에서 할 예정인 staging packaging

### 2.3 수정 원칙

- `check_windows_runtime.ps1`의 기본 동작은 유지한다.
  - 사용자가 번들 루트나 install tree에서 직접 실행할 때는 계속 NVIDIA GPU를 검사한다.
- 대신 package 단계에서만 GPU 검사를 명시적으로 건너뛸 수 있어야 한다.
- 이를 위해 `check_windows_runtime.ps1`에 `-SkipGpuCheck` 옵션을 추가한다.
- `package_windows_portable_bundle.ps1`가 bundle preflight를 호출할 때는 항상 `-SkipGpuCheck`를 같이 넘긴다.

### 2.4 기대 결과

- package 호스트에 NVIDIA GPU가 없어도 번들 생성은 가능하다.
- package 단계에서는 runtime DLL 누락과 viewer executable 누락을 계속 잡는다.
- manifest JSON 오류와 asset data root 누락은 sample manifest와 sample data root가 함께 들어 있는 경우에 수행하는 full preflight에서 계속 잡는다.
- 최종 수신 PC에서는 기존처럼 `check_windows_runtime.ps1` 기본 호출로 GPU까지 포함한 최종 검증을 수행한다.

## 3. 문제 2: `MinSizeRel` config가 release 산출물 이름으로 매핑된다

### 3.1 현재 동작

- `package_windows_portable_bundle.ps1`의 `Get-ExecutableCandidates`는 `MinSizeRel`을 `extended_gaussianViewer_app.exe`로 매핑한다.
- 하지만 top-level `CMakeLists.txt`는 `CMAKE_MINSIZEREL_POSTFIX`를 `_msr`로 정의한다.
- 따라서 실제 `MinSizeRel` viewer 이름은 `extended_gaussianViewer_app_msr.exe`가 되어야 한다.

같은 문제는 runtime DLL 기대 이름에도 이어진다.

- `check_windows_runtime.ps1`는 `_d`, `_rwdi`만 구분하고 `_msr`는 고려하지 않는다.
- `run_portable_bundle.cmd`의 fallback 순서에도 `_msr` executable이 없다.

### 3.2 왜 문제인지

- `-Config MinSizeRel`로 package를 호출했을 때:
  - 실제 `*_msr.exe`를 찾지 못하고 실패할 수 있다.
  - 혹은 install tree에 release exe가 남아 있으면 잘못된 산출물을 조용히 집어갈 수 있다.
- 이런 문제는 가장 위험한 타입의 packaging bug다.
  - 명령은 성공했는데, 사용자가 의도한 config와 다른 binary가 번들에 들어갈 수 있기 때문이다.

### 3.3 수정 원칙

- `package_windows_portable_bundle.ps1`
  - `MinSizeRel`을 `extended_gaussianViewer_app_msr.exe`로 매핑한다.
  - fallback 후보 목록에도 `_msr`를 포함한다.
- `check_windows_runtime.ps1`
  - viewer executable 이름에서 `_msr` suffix를 인식한다.
  - `_msr` 기준 runtime DLL 패턴도 계산한다.
  - 기본 executable 탐색 목록에도 `_msr`를 포함한다.
- `run_portable_bundle.cmd`
  - `selected_viewer_exe.txt`가 없을 때의 fallback 후보에 `_msr`를 추가한다.

### 3.4 기대 결과

- `MinSizeRel` package 요청이 실제 `*_msr` 산출물과 일치한다.
- checker와 launcher도 같은 naming contract를 사용한다.
- config-aware packaging이 `Debug`, `RelWithDebInfo`, `Release`, `MinSizeRel` 네 경우 모두 일관된 규칙을 갖게 된다.

## 4. 구현 순서

1. 이 문서를 추가한다.
2. `check_windows_runtime.ps1`에 `-SkipGpuCheck`와 `_msr` suffix 처리를 추가한다.
3. `package_windows_portable_bundle.ps1`에서 package-time preflight에 `-SkipGpuCheck`를 전달하고 `MinSizeRel` 매핑을 고친다.
4. `run_portable_bundle.cmd` fallback 후보를 `_msr`까지 확장한다.
5. `docs/extended_gaussian_windows_portable_bundle_ko.md`에 package-time preflight와 `MinSizeRel -> _msr` 규칙을 반영한다.
6. `docs/extended_gaussian_modification_log_ko.md`에 이번 후속 수정을 기록한다.

## 5. 검증 계획

### 5.1 checker 단위 확인

- `check_windows_runtime.ps1 -AppRoot .\install -SkipDataCheck`가 기존처럼 통과하는지 확인
- `check_windows_runtime.ps1 -AppRoot .\install -SkipDataCheck -SkipGpuCheck`도 통과하는지 확인

### 5.2 package 확인

- `package_windows_portable_bundle.ps1 -Config RelWithDebInfo`가 기존처럼 bundle 생성에 성공하는지 확인
- package 출력 로그에 GPU check를 강제하지 않는 흐름이 반영되었는지 확인

### 5.3 naming contract 확인

- 번들 루트의 `selected_viewer_exe.txt`가 선택된 viewer 이름을 유지하는지 확인
- fallback launcher가 `_msr`까지 포함하는지 확인
- 문서가 `MinSizeRel -> _msr` 규칙을 현재 구현과 동일하게 설명하는지 확인
