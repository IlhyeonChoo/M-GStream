# extended_gaussian 수정 및 검증 기록

작성일: 2026-04-03

> 주의: 본 문서는 2026-04-03 시점 스냅샷에서 시작했으며, 아래에 후속 메모를 계속 덧붙인다. 현재 상태 판단은 코드와 `AGENTS.md`를 우선한다.

## 0. 2026-04-08 후속 메모

### 0.5 Portable bundle Turing+ 지원 범위 정합화

Windows portable bundle의 CUDA 지원 범위를 빌드, 패키징 검증, 런타임 가드, 사용자 문서에서 동일하게 맞췄다.

- `src/projects/extended_gaussian/renderer/CMakeLists.txt`
  - portable bundle 기본 CUDA 아키텍처를 `75-real;86-real;89-real;120`으로 조정했다.
  - 따라서 기본 지원 범위는 Turing(SM 7.5)부터 Blackwell(SM 12.0)까지다.
- `tools/windows/build_windows_portable_bundle.ps1`
  - portable bundle 검증 목록도 `75`, `86`, `89`, `120`으로 맞췄다.
  - 누락 시 재configure 예시도 같은 값으로 안내한다.
- `src/projects/extended_gaussian/renderer/subsystem/rendering_system/RenderingSystem.cpp`
  - 런타임 가드를 `SM 7.5+` 기준으로 바꾸고, 구형 GPU에서는 첫 커널 디스패치까지 가지 않도록 명확한 에러 메시지로 조기 종료하게 했다.
- `docs/extended_gaussian_windows_portable_bundle_ko.md`
  - direct build 예시, bundle 검증 설명, 재configure 힌트, 지원 GPU 범위를 모두 같은 기준으로 갱신했다.

### 0.4 Windows portable CUDA 아키텍처 고정

다른 Windows PC에서 portable bundle을 실행했을 때 `no kernel image is available for execution on the device`가 보고됐다.

이번 확인에서 핵심 원인은 build tree cache에 남아 있던 `CMAKE_CUDA_ARCHITECTURES=52`였다.
이 값으로 build된 CUDA 바이너리는 최신 GeForce RTX 50 계열 같은 새 GPU까지 고려한 portable bundle 기준으로는 너무 좁다.

이번 후속 수정은 아래 두 군데에 적용했다.

- `src/projects/extended_gaussian/renderer/CMakeLists.txt`
  - 프로젝트 전용 cache 변수 `EXTENDED_GAUSSIAN_CUDA_ARCHITECTURES`를 추가했다.
  - 기본값을 `86-real;89-real;90-real;120`으로 두고, `extended_gaussian` 본체와 `CudaRasterizer` 둘 다 이 값을 명시적으로 사용하도록 고정했다.
  - 따라서 generator별 `native` 해석이나 과거 cache 값에 덜 의존하게 됐다.
- `tools/windows/build_windows_portable_bundle.ps1`
  - viewer build 직후 build tree cache를 다시 읽어, portable bundle용 CUDA 아키텍처가 `86`, `89`, `90`, `120`을 모두 포함하는지 확인하도록 했다.
  - 누락 시 install/package 전에 즉시 실패시키고, 재configure 인자를 에러 메시지에 같이 출력하도록 했다.

관련 문서도 함께 업데이트했다.

- `docs/extended_gaussian_windows_portable_bundle_ko.md`
  - 새 Windows PC direct build 예시에 `-DEXTENDED_GAUSSIAN_CUDA_ARCHITECTURES=86-real;89-real;90-real;120`를 명시했다.
  - portable bundle 스크립트가 build 직후 CUDA 아키텍처를 검증한다는 점을 추가했다.

이번 수정은 아직 end-to-end CUDA rebuild까지 여기서 다시 돌리지는 않았다.
이유는 현재 저장소 규칙상 기존 build tree를 재사용하는 편을 우선했고, 실제 효과 검증에는 한 번의 reconfigure + rebuild가 필요하기 때문이다.

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

### 0.8 `develop/ubuntu24-remote-browser-stream` 선행 작업 추가

Ubuntu 24.04 remote browser stream 브랜치에서는 실제 headless renderer / HTTP / WebSocket 서버 구현에 들어가기 전에, 후속 브랜치가 공통으로 재사용할 수 있는 **저충돌 선행 작업**만 먼저 추가했다.

이번 선행 작업의 원칙은 다음과 같다.

