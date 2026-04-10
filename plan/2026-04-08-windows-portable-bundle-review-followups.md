# Windows Portable Bundle Review Follow-up Plan

작성일: 2026-04-08
기준 문서: `docs/extended_gaussian_windows_portable_bundle_review_ko.md`

## 1. 요약

- 현재 작업 트리의 `Windows portable bundle` 변경분 리뷰 결과를 바탕으로 후속 수정 작업을 정리한다.
- 우선순위는 build tree/config 정합성 보장, preflight 엄격화, runtime 책임 단일화 순으로 둔다.
- 구현 대상은 `build_windows_portable_bundle.ps1`, `check_windows_runtime.ps1`, `package_windows_portable_bundle.ps1`, `docs/extended_gaussian_windows_portable_bundle_ko.md`다.
- `run_installed_viewer.cmd`와 `run_portable_bundle.cmd`의 기본 manifest 자동 선택 동작은 유지한다.

## 2. 핵심 변경

### 2.1 build 스크립트 정합성 고정

- 대상 파일: `tools/windows/build_windows_portable_bundle.ps1`
- 기본 `BuildRoot`는 `build/`로 변경한다.
- `BuildRoot` 아래 `CMakeCache.txt`를 읽어 generator 종류와 `CMAKE_BUILD_TYPE`를 판별한다.
- multi-config generator면 기존처럼 `--config <Config>`를 사용한다.
- single-config generator면 cache의 `CMAKE_BUILD_TYPE`와 사용자가 요청한 `-Config`를 비교한다.
- 둘이 다르면 즉시 실패시키고, 오류 메시지에 `BuildRoot`, generator, cached build type, requested config를 모두 출력한다.
- single-config generator에서 값이 일치하면 그때만 진행한다.

### 2.2 preflight 엄격화

- 대상 파일: `tools/windows/check_windows_runtime.ps1`
- 사용자가 `-ManifestPath`를 직접 넘긴 경우:
  - 파일이 없으면 즉시 실패한다.
  - 디렉터리를 넘긴 경우도 즉시 실패한다.
- manifest 파싱은 `try/catch`로 감싸고, JSON 형식 오류를 명확히 보고한다.
- `assets`가 없거나 객체가 아니면 manifest 형식 오류로 처리한다.
- 종료 코드는 다음으로 고정한다.
  - `0`: runtime 및 데이터 검증 통과
  - `1`: runtime 누락 또는 NVIDIA GPU 미검출
  - `2`: manifest asset data root 누락
  - `3`: 잘못된 manifest 경로 또는 manifest 형식 오류

### 2.3 runtime 책임 단일화

- 대상 파일:
  - `src/projects/extended_gaussian/apps/extended_gaussianViewer/CMakeLists.txt`
  - `tools/windows/package_windows_portable_bundle.ps1`
  - `tools/windows/build_windows_portable_bundle.ps1`
- runtime DLL 포함 책임은 CMake install 단계로 고정한다.
- 단, 이 정리는 `extended_gaussianViewer_app_install` 단계의 unresolved dependency 경고가 실제로 해소된 뒤에만 진행한다.
- 현재 남아 있는 `cudart64_12.dll`, `xatlas_d.dll` unresolved 경고를 먼저 없애지 않으면, package의 backfill 제거 순간 다시 배포가 깨질 수 있다.
- `package_windows_portable_bundle.ps1`의 `Copy-MatchingFiles`, `Copy-PortableRuntime` 및 `install/bin` 수정 로직은 제거한다.
- package 스크립트는 `install/`, `manifests/`, 문서, 런처, 선택적 `swaptest/`만 복사한다.
- package 스크립트 끝에서는 항상 runtime-only preflight를 실행한다.
  - 호출 방식: `check_windows_runtime.ps1 -AppRoot <bundleRoot\\install> -SkipDataCheck`
- sample manifest와 sample data가 둘 다 번들 안에 있을 때만 추가로 full preflight를 한 번 더 실행한다.
  - 이 검사는 asset data root까지 확인하는 용도다.

### 2.4 문서 정리

- 대상 파일: `docs/extended_gaussian_windows_portable_bundle_ko.md`
- 문서 표현은 아래 원칙으로 통일한다.
  - install 단계가 런타임을 포함한다.
  - package 단계는 검증과 전달물 구성만 수행한다.
  - build script는 기본적으로 `build/`를 사용한다.
  - single-config build tree는 명시적으로 넘겼을 때만 허용되고, config 불일치 시 실패한다.
- `-IncludeSwaptestData`는 선택 기능으로 남기되, 데이터가 없을 때 bundle 생성 자체는 허용된다고 명확히 적는다.

## 3. 테스트 계획

### 3.1 build/config 검증

- `tools/windows/build_windows_portable_bundle.ps1`를 기본 인자로 실행했을 때 `build/` 기준으로 진행되는지 확인한다.
- `-BuildRoot build-ninja -Config RelWithDebInfo` 조합에서 즉시 실패하고 불일치 메시지가 나오는지 확인한다.
- `-BuildRoot build-ninja -Config Debug` 조합에서는 정상 진행 가능한지 확인한다.

### 3.2 preflight 검증

- `check_windows_runtime.ps1 -ManifestPath .\does-not-exist.json`이 `3`으로 실패하는지 확인한다.
- 형식이 깨진 JSON을 넘겼을 때 `3`으로 실패하는지 확인한다.
- `assets`가 없는 manifest를 넘겼을 때 `3`으로 실패하는지 확인한다.
- 유효한 manifest와 없는 `swaptest` 데이터 조합에서는 `2`로 실패하는지 확인한다.

### 3.3 packaging 검증

- `extended_gaussianViewer_app_install` 실행 시 `cudart64_12.dll`, `xatlas_d.dll` unresolved 경고가 사라졌는지 먼저 확인한다.
- package 스크립트 실행 전후로 `install/bin` 파일 목록이 바뀌지 않는지 확인한다.
- 필수 DLL이 빠진 `install/`을 인위적으로 만들면 package 단계의 preflight가 실패하는지 확인한다.
- `-IncludeSwaptestData` 없이도 번들은 생성되되, runtime-only preflight는 통과하는지 확인한다.
- `-IncludeSwaptestData`를 주고 sample data를 포함한 경우 full preflight까지 통과하는지 확인한다.

### 3.4 실행 동작 회귀 확인

- `run_installed_viewer.cmd`를 인자 없이 실행했을 때 기존과 동일하게 sample manifest 자동 부착 조건이 유지되는지 확인한다.
- `run_portable_bundle.cmd`도 동일한 자동 부착 조건을 유지하는지 확인한다.

## 4. 기본 가정과 선택한 기본값

- 리뷰 범위는 현재 브랜치 `develop/windows-portable-bundle`의 미커밋 변경으로 유지한다.
- Windows에서 재사용해야 할 기본 build tree는 저장소에 이미 존재하는 `build/`다.
- `build-ninja/`는 사용자가 의도적으로 선택했을 때만 허용한다.
- 번들 생성은 sample data 없이도 성공 가능해야 한다.
- preflight는 runtime 문제와 data 문제를 분리해 알려줘야 하며, manifest 자체가 잘못된 경우는 별도 실패 코드로 구분한다.
