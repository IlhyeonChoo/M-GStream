# MGStream Windows Portable Bundle Review Notes

작성일: 2026-04-08
대상 브랜치: `develop/windows-portable-bundle`

## 리뷰 범위

- 현재 작업 트리의 `Windows portable bundle` 관련 미커밋 변경만 검토했다.
- 렌더러/씬 로직과 다른 주제의 변경은 이번 리뷰 범위에서 제외했다.

## 검토 파일 목록

- `src/projects/M_GStream/apps/M_GStreamViewer/CMakeLists.txt`
- `tools/windows/build_windows_portable_bundle.ps1`
- `tools/windows/package_windows_portable_bundle.ps1`
- `tools/windows/check_windows_runtime.ps1`
- `tools/windows/run_installed_viewer.cmd`
- `tools/windows/run_portable_bundle.cmd`
- `docs/M_GStream_windows_portable_bundle_ko.md`

## 핵심 총평

이번 변경은 Windows 배포 흐름을 한 번에 다루려는 방향 자체는 좋다. 특히 설치 후 실행 스크립트, 번들 루트 실행 스크립트, preflight 스크립트를 같이 제공한 점은 사용성 측면에서 분명한 개선이다.

다만 현재 상태에서는 "어떤 build tree와 config를 기준으로 번들을 만드는가", "manifest 검증이 정말로 수행되었는가", "런타임 포함 책임이 install인지 package인지"가 각각 흔들린다. 이 세 지점은 번들이 우연히 되는 환경에서는 지나가지만, 다른 PC로 넘겼을 때 재현성이 무너지는 종류의 문제라 우선순위를 높게 두는 편이 맞다.

`run_installed_viewer.cmd`와 `run_portable_bundle.cmd`의 기본 manifest 자동 선택 로직은 이번 범위에서 별도 문제를 만들지는 않았다. 이번 리뷰의 핵심 이슈는 빌드/검증/패키징 경계에 집중된다.

## 우선순위별 상세 피드백

### 1. [P1] 기본 build tree와 config 가정이 충돌한다

#### 문제 위치

- `tools/windows/build_windows_portable_bundle.ps1:3-4`
- `tools/windows/build_windows_portable_bundle.ps1:71-80`

#### 아쉬운 점

스크립트 기본값은 `BuildRoot=build-ninja`, `Config=RelWithDebInfo`인데, 현재 워크스페이스의 `build-ninja/CMakeCache.txt`는 single-config `Ninja` + `CMAKE_BUILD_TYPE=Debug`다. 즉 스크립트 인자만 보면 `RelWithDebInfo` 기준 번들을 만드는 것처럼 보이지만, 실제로는 기존 `Debug` build tree를 그대로 사용할 가능성이 높다.

#### 왜 아쉬운지

이 스크립트의 목적은 "원클릭으로 재현 가능한 portable bundle 생성"인데, 현재 구현은 로컬에 이미 존재하는 build tree 상태에 결과가 종속된다. 특히 single-config generator에서는 `--config RelWithDebInfo`가 의도한 보호장치가 되지 못하므로, 문서가 권장하는 `*_rwdi.exe` 기준 흐름과 실제 산출물이 쉽게 어긋난다.

이 문제는 사용자가 실수했을 때만 드러나는 수준이 아니라, 기본값 자체가 현재 저장소 상태와 충돌한다는 점이 더 아쉽다. 번들이 생성되더라도 `_d` 실행 파일 중심 결과가 나오면, 이후 문서/검증/배포 설명 전체가 흔들린다.

#### 수정 방향

- 기본 `BuildRoot`를 `build/`로 바꿔 multi-config Visual Studio build tree를 우선 사용한다.
- 사용자가 `-BuildRoot`로 single-config tree를 명시적으로 넘긴 경우에는 `CMakeCache.txt`를 읽어 `CMAKE_BUILD_TYPE`와 `-Config` 값을 비교한다.
- single-config tree에서 둘이 다르면 조용히 진행하지 말고 즉시 실패시켜야 한다.
- 오류 메시지에는 현재 generator, cache build type, 요청 config를 모두 출력해서 사용자가 바로 수정할 수 있게 한다.

### 2. [P1] preflight가 사용자가 지정한 manifest를 실제로 검증하지 않을 수 있다

#### 문제 위치

- `tools/windows/check_windows_runtime.ps1:2-4`
- `tools/windows/check_windows_runtime.ps1:41-67`
- `tools/windows/check_windows_runtime.ps1:122-175`

#### 아쉬운 점

사용자가 `-ManifestPath`를 직접 넘겨도, 해당 파일이 없으면 즉시 실패하지 않는다. 현재 코드는 경로를 절대경로로만 바꾼 뒤, 나중 단계에서 `Test-Path`가 실패하면 asset 검증 블록 자체를 건너뛴다. 그 결과 runtime DLL과 GPU만 맞으면 preflight가 성공으로 끝날 수 있다.

#### 왜 아쉬운지

이 스크립트는 번들 전달 전에 "정말 실행 가능한가"를 확인하는 마지막 안전망 역할을 해야 한다. 그런데 사용자가 의도적으로 검증 대상을 지정했는데도 검증이 생략될 수 있으면, 가장 위험한 종류의 false positive가 된다. 특히 문서에서 이 스크립트를 권장 검증 순서의 첫 단계로 올려둔 상태라, 여기서 성공이 나오면 사용자는 manifest와 데이터 경로까지 확인됐다고 믿게 된다.