- 기존 hot path 파일은 수정하지 않는다.
- 신규 파일만 추가한다.
- 목표는 기능 활성화가 아니라 **공용 계약 선점**과 **참조 자산 준비**다.

추가한 항목은 다음과 같다.

- `src/projects/extended_gaussian/renderer/server/ServerProtocol.hpp`
- `src/projects/extended_gaussian/renderer/server/ServerProtocol.cpp`
- `src/projects/extended_gaussian/renderer/server/CameraPoseAdapter.hpp`
- `src/projects/extended_gaussian/renderer/server/CameraPoseAdapter.cpp`
- `src/projects/extended_gaussian/renderer/server/www/index.html`
- `src/projects/extended_gaussian/renderer/server/www/app.js`
- `src/projects/extended_gaussian/renderer/server/www/styles.css`

구체적으로는 아래 내용을 고정했다.

- server mode 공용 옵션 구조
  - `ServerOptions`
  - 기본값
    - `listen_host=127.0.0.1`
    - `listen_port=8080`
    - `stream_width=1280`
    - `stream_height=720`
    - `stream_fps=15`
- browser control message 공용 계약
  - 메시지 타입은 현재 `set_camera_pose` 1종만 정의
  - 필드
    - `position`
    - `forward`
    - `up`
    - `fovy`
  - wire format 규칙
    - 벡터는 길이 3의 number array
    - `fovy` 단위는 radians
- `RemoteCameraPose <-> sibr::InputCamera` 변환 유틸
  - `forward/up` finite 검증
  - zero vector 금지
  - 평행 금지
  - 직교 기저 재구성 규칙 고정
- browser reference client
  - `/stream.mjpg` 미리보기
  - `/control` WebSocket 연결/해제
  - `set_camera_pose` payload 생성/전송

이번에 **의도적으로 하지 않은 것**은 다음과 같다.

- `main.cpp` server mode 진입 추가
- `ExtendedGaussianViewer` / `RenderingSystem` / `GaussianView` 연결
- EGL headless context
- 실제 HTTP / MJPEG / WebSocket 서버 구현
- install / resource packaging wiring

이 선행 작업은 후속 브랜치에서 다음 용도로 재사용한다.

- 실제 server runtime이 CLI를 읽을 때 `ServerOptions` 재사용
- WebSocket handler가 control payload를 해석할 때 `ParseControlMessageJson(...)` 재사용
- remote camera 제어를 `InputCamera`에 반영할 때 `TryBuildInputCamera(...)` 재사용
- 브라우저 초안 페이지를 실제 서버 정적 자산으로 승격할 때 `renderer/server/www/` 내용 재사용

검증 메모:

- 신규 C++ 파일은 `c++ -std=gnu++17 -fpermissive -fsyntax-only ...`로 문법 검증했다.
- 전체 `cmake -S . -B build-ninja -G Ninja` configure는 실패했지만,
  현재 남아 있는 partial configure 산출물만으로는 exact fatal line을 확정하지 않았다.
- 다만 보존된 증거상 `renderer` 서브프로젝트의 CUDA compiler identification 단계까지는 도달했고,
  최종 generator 파일 `build.ninja`는 생성되지 않았다.
- `plan/2026-04-08-ubuntu24-remote-browser-stream-prep.md`도 함께 작성했지만, 현재 저장소의 `.gitignore`에 `plan` 디렉터리가 포함되어 있어 기본 `git status`에는 나타나지 않는다. 따라서 작업 이력의 기준 문서는 본 `docs/extended_gaussian_modification_log_ko.md` 항목으로 본다.

### 0.9 remote browser stream 선행 작업 리뷰 반영

`docs/extended_gaussian_ubuntu24_remote_browser_stream_prep_review_ko.md`의 지적을 반영해,
선행 작업 계약을 다음과 같이 정리했다.

- `ParseControlMessageJson(...)`의 입력 타입에서 `std::string_view`를 제거했다.
  - 이유: 저장소의 Windows / Visual Studio 기본 경로는 여전히 C++14를 사용하므로,
    C++17 전용 타입을 공용 선행 모듈에 남기지 않기 위해서다.
- control parser에 trailing content 거부를 추가했다.
  - 이제 JSON object 뒤에 비공백 문자가 남아 있으면 parse 실패로 처리한다.
- camera pose validation의 문서 표현을 구현 기준에 맞췄다.
  - 기존 문서의 "평행 금지"를 "평행 또는 near-parallel 금지"로 수정했다.
- browser reference client에도 parser와 같은 최소 validation을 반영했다.
  - `0 < fovy < pi`
  - zero vector 금지
  - `forward` / `up`의 평행 또는 near-parallel 금지

추가 검증 메모:

- 신규 C++ 파일은 `c++ -std=gnu++14 -fpermissive -fsyntax-only ...`로도 다시 확인했다.
- 다만 전체 `cmake -S . -B build-ninja -G Ninja` configure는 여전히 실패했고,
  현재 남아 있는 산출물만으로는 exact fatal line을 확정하지 않았다.
- 보존된 증거상 `renderer` 서브프로젝트의 CUDA compiler identification 단계까지는 도달했고,
  최종 generator 파일 `build.ninja`는 생성되지 않았다.

### 1.0 remote browser stream follow-up 문서 / 샘플 추가

`docs/extended_gaussian_develop_ubuntu24_remote_browser_stream_followup_plan_ko.md`에서
"지금 브랜치에서 안전하게 선점 가능한 후속 작업"으로 분류한 항목 중,
실제 runtime 통합 없이 추가 가능한 문서와 샘플을 다음과 같이 정리했다.

추가한 항목:

- `docs/extended_gaussian_ubuntu24_toolchain_status_ko.md`
- `docs/extended_gaussian_remote_control_examples_ko.md`
- `docs/extended_gaussian_remote_browser_stream_manual_checklist_ko.md`
- `docs/extended_gaussian_remote_camera_contract_ko.md`
- `src/projects/extended_gaussian/renderer/server/examples/control_messages/valid_set_camera_pose_default.json`
- `src/projects/extended_gaussian/renderer/server/examples/control_messages/invalid_missing_fovy.json`
- `src/projects/extended_gaussian/renderer/server/examples/control_messages/invalid_position_length.json`
- `src/projects/extended_gaussian/renderer/server/examples/control_messages/invalid_fovy_out_of_range.json`
- `src/projects/extended_gaussian/renderer/server/examples/control_messages/invalid_forward_zero.json`
- `src/projects/extended_gaussian/renderer/server/examples/control_messages/invalid_forward_up_near_parallel.json`
- `src/projects/extended_gaussian/renderer/server/examples/control_messages/invalid_trailing_content.txt`
- `src/projects/extended_gaussian/renderer/server/www/README.md`

정리한 내용:

- partial `build-ninja/` configure 산출물 기준의 Ubuntu 24.04 CUDA toolchain stop point 기록
- `set_camera_pose` 정상 / 실패 payload 예제와 expected parser result 정리
- `/stream.mjpg` / `/control` 구현 이후 사용할 수동 검증 체크리스트 정리
- `position`, `forward`, `up`, `fovy` 의미와 `TryBuildInputCamera(...)`의 직교화 규칙 문서화
- `renderer/server/www/`가 최종 제품 UI가 아니라 protocol reference client라는 점을 별도 README로 고정

이번 follow-up 묶음의 원칙:

- 기존 hot path 파일은 수정하지 않는다.
- `main.cpp`, `ExtendedGaussianViewer`, `RenderingSystem`, `GaussianView`는 건드리지 않는다.
- 실제 HTTP / MJPEG / WebSocket 구현은 포함하지 않는다.
- 후속 브랜치가 바로 참조 가능한 기준 문서와 샘플만 추가한다.
### 0.2 Windows portable bundle 후속 수정 기록

이번 세션에서는 Windows portable bundle 관련 리뷰 문서와 후속 계획 문서를 기준으로, 실제 bundle 생성/검증 스크립트와 install 책임 경계를 정리했다.

기준 문서는 다음 두 개였다.

- `docs/extended_gaussian_windows_portable_bundle_review_ko.md`
- `plan/2026-04-08-windows-portable-bundle-review-followups.md`

핵심 수정 사항은 다음과 같다.

- `tools/windows/build_windows_portable_bundle.ps1`
  - 기본 `BuildRoot`를 `build/`로 변경했다.
  - single-config build tree를 명시적으로 넘겼을 때 `CMAKE_BUILD_TYPE`와 `-Config`가 다르면 즉시 실패하도록 했다.
  - package 단계에 요청 `Config`를 명시적으로 전달하도록 바꿨다.
- `src/projects/extended_gaussian/apps/extended_gaussianViewer/CMakeLists.txt`
  - Windows portable runtime 포함 책임을 package 단계가 아니라 install 단계로 이동시켰다.
  - `CUDA::cudart`와 `xatlas` runtime artifact를 install 시점에 직접 포함하도록 정리했다.