또한 현재 구현은 JSON 파싱 실패나 `assets` 누락을 구조적으로 구분하지 않는다. 실행 전 검증 스크립트라면 "runtime 문제", "데이터 누락", "manifest 자체가 잘못됨"을 최소한 별도 실패로 나눠 주는 편이 운영상 훨씬 낫다.

#### 수정 방향

- 사용자가 `-ManifestPath`를 직접 넘겼다면 존재 여부를 즉시 검사하고, 없으면 명확한 오류와 함께 종료한다.
- `ConvertFrom-Json`은 예외를 포착해 manifest 형식 오류로 분류한다.
- `assets`가 없거나 객체 형태가 아니면 역시 manifest 형식 오류로 처리한다.
- 종료 코드는 기존 runtime 실패와 asset missing을 유지하되, 잘못된 manifest 경로/스키마용 코드를 별도로 두는 편이 좋다.

### 3. [P2] 런타임 포함 책임이 install과 package에 중복되어 있다

#### 문제 위치

- `src/projects/M_GStream/apps/M_GStreamViewer/CMakeLists.txt:12-45`
- `src/projects/M_GStream/apps/M_GStreamViewer/CMakeLists.txt:72-79`
- `src/projects/M_GStream/apps/M_GStreamViewer/CMakeLists.txt:96-97`
- `tools/windows/package_windows_portable_bundle.ps1:18-65`
- `tools/windows/package_windows_portable_bundle.ps1:132-166`
- `docs/M_GStream_windows_portable_bundle_ko.md:95-100`
- `docs/M_GStream_windows_portable_bundle_ko.md:144-147`

#### 아쉬운 점

현재 구현은 CMake install 단계에서도 `CUDA::cudart`, `xatlas` 경로를 수집하고, package 스크립트에서도 다시 `cudart64_*.dll`, `xatlas*.dll`를 직접 복사한다. package 스크립트는 번들을 만들기 전에 `install/bin` 자체를 수정하므로, 설치 산출물이 원래 완전했는지 아닌지를 흐리게 만든다.

#### 왜 아쉬운지

portable bundle에서 가장 중요한 것은 "어느 단계가 완성 책임을 지는가"가 분명해야 한다는 점이다. 지금처럼 install과 package가 둘 다 런타임을 보강하면, 설치가 깨져 있어도 packaging이 나중에 덮어써서 결과만 맞출 수 있다. 이런 구조는 당장은 편하지만, 시간이 지나면 두 경로가 서로 다른 DLL 집합을 복사하게 되고 어느 쪽이 정답인지 설명하기 어려워진다.

문서도 현재는 package 단계가 런타임을 보강한다고 설명하고 있어, CMake install 쪽 변경과 책임 경계를 더 흐리게 만든다. 결국 설치 실패를 packaging이 가리는 구조는 디버깅과 유지보수 모두에 좋지 않다.

#### 수정 방향

- 이 정리는 바로 package 쪽 backfill부터 지우는 방식으로 들어가면 안 된다.
- 먼저 `M_GStreamViewer_app_install` 단계에서 현재 남아 있는 `cudart64_12.dll`, `xatlas_d.dll` unresolved 경고를 실제로 없애야 한다.
- 그 전에는 package의 backfill이 install 불완전 상태를 가리는 안전망 역할도 하고 있으므로, install 쪽이 완성되기 전에 제거하면 다시 배포가 깨질 수 있다.
- 런타임 포함의 단일 진실 원천은 CMake install 단계로 고정한다.
- `package_windows_portable_bundle.ps1`의 수동 DLL backfill 로직은 제거한다.
- package 스크립트는 기존 `install/`을 수정하지 않고 복사와 번들링만 수행해야 한다.
- 대신 package/build 스크립트 끝에서 `check_windows_runtime.ps1`를 호출해 번들 완성도를 검증하도록 바꾸는 편이 안전하다.
- 문서 설명도 "package가 런타임을 보강한다"가 아니라 "install이 런타임을 포함하고 package는 검증 및 전달물 구성만 한다"로 정리해야 한다.

## 권장 수정 순서

1. `build_windows_portable_bundle.ps1`의 기본 build tree와 config 검증을 먼저 바로잡는다.
2. `check_windows_runtime.ps1`에서 explicit manifest 검증과 manifest 형식 오류 처리를 추가한다.
3. 먼저 `M_GStreamViewer_app_install`의 `cudart64_12.dll`, `xatlas_d.dll` unresolved 경고를 실제로 해소한다.
4. 그 다음 `package_windows_portable_bundle.ps1`의 수동 DLL backfill을 제거하고, 검증 호출 기반 구조로 바꾼다.
5. 마지막으로 `M_GStream_windows_portable_bundle_ko.md`를 실제 책임 경계에 맞게 다시 정리한다.

## 결론

이번 변경은 방향성은 맞지만, 현재 상태로는 "운 좋게 되는 번들링"에 더 가깝다. build tree 선택, preflight 의미, runtime 책임 경계를 먼저 고정하면 그 다음부터는 Windows 배포 플로우가 훨씬 예측 가능해질 것이다.