- `tools/windows/package_windows_portable_bundle.ps1`
  - `install/`을 수정하지 않고 bundle 복사 + preflight만 수행하도록 유지했다.
  - 요청 `Config`에 맞는 viewer executable만 선택하도록 config-aware packaging 로직을 추가했다.
  - 번들 루트에 `selected_viewer_exe.txt`를 기록해, 번들 실행 시 선택된 executable을 다시 사용할 수 있게 했다.
- `tools/windows/check_windows_runtime.ps1`
  - 사용자가 직접 넘긴 `-ManifestPath`가 없거나 형식이 잘못되면 `exit 3`으로 실패하도록 강화했다.
  - 선택된 viewer executable suffix에 맞춰 `_d` / `_rwdi` runtime DLL을 구분해서 검사하도록 바꿨다.
  - `xatlas*.dll`도 runtime 필수 항목으로 추가했다.
  - 현재 exit code 의미는 아래처럼 유지된다.
    - `0`: 통과
    - `1`: runtime 또는 GPU 문제
    - `2`: manifest가 가리키는 asset data root 누락
    - `3`: manifest 경로 또는 manifest 형식 문제
- `tools/windows/run_portable_bundle.cmd`
  - 번들 생성 시 기록된 `selected_viewer_exe.txt`를 먼저 읽고, 그 executable을 우선 실행하도록 수정했다.
- `docs/extended_gaussian_windows_portable_bundle_ko.md`
  - install 단계가 runtime DLL 포함의 단일 진실 원천이고, package 단계는 번들링 및 preflight만 수행한다는 점을 반영해 문서를 갱신했다.
  - `package_windows_portable_bundle.ps1 -Config ...` 사용 예시와 `selected_viewer_exe.txt` 동작도 추가로 기록했다.

이번 수정으로 해결한 문제는 다음과 같다.

- `build-ninja`처럼 single-config `Debug` build tree를 명시적으로 허용해도, stale `*_rwdi.exe`가 bundle에 들어가던 문제
- preflight가 `xatlas` 누락을 잡지 못하던 문제
- package 단계가 runtime 포함 책임까지 가져가면서 `install/`을 수정하던 설계 혼선
- bundle launcher가 실제 bundle 생성 시 선택된 config와 다른 executable을 집을 수 있던 문제

검증은 아래 항목까지 완료했다.

- 기본 인자 `tools/windows/build_windows_portable_bundle.ps1` 실행
  - `build/`를 사용해 `RelWithDebInfo` build -> install -> package -> runtime-only preflight가 통과했다.
  - bundle은 `extended_gaussianViewer_app_rwdi.exe`를 선택했다.
- `tools/windows/build_windows_portable_bundle.ps1 -BuildRoot .\build-ninja -Config Debug` 실행
  - single-config `Debug` build tree 기준으로 build -> install -> package -> runtime-only preflight가 통과했다.
  - bundle은 `extended_gaussianViewer_app_d.exe`를 선택했다.
- `tools/windows/check_windows_runtime.ps1`
  - 없는 manifest 경로는 `exit 3`
  - asset data root가 없는 manifest는 `exit 2`
  - `xatlas_rwdi.dll`을 제거한 임시 install 복사본은 `exit 1`
- `tools/windows/package_windows_portable_bundle.ps1`
  - package 실행 전후 `install/bin` 스냅샷을 비교해, package 단계가 `install/`을 수정하지 않는 것을 확인했다.
  - bundle root의 `selected_viewer_exe.txt`가 실제 선택된 executable 이름을 기록하는 것도 확인했다.

이번 변경은 아래 두 커밋으로 남겼다.

- `395a399` `fix: tighten windows portable bundle packaging`
- `8777379` `docs: update windows portable bundle guide`

남겨 둔 주의 사항은 다음과 같다.

- 이번 세션에서는 `Debug`와 `RelWithDebInfo` 경로를 실제로 검증했다.
- `Release` / `MinSizeRel`은 executable 선택 매핑은 넣었지만, end-to-end 실행 검증은 아직 하지 않았다.
- GUI viewer를 실제로 장시간 띄워 조작하는 수동 시나리오까지는 다시 수행하지 않았고, 이번 범위의 검증은 build/install/package/preflight 중심이다.

### 0.3 Windows portable bundle 리뷰 후속 2차 수정 기록

직전 portable bundle 정리 이후 추가 리뷰에서, package 단계의 GPU 의존성과 `MinSizeRel` executable naming 불일치가 새로 지적됐다.

이번 세션에서는 아래 두 문제를 먼저 문서로 정리한 뒤, 스크립트와 가이드를 같은 기준으로 수정했다.

- `docs/extended_gaussian_windows_portable_bundle_review_finding_response_ko.md`

핵심 수정 사항은 다음과 같다.

- `tools/windows/check_windows_runtime.ps1`
  - `-SkipGpuCheck` 옵션을 추가했다.
  - 기본 동작은 그대로 유지해, 사용자가 직접 preflight를 실행할 때는 계속 NVIDIA GPU를 검사한다.
  - package 단계처럼 artifact completeness만 보고 싶은 경우에만 GPU 검사를 명시적으로 건너뛸 수 있게 했다.
  - viewer executable 탐색 목록과 runtime suffix 판별에 `*_msr.exe` / `_msr`를 추가했다.
  - bundle 루트에 `selected_viewer_exe.txt`가 있으면, 수동 preflight도 그 executable을 우선 검사하도록 맞췄다.
- `tools/windows/package_windows_portable_bundle.ps1`
  - package-time preflight를 호출할 때 항상 `-SkipGpuCheck`를 넘기도록 바꿨다.
  - 따라서 GPU 없는 CI나 packaging host에서도 bundle 조립 자체는 가능해졌다.
  - `MinSizeRel` config를 `extended_gaussianViewer_app_msr.exe`로 올바르게 매핑했다.
  - executable fallback 후보 목록에도 `_msr`를 포함했다.
- `tools/windows/run_portable_bundle.cmd`
  - `selected_viewer_exe.txt`가 없을 때의 fallback 후보에 `extended_gaussianViewer_app_msr.exe`를 추가했다.
- `docs/extended_gaussian_windows_portable_bundle_ko.md`
  - package-time preflight는 GPU 검사를 건너뛴다는 점을 명시했다.
  - 최종 실행 대상 PC에서는 기본 preflight로 GPU까지 검증해야 한다는 점을 추가했다.
  - config별 viewer executable naming contract에 `MinSizeRel -> _msr`를 반영했다.

이번 수정으로 해결한 문제는 다음과 같다.

- GPU 없는 Windows 호스트에서 bundle 조립만 하려 해도 package가 실패하던 문제
- `-Config MinSizeRel` 사용 시 실제 `*_msr` 산출물 대신 release executable을 잘못 고를 수 있던 문제
- checker, package, launcher가 서로 다른 executable naming contract를 쓰던 문제

이번 범위에서 수행한 검증은 아래와 같다.

- `tools/windows/check_windows_runtime.ps1 -AppRoot .\install -SkipDataCheck`
  - 기존과 동일하게 통과하는지 확인
- `tools/windows/check_windows_runtime.ps1 -AppRoot .\install -SkipDataCheck -SkipGpuCheck`
  - GPU 검사 opt-out 경로가 통과하는지 확인
- `tools/windows/package_windows_portable_bundle.ps1 -Config RelWithDebInfo`
  - bundle 생성과 package-time preflight가 통과하는지 확인

남겨 둔 주의 사항은 다음과 같다.

- 이번 세션에서도 실제 end-to-end 수동 실행 검증은 `RelWithDebInfo` 기준 위주로 확인했다.
- `MinSizeRel` naming contract는 checker/package/launcher에서 바로잡았지만, 실제 `MinSizeRel` build 산출물로 end-to-end 실행까지 다시 검증한 것은 아니다.

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


## 7. 2026-04-12 M2 headless one-shot snapshot CLI

`develop/ubuntu24-remote-browser-stream` 브랜치에서 M2 범위의 최소 구현을 실제 app/runtime 경로에 연결했다.

이번 묶음에서 수정한 코드는 다음과 같다.

- `src/core/graphics/Window.cpp`
- `src/projects/extended_gaussian/apps/extended_gaussianViewer/main.cpp`
- `src/projects/extended_gaussian/renderer/ExtendedGaussianViewer.hpp`
- `src/projects/extended_gaussian/renderer/ExtendedGaussianViewer.cpp`

적용한 변경은 다음과 같다.

- `main.cpp`
  - `--headless`, `--manifest`, `--render-width`, `--render-height`, `--snapshot`, `--wait-for-streaming-idle`, `--max-headless-frames`를 추가했다.
  - 기존 interactive loop와 별도로 finite headless loop를 추가했다.
  - headless mode에서는 `offscreen`, `nogui`, `vsync=0`을 강제하고 size-based `Window` ctor를 사용한다.
- `ExtendedGaussianViewer`
  - GUI가 비활성 상태일 때 top-level ImGui 코드가 실행되지 않도록 guard를 추가했다.
  - raw model directory를 직접 load해서 scene instance를 만드는 helper를 추가했다.
  - `"Gaussian View"` snapshot 저장 helper를 추가했다.
  - manifest streaming idle 판정 helper를 추가했다.
  - manifest bounds focus 경로를 일반 bounds helper로 분리했다.
- `Window.cpp`
  - `GLFW_PLATFORM_NULL`이 존재하는 빌드에서는 `offscreen` 초기화 시 null platform hint를 먼저 주도록 보강했다.

이번 수정으로 닫은 범위는 다음과 같다.

- CLI에서 finite offscreen render 실행
- manifest 또는 raw model directory 입력 허용
- PNG snapshot 저장
- manifest streaming queue drain 대기 옵션 제공
- help 출력에 M2 전용 플래그 노출

당시 기준 미완료 항목은 다음이었고, 아래 section 8에서 해소됐다.

- true surfaceless EGL 보장
- no-display 환경 end-to-end snapshot smoke 완료
- HTTP / MJPEG / WebSocket server runtime 연결

검증 메모:

- `cmake --build build-ninja-m2 --target extended_gaussianViewer_app --parallel` 통과
- `cmake --install build-ninja-m2` 통과
- `./install/bin/extended_gaussianViewer_app --help` 실행 통과
- `--help`에서 새 M2 플래그 노출 확인
- Ubuntu Desktop에서 이 브랜치의 재빌드와 실행이 가능하다는 사용자 확인이 있었다.


## 8. 2026-04-12 M2 direct EGL completion

이후 마무리 작업에서 M2의 남은 no-display blocker를 `Window` 레벨에서 정리했다.

추가 수정 파일:

- `src/core/graphics/Window.hpp`
- `src/core/graphics/Window.cpp`
- `src/core/graphics/CMakeLists.txt`
- `src/projects/extended_gaussian/apps/extended_gaussianViewer/main.cpp`

적용한 변경:

- `Window` 에 direct EGL pbuffer context 경로를 추가했다.
- `DISPLAY/WAYLAND_DISPLAY` 가 없는 `offscreen` 실행은 GLFW 대신 direct EGL backend를 사용한다.
- `sibr_graphics` target에 `EGL_FOUND` 시 `libEGL` 링크를 추가했다.
- `--headless` 는 `--snapshot` 을 필수로 검증하고, `--path` 없이 empty snapshot smoke 를 허용한다.
- 일반 `--offscreen` 도 size-based ctor를 사용하도록 app entry를 정리했다.

검증 결과:

- no-display empty snapshot smoke: `PASS`
  - `/tmp/extended_gaussian_empty.png`, `64x64`, `173` bytes
- no-display Gaussian snapshot smoke: `PASS`
  - `/tmp/extended_gaussian_bonsai.png`, `640x360`, `386837` bytes
  - model: `../gaussian-splatting/eval/bonsai`
  - runtime log: `GaussianView CUDA/GL interop enabled.`
- no-display `--offscreen --nogui` probe: `PASS`
  - direct EGL 초기화 후 timeout `124`; 기존 `DISPLAY` crash 제거
- Ubuntu Desktop GUI rebuild/run: 사용자 확인 `PASS`

이 시점 기준으로 M2 구현 이슈는 닫고 M3로 넘어갈 수 있다.

## 9. 2026-04-12 M3 server build surface 분리 준비

M3의 목적은 server runtime 을 붙이는 것이 아니라, server 관련 소스의 build surface 와 dependency policy 를 먼저 고정하는 것이다.

이번 단계에서 기록할 구현 내용은 다음과 같다.

- `src/projects/extended_gaussian/CMakeLists.txt`
  - `SIBR_BUILD_REMOTE_STREAM` option 추가
  - Linux 기본값 `ON`, Windows 기본값 `OFF`
- `src/projects/extended_gaussian/renderer/CMakeLists.txt`
  - `renderer/server` subtree 를 `extended_gaussian` 의 암묵 `GLOB_RECURSE` 소스 집합에서 제외
  - 조건부로 `renderer/server` 하위 디렉터리를 별도 target 으로 추가
- `src/projects/extended_gaussian/renderer/server/CMakeLists.txt`
  - `extended_gaussian_server` static target 추가
  - HTTP / WebSocket backend 를 `Boost.Beast` 로 고정
  - `TurboJPEG` 는 probe / summary 만 추가하고 hard requirement 는 M5 로 deferred
- `src/projects/extended_gaussian/renderer/server/Config.hpp`
  - server 전용 export macro 분리
- `src/projects/extended_gaussian/renderer/server/ServerProtocol.hpp`
- `src/projects/extended_gaussian/renderer/server/CameraPoseAdapter.hpp`
  - server 전용 export macro 로 전환

build tree 운영 메모:

- canonical build path 는 다시 `build-ninja/` 로 사용한다.
- `build-ninja-m2/` 는 더 이상 기준 경로로 사용하지 않는다.

M3 intended validation command:

```bash
cmake -S . -B build-ninja -G Ninja -DSIBR_BUILD_REMOTE_STREAM=ON
cmake --build build-ninja --target extended_gaussian_server --parallel
cmake --build build-ninja --target extended_gaussian --parallel
cmake --build build-ninja --target extended_gaussianViewer_app --parallel
```

주의:

- 이 section 은 구현 내용과 검증 계획만 기록한다.
- 실제 build / install / runtime 결과는 검증이 끝난 뒤 별도 section 에 append 한다.

## 10. 2026-04-12 M3 server build surface completion

M3에서 `renderer/server` 관련 코드를 runtime feature 추가 없이 명시적인 build target 으로 분리했다.

수정 파일:

- `src/projects/extended_gaussian/CMakeLists.txt`
- `src/projects/extended_gaussian/renderer/CMakeLists.txt`
- `src/projects/extended_gaussian/renderer/server/CMakeLists.txt`
- `src/projects/extended_gaussian/renderer/server/Config.hpp`
- `src/projects/extended_gaussian/renderer/server/ServerProtocol.hpp`
- `src/projects/extended_gaussian/renderer/server/CameraPoseAdapter.hpp`

적용한 변경:

- top-level project 에 `SIBR_BUILD_REMOTE_STREAM` option 을 추가했다.
  - Linux 기본값 `ON`
  - Windows 기본값 `OFF`
- `extended_gaussian` shared library 의 `GLOB_RECURSE` 결과에서 `renderer/server/*` 를 제거했다.
- `SIBR_BUILD_REMOTE_STREAM=ON` 일 때만 `renderer/server` 하위 디렉터리를 추가한다.
- `extended_gaussian_server` 정적 라이브러리를 새로 만들었다.
- `renderer/server/Config.hpp` 를 추가해 `SIBR_EXTENDED_GAUSSIAN_SERVER_EXPORT` macro 를 분리했다.
- `ServerProtocol.hpp`, `CameraPoseAdapter.hpp` 는 server 전용 export macro 를 사용하도록 바꿨다.
- `Boost.Beast` 헤더 존재를 configure 시점에서 검증하도록 추가했다.
- `TurboJPEG` 는 probe / summary 만 수행하고, hard requirement 는 M5 로 deferred 했다.
- canonical build path 를 `build-ninja/` 로 되돌렸다.

검증 결과:

- `cmake -S . -B build-ninja -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CUDA_COMPILER=/usr/local/cuda-12.8/bin/nvcc -DSIBR_BUILD_REMOTE_STREAM=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON` 통과
- configure summary 에서 다음 확인
  - `extended_gaussian remote stream modules: ON`
  - `extended_gaussian_server JSON backend: picojson`
  - `extended_gaussian_server HTTP/WebSocket backend: Boost.Beast`
  - `extended_gaussian_server JPEG backend: TurboJPEG not found...`
- `cmake --build build-ninja --target extended_gaussian_server --parallel` 통과
- `cmake --build build-ninja --target extended_gaussian --parallel` 통과
- `cmake --build build-ninja --target extended_gaussianViewer_app --parallel` 통과
- `cmake --build build-ninja --target install --parallel` 통과
- `./install/bin/extended_gaussianViewer_app --help` 실행 통과
- `build-ninja/build.ninja`, `compile_commands.json` 에서 `ServerProtocol.cpp`, `CameraPoseAdapter.cpp` 가 `extended_gaussian_server` 에만 속하는 것 확인
- `ar -t`, `nm -C` 로 server 정적 라이브러리 산출과 핵심 심볼 존재 확인
- install 결과에는 새 server runtime binary / `www` asset 이 추가되지 않음

이 시점 기준 M3는 build graph / dependency policy / source ownership 정리 범위에서 닫고, runtime 연결은 M4 이후로 넘긴다.


## 11. 2026-04-13 M4 HTTP skeleton completion

M4에서 `extended_gaussianViewer_app` 와 `extended_gaussian_server` 를 실제 runtime 으로 연결하고, no-display viewer 위에서 동작하는 HTTP skeleton 을 구현했다.

수정 파일:

- `src/projects/extended_gaussian/CMakeLists.txt`
- `src/projects/extended_gaussian/apps/extended_gaussianViewer/CMakeLists.txt`
- `src/projects/extended_gaussian/apps/extended_gaussianViewer/main.cpp`
- `src/projects/extended_gaussian/renderer/ExtendedGaussianViewer.hpp`
- `src/projects/extended_gaussian/renderer/ExtendedGaussianViewer.cpp`
- `src/projects/extended_gaussian/renderer/server/CMakeLists.txt`
- `src/projects/extended_gaussian/renderer/server/ServerProtocol.hpp`
- `src/projects/extended_gaussian/renderer/server/ServerProtocol.cpp`
- `src/projects/extended_gaussian/renderer/server/RemoteStreamServer.hpp`
- `src/projects/extended_gaussian/renderer/server/RemoteStreamServer.cpp`

적용한 변경:

- `main.cpp` 에 remote-stream CLI surface 와 process signal handling 을 추가했다.
- `main.cpp` 가 `RemoteStreamServer` 의 생성 / start / per-frame health publish / stop 을 직접 관리하도록 연결했다.
- `--server --headless` 조합은 거부하고, 장기 실행 server 는 `--offscreen --nogui --server` 조합만 허용했다.
- `ServerProtocol` 에 `--bind`, `--port`, `--www-root` 정규화를 추가했다.
- `RemoteStreamServer` 를 신규 추가해 다음을 구현했다.
  - dedicated server thread
  - non-blocking accept loop
  - `/healthz`
  - `/`, `/index.html`, `/app.js`, `/styles.css`, `/static/*`
  - `/stream.mjpg` placeholder 501
  - `/control` placeholder 426 + `Upgrade: websocket`
  - percent-decoding / path traversal 방어
- `ExtendedGaussianViewer` 에 health snapshot 용 getter 를 추가했다.
- `src/projects/extended_gaussian/CMakeLists.txt` 에서 `renderer` 를 `apps` 보다 먼저 add 하도록 순서를 바꿨다.
  - 이유: app CMake 가 `extended_gaussian_server` target 을 볼 수 있어야 app link 와 compile definition 이 적용된다.
- `apps/extended_gaussianViewer/CMakeLists.txt` 에서 app 이 `extended_gaussian_server` 를 link 하고 `SIBR_EXTENDED_GAUSSIAN_REMOTE_STREAM_BUILD=1` compile definition 을 받도록 수정했다.
- `www` install destination 을 `resources/extended_gaussian/server/www` 로 맞췄고, runtime lookup 도 같은 install path 를 우선 탐색하게 했다.

검증 결과:

- `cmake -S . -B build-ninja -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CUDA_COMPILER=/usr/local/cuda-12.8/bin/nvcc -DSIBR_BUILD_REMOTE_STREAM=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON` 통과
- `cmake --build build-ninja --target extended_gaussianViewer_app --parallel` 통과
- `cmake --build build-ninja --target install --parallel` 통과
- `./install/bin/extended_gaussianViewer_app --help` 에 server 관련 옵션 노출 확인
- `./install/bin/extended_gaussianViewer_app --offscreen --nogui --server --listen-host 127.0.0.1 --listen-port 18080 --width 640 --height 360` 기동 통과
  - startup log 에 `Initialization of direct headless EGL`
  - startup log 에 `RemoteStreamServer listening on 127.0.0.1:18080`
  - resolved `www root` 가 `install/resources/extended_gaussian/server/www` 로 잡힘
- `curl` smoke
  - `/healthz`: `200 OK`
  - `/`: `200 OK`
  - `/app.js`: `200 OK`
  - `/styles.css`: `200 OK`
  - `/static/app.js`: `200 OK`
  - `/stream.mjpg`: `501 Not Implemented`
  - `/control`: `426 Upgrade Required`
- `Ctrl+C` 종료 후 `RemoteStreamServer stop elapsed: ... sec` 로그 및 exit code `0` 확인

이 시점 기준 M4는 HTTP skeleton / static asset serving / process lifecycle glue 범위에서 닫고, 실제 MJPEG frame delivery 와 WebSocket control session 은 M5/M6 로 넘긴다.
