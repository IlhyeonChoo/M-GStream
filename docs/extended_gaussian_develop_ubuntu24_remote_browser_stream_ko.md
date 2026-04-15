# `develop/ubuntu24-remote-browser-stream` 브랜치 작업 기록

작성일: 2026-04-08  
기준 브랜치: `develop/ubuntu24-remote-browser-stream`

## 1. 문서 목적

이 문서는 현재 브랜치에서 진행한 변경만 별도로 모아두기 위한 브랜치 전용 기록이다.

기존 `docs/extended_gaussian_modification_log_ko.md`가 저장소 전반의 수정 이력을 누적하는 문서라면,
이 문서는 `develop/ubuntu24-remote-browser-stream` 브랜치에서 직접 추가하거나 수정한 내용만 정리한다.

정리 대상은 다음 두 범주다.

- remote browser stream 구현 전에 먼저 빼둘 수 있는 선행 작업
- 그 선행 작업에 대해 리뷰 문서에서 지적한 사항을 반영한 후속 수정
- 실제 runtime 통합 없이 안전하게 추가한 follow-up 문서 / 샘플 묶음

## 2. 브랜치 목표

이 브랜치의 본래 목표는 Ubuntu 24.04 Server에서 headless renderer 결과를 브라우저로 스트리밍하는 기능을 준비하는 것이다.

다만 현재 시점의 실제 변경은 전체 server mode 구현이 아니라, 후속 브랜치가 공통으로 재사용할 수 있는 계약과 참조 자산을 먼저 고정하는 데 집중되어 있다.

즉, 이번 브랜치에서 먼저 처리한 범위는 다음과 같다.

- server mode 공용 옵션 구조 정의
- browser control message wire contract 정의
- remote camera pose와 `sibr::InputCamera` 사이 변환 규칙 정의
- 브라우저 참조용 정적 클라이언트 초안 추가
- 선행 작업에 대한 코드리뷰 지적 반영
- follow-up용 툴체인 상태 문서, control payload 샘플, 수동 체크리스트, camera contract 문서 추가

반대로 아직 하지 않은 것은 다음과 같다.

- 실제 HTTP server 구현
- 실제 MJPEG 스트리밍 구현
- 실제 WebSocket server 구현
- headless EGL context 연결
- `main.cpp`, `ExtendedGaussianViewer`, `RenderingSystem`, `GaussianView`와의 런타임 연결

## 3. 이번 브랜치에서 추가된 핵심 파일

### 3.1 server 공용 계약 / 유틸

- `src/projects/extended_gaussian/renderer/server/ServerProtocol.hpp`
- `src/projects/extended_gaussian/renderer/server/ServerProtocol.cpp`
- `src/projects/extended_gaussian/renderer/server/CameraPoseAdapter.hpp`
- `src/projects/extended_gaussian/renderer/server/CameraPoseAdapter.cpp`

이 파일들은 아직 실제 server runtime에 연결되지는 않았지만,
후속 브랜치가 바로 가져다 쓸 수 있는 공용 타입과 parser/adapter를 제공한다.

정의된 핵심 요소는 다음과 같다.

- `ServerOptions`
  - `enabled`
  - `listen_host`
  - `listen_port`
  - `stream_width`
  - `stream_height`
  - `stream_fps`
- `RemoteCameraPose`
  - `position`
  - `forward`
  - `up`
  - `fovy`
- `ControlMessageType`
  - 현재는 `SetCameraPose`만 정의
- `ControlMessage`
- `ParseControlMessageResult`
- `ParseServerOptions(...)`
- `ParseControlMessageJson(...)`
- `ValidateRemoteCameraPose(...)`
- `TryBuildInputCamera(...)`
- `ExportRemoteCameraPose(...)`

### 3.2 browser reference client

- `src/projects/extended_gaussian/renderer/server/www/index.html`
- `src/projects/extended_gaussian/renderer/server/www/app.js`
- `src/projects/extended_gaussian/renderer/server/www/styles.css`

이 정적 자산은 최종 제품 UI가 아니라 protocol reference 용도다.

현재 기준 기능은 다음과 같다.

- `/stream.mjpg` 미리보기 URL 생성
- `/control` WebSocket URL 생성
- `set_camera_pose` JSON payload 생성
- WebSocket 연결/해제
- payload 수동 전송
- parser와 같은 최소 validation 적용

### 3.3 follow-up 문서 / 샘플 묶음

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

이 묶음은 실제 server runtime을 추가하지 않고도
후속 브랜치가 바로 참조할 수 있는 기준 문서와 테스트 벡터를 늘리기 위한 것이다.

정리한 내용은 다음과 같다.

- partial `build-ninja/` 산출물 기준 Ubuntu 24.04 toolchain 상태 기록
- `set_camera_pose` 정상 / 실패 예제 payload 정리
- `/stream.mjpg` / `/control` 구현 이후 사용할 수동 검증 체크리스트 정리
- remote camera payload의 의미 계약과 직교화 규칙 문서화
- `renderer/server/www/`가 shipped UI가 아니라 protocol reference client라는 점 명시

## 4. 고정한 계약

### 4.1 server option 기본값

기본값은 다음으로 고정했다.

- `listen_host=127.0.0.1`
- `listen_port=8080`
- `stream_width=1280`
- `stream_height=720`
- `stream_fps=15`

보안과 범위 측면에서 초기 기본값은 loopback 기준으로 유지한다.

### 4.2 control message wire contract

현재 브랜치에서는 control message를 1종만 정의했다.

- `type = "set_camera_pose"`

JSON shape는 다음과 같다.

```json
{
  "type": "set_camera_pose",
  "position": [0.0, 0.0, 0.0],
  "forward": [0.0, 0.0, -1.0],
  "up": [0.0, 1.0, 0.0],
  "fovy": 0.78539816339
}
```

계약 규칙은 다음으로 고정했다.

- `position`, `forward`, `up`는 길이 3의 숫자 배열
- `fovy`는 radians
- `0 < fovy < pi`
- `forward`, `up`는 zero vector 금지
- `forward`, `up`는 평행 또는 near-parallel 금지
- JSON object 뒤의 trailing content는 허용하지 않음

### 4.3 camera pose adapter 규칙

`RemoteCameraPose`를 `InputCamera`에 반영할 때는 다음 규칙을 사용한다.

- `forward`, `up`를 정규화한다.
- `right = normalize(cross(forward, up))`
- `corrected_up = normalize(cross(right, forward))`
- 현재 camera의 `aspect`, `znear`, `zfar`가 유효하면 유지한다.
- 기존 값이 유효하지 않으면 fallback을 사용한다.
  - `aspect = 1.0`
  - `znear = 0.1`
  - `zfar = 1000.0`

## 5. 리뷰 반영 사항

이 브랜치에는 별도 리뷰 문서가 존재한다.

- `docs/extended_gaussian_ubuntu24_remote_browser_stream_prep_review_ko.md`

해당 리뷰에서 지적된 사항 중 이번 브랜치에서 이미 반영한 것은 다음과 같다.

### 5.1 C++14 호환성 복구

초기 버전은 `ParseControlMessageJson(std::string_view)`를 사용하고 있었는데,
저장소의 Windows / Visual Studio 기본 경로는 여전히 C++14를 사용하므로 이 상태로는 안전하지 않았다.

따라서 현재는 다음과 같이 수정했다.

- `std::string_view` 제거
- `ParseControlMessageJson(const std::string&)`로 통일

### 5.2 parser trailing garbage 거부

초기 parser는 JSON object 뒤에 남는 비공백 문자를 검사하지 않았다.

현재는 parse 이후 stream을 다시 확인해서,
뒤에 trailing content가 남아 있으면 parse 실패로 처리한다.

### 5.3 문서와 구현의 near-parallel 정책 통일

초기 계획 문서에는 `forward` / `up`에 대해 "평행 금지"만 적혀 있었지만,
구현은 `abs(dot) >= 0.999` 기준으로 near-parallel도 거부하고 있었다.

현재는 문서 표현을 구현 기준에 맞춰 다음으로 정리했다.

- `forward`, `up`는 평행 또는 near-parallel 금지

### 5.4 browser reference client validation 보강

초기 browser reference client는 단순한 JSON formatter 수준에 가까웠다.

현재는 JS 쪽에도 parser와 같은 최소 validation을 넣었다.

- `0 < fovy < pi`
- zero vector 금지
- `forward` / `up`의 평행 또는 near-parallel 금지

## 6. 검증 메모

현재 브랜치에서 확인한 내용은 다음과 같다.

- `ServerProtocol.cpp`
  - `c++ -std=gnu++14 -fpermissive -fsyntax-only ...` 통과
- `CameraPoseAdapter.cpp`
  - `c++ -std=gnu++14 -fpermissive -fsyntax-only ...` 통과
- `app.js`
  - `node --check ...` 통과

주의할 점:

- 전체 `cmake -S . -B build-ninja -G Ninja` configure는 여전히 실패한다.
- 현재 남아 있는 configure 산출물만으로는 exact fatal line을 확정할 수 없다.
- 다만 보존된 증거상 `renderer` 서브프로젝트의 CUDA compiler identification 단계까지는 도달했고,
  최종 generator 파일 `build.ninja`는 생성되지 않았다.
- 따라서 "브랜치 전용 선행 작업 코드의 문법/정적 계약 검증"까지는 확인했지만, 전체 타깃 빌드 성공은 아직 별도 환경 정리가 필요하다.

## 7. 현재 브랜치에서 바로 재사용 가능한 산출물

후속 브랜치는 이 브랜치의 산출물을 다음 방식으로 재사용하면 된다.

- server runtime
  - `ParseServerOptions(...)`
- WebSocket control handler
  - `ParseControlMessageJson(...)`
- remote camera 적용 경로
  - `TryBuildInputCamera(...)`
- 현재 카메라를 control payload로 내보내는 경로
  - `ExportRemoteCameraPose(...)`
- 브라우저 정적 클라이언트 초안
  - `renderer/server/www/*`
- follow-up 문서 / 샘플
  - `docs/extended_gaussian_ubuntu24_toolchain_status_ko.md`
  - `docs/extended_gaussian_remote_control_examples_ko.md`
  - `docs/extended_gaussian_remote_browser_stream_manual_checklist_ko.md`
  - `docs/extended_gaussian_remote_camera_contract_ko.md`
  - `renderer/server/examples/control_messages/*`
  - `renderer/server/www/README.md`

## 8. 현재 시점 기준 남은 일

이 브랜치에서 아직 남아 있는 일은 전부 "실제 기능 연결" 단계다.

- 실제 `--server` 실행 분기 연결
- headless renderer 경로 연결
- HTTP endpoint (`/`, `/stream.mjpg`, `/healthz`) 구현
- WebSocket endpoint (`/control`) 구현
- MJPEG frame 생성/전송
- camera handler 또는 render loop와의 실제 연결

즉, 지금까지의 작업은 후속 구현을 위한 기반 정리이며,
다음 단계부터는 신규 파일 추가 수준을 넘어 기존 실행 경로와 렌더링 루프를 실제로 건드리게 된다.


## 9. 2026-04-12 M2 headless one-shot snapshot 후속 구현

이번 후속 수정에서는 M2 범위를 "CLI에서 finite offscreen render를 수행하고 PNG snapshot을 저장한 뒤 종료하는 최소 경로"로 좁혀 실제 코드에 연결했다.

수정한 파일은 다음과 같다.

- `src/core/graphics/Window.cpp`
- `src/projects/extended_gaussian/apps/extended_gaussianViewer/main.cpp`
- `src/projects/extended_gaussian/renderer/ExtendedGaussianViewer.hpp`
- `src/projects/extended_gaussian/renderer/ExtendedGaussianViewer.cpp`

핵심 변경은 다음과 같다.

- `main.cpp`
  - `--headless`
  - `--manifest`
  - `--render-width`
  - `--render-height`
  - `--snapshot`
  - `--wait-for-streaming-idle`
  - `--max-headless-frames`
  를 정식 CLI 인자로 추가했다.
  - interactive loop와 별도로 finite headless loop를 추가했다.
  - headless mode에서는 `offscreen`, `nogui`, `vsync=0`을 강제로 적용하고 margin constructor 대신 size-based `Window` 경로를 사용한다.
- `ExtendedGaussianViewer`
  - GUI가 비활성 상태일 때 top-level ImGui 경로를 타지 않도록 guard를 추가했다.
  - raw model directory를 바로 load해서 scene instance를 만들 수 있는 helper를 추가했다.
  - `"Gaussian View"` subview를 PNG로 저장하는 snapshot helper를 추가했다.
  - manifest streaming queue가 비었는지 판정하는 helper를 추가했다.
  - manifest bounds focus 로직을 일반 bounds helper로 분리해 non-manifest headless path에서도 재사용하게 했다.
- `Window.cpp`
  - `GLFW_PLATFORM_NULL`이 사용 가능한 빌드에서는 `offscreen` 초기화 시 null platform hint를 먼저 주도록 보강했다.

이번 구현에서 의도적으로 유지한 제약은 다음과 같다.

- 현재 `offscreen`은 여전히 hidden GLFW window 경로다.
- 즉 이번 M2는 "true surfaceless EGL"이 아니라 "one-shot offscreen snapshot CLI" 구현이다.
- HTTP / MJPEG / WebSocket server는 아직 포함하지 않았다.

검증 메모:

- `cmake --build build-ninja-m2 --target extended_gaussianViewer_app --parallel` 통과
- `cmake --install build-ninja-m2` 통과
- `./install/bin/extended_gaussianViewer_app --help` 실행 통과
- `--help` 출력에서 새 M2 플래그 노출 확인
- Ubuntu Desktop에서 이 브랜치의 재빌드와 실행이 가능하다는 사용자 확인이 있었다.

당시 남은 확인(후속 section 10에서 해소):

- 실제 `--headless --snapshot` 명령으로 PNG가 생성되는지 desktop 환경에서 다시 확인
- real no-display 환경에서 `GLFW_PLATFORM_NULL` / EGL 조합이 실제로 충분한지 확인
- 이후 M3/M4에서 server runtime과 연결


## 10. 2026-04-12 M2 direct EGL headless 마무리

이후 후속 수정으로 M2의 no-display acceptance도 실제 코드 경로에서 닫았다.

추가로 수정한 파일은 다음과 같다.

- `src/core/graphics/Window.hpp`
- `src/core/graphics/Window.cpp`
- `src/core/graphics/CMakeLists.txt`
- `src/projects/extended_gaussian/apps/extended_gaussianViewer/main.cpp`

핵심 변경은 다음과 같다.

- `Window`
  - `DISPLAY` 와 `WAYLAND_DISPLAY` 가 모두 없는 `offscreen` 실행에서는 GLFW를 거치지 않고 direct EGL pbuffer context를 생성하도록 분기했다.
  - 생성 순서는 `EGL_EXT_platform_device` 우선, 가능하지 않으면 surfaceless/default display fallback 순으로 구성했다.
  - headless EGL 경로에서는 `size/position/isOpened/setVsynced/enableCursor/swapBuffer` 가 GLFW window 없이도 동작하도록 보강했다.
- `sibr_graphics` CMake
  - Linux에서 `EGL_FOUND` 인 경우 `libEGL` 을 실제 링크하도록 추가했다.
- `main.cpp`
  - `--headless` 에서 `--snapshot` 누락 시 nonzero exit 하도록 validation을 추가했다.
  - `--render-width`, `--render-height` 가 0 이하이면 nonzero exit 하도록 validation을 추가했다.
  - `--headless` 는 `--path` 없이도 empty snapshot smoke 를 허용하게 바꿨다.
  - 일반 `--offscreen` 실행도 margin ctor 대신 size-based `Window` ctor 를 사용하게 바꿔 no-display probe 에서 음수 크기가 들어가지 않게 했다.
  - GLFW window가 없는 direct EGL 경로에서는 poll을 건너뛰도록 루프를 보강했다.

검증 메모:

- `cmake --build build-ninja-m2 --target extended_gaussianViewer_app --parallel` 통과
- `cmake --install build-ninja-m2` 통과
- `env -u DISPLAY -u WAYLAND_DISPLAY ./install/bin/extended_gaussianViewer_app --headless --render-width 64 --render-height 64 --snapshot /tmp/extended_gaussian_empty.png` 통과
  - 결과: `/tmp/extended_gaussian_empty.png`, `173` bytes, `64x64` PNG
- `env -u DISPLAY -u WAYLAND_DISPLAY ./install/bin/extended_gaussianViewer_app --headless --render-width 640 --render-height 360 --path ../gaussian-splatting/eval/bonsai --snapshot /tmp/extended_gaussian_bonsai.png` 통과
  - 결과: `/tmp/extended_gaussian_bonsai.png`, `386837` bytes, `640x360` PNG
  - 로그: `GaussianView CUDA/GL interop enabled.`
- `timeout 2s env -u DISPLAY -u WAYLAND_DISPLAY ./install/bin/extended_gaussianViewer_app --offscreen --nogui --width 64 --height 64` 실행 시 direct EGL 초기화 후 `EXIT:124` 확인
  - 즉 기존 `DISPLAY environment variable is missing` crash 는 제거됐다.
- Ubuntu Desktop에서 이 브랜치의 재빌드와 GUI 실행이 가능하다는 사용자 확인이 있었다.

M2 completion report:

```text
- milestone: M2 headless EGL
- build: pass
- installed executable: install/bin/extended_gaussianViewer_app
- EGL backend used: direct EGL pbuffer (device/surfaceless/default fallback)
- interop: pass
- empty snapshot: /tmp/extended_gaussian_empty.png, 173 bytes, 64x64
- model snapshot: /tmp/extended_gaussian_bonsai.png, 386837 bytes, 640x360
- GUI regression: pass (Ubuntu Desktop user validation)
- M5 handoff capture target: "Gaussian View" subview via captureView helper
- unresolved blockers:
  - none for M2 scope
```

정리:

- M2 기준의 headless render context, empty snapshot, model snapshot, no-display probe, GUI regression evidence가 모두 확보됐다.
- M5 handoff용 capture 경계는 여전히 `"Gaussian View"` subview의 `captureView(...)` helper다.
- 이후 단계는 M3 server build surface 와 M4 HTTP skeleton 으로 넘어가면 된다.

## 11. 2026-04-12 M3 server build surface 구현 진행

현재 브랜치의 다음 단계는 runtime 기능 추가가 아니라, server 관련 소스의 build ownership 을 명시적으로 분리하는 것이다.

이번 M3에서 반영하려는 구현 내용은 다음과 같다.

- `renderer/server` 가 `extended_gaussian` shared library 에 `GLOB_RECURSE` 로 암묵 포함되던 상태를 끊고, `extended_gaussian_server` static target 으로 분리한다.
- `src/projects/extended_gaussian/CMakeLists.txt` 에 `SIBR_BUILD_REMOTE_STREAM` option 을 추가한다.
  - Linux 기본값: `ON`
  - Windows 기본값: `OFF`
- HTTP / WebSocket backend 는 `Boost.Beast` 로 고정한다.
- `TurboJPEG` 는 M3 시점에는 probe / summary 만 추가하고, hard requirement 는 M5 MJPEG 단계로 미룬다.
- canonical build tree 는 다시 `build-ninja/` 를 사용한다. `build-ninja-m2/` 는 더 이상 기준 경로로 쓰지 않는다.

이 단계에서 의도하는 검증 명령은 다음과 같다. 실제 결과는 후속 검증이 끝난 뒤 별도 기록한다.

```bash
cmake -S . -B build-ninja -G Ninja -DSIBR_BUILD_REMOTE_STREAM=ON
cmake --build build-ninja --target extended_gaussian_server --parallel
cmake --build build-ninja --target extended_gaussian --parallel
cmake --build build-ninja --target extended_gaussianViewer_app --parallel
```

추가 확인 포인트:

- `build-ninja/build.ninja` 또는 `compile_commands.json` 에서
  - `ServerProtocol.cpp`
  - `CameraPoseAdapter.cpp`
  가 `extended_gaussian_server` 에만 속하는지 확인
- install/runtime binary 증가 없이 build graph 만 정리되었는지 확인
- 실제 HTTP / MJPEG / WebSocket runtime 연결은 여전히 M4~M6 범위로 유지

## 12. 2026-04-12 M3 server build surface 검증 완료

위 section 11에서 계획한 M3 구현을 실제 `build-ninja/` 기준으로 configure/build/install 까지 검증했다.

실제 검증 결과는 다음과 같다.

- configure
  - `cmake -S . -B build-ninja -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CUDA_COMPILER=/usr/local/cuda-12.8/bin/nvcc -DSIBR_BUILD_REMOTE_STREAM=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON` 통과
  - configure 출력에서 다음을 확인했다.
    - `extended_gaussian remote stream modules: ON`
    - `extended_gaussian_server JSON backend: picojson`
    - `extended_gaussian_server HTTP/WebSocket backend: Boost.Beast`
    - `extended_gaussian_server JPEG backend: TurboJPEG not found...`
- build
  - `cmake --build build-ninja --target extended_gaussian_server --parallel` 통과
  - `cmake --build build-ninja --target extended_gaussian --parallel` 통과
  - `cmake --build build-ninja --target extended_gaussianViewer_app --parallel` 통과
- install / smoke
  - `cmake --build build-ninja --target install --parallel` 통과
  - `./install/bin/extended_gaussianViewer_app --help` 실행 통과
- source ownership
  - `build-ninja/build.ninja` 와 `compile_commands.json` 에서 `ServerProtocol.cpp`, `CameraPoseAdapter.cpp` 가 `extended_gaussian_server` 에만 매핑되는 것을 확인했다.
- static archive symbols
  - `ar -t build-ninja/src/projects/extended_gaussian/renderer/server/libextended_gaussian_server_rwdi.a` 결과에 `CameraPoseAdapter.cpp.o`, `ServerProtocol.cpp.o` 포함
  - `nm -C` 로 `ParseServerOptions`, `ParseControlMessageJson`, `TryBuildInputCamera`, `ExportRemoteCameraPose`, `ValidateRemoteCameraPose` 심볼 존재 확인
- install artifact scope
  - `install/bin`, `install/lib` 에는 기존 `extended_gaussianViewer_app`, `libextended_gaussian_rwdi.so` 만 설치되고, 별도 server runtime binary 나 `www` asset 은 추가되지 않았다.

M3 completion report:

```text
- milestone: M3 server build surface
- canonical build tree: build-ninja
- option: SIBR_BUILD_REMOTE_STREAM (Linux ON / Windows OFF)
- server target: extended_gaussian_server (STATIC)
- server source ownership: ServerProtocol + CameraPoseAdapter only
- HTTP/WebSocket backend: Boost.Beast
- JPEG backend policy: TurboJPEG selected, probe-only in M3
- build: pass
- install: pass
- viewer help smoke: pass
- deferred scope:
  - no --server runtime branch yet
  - no link from extended_gaussianViewer_app to extended_gaussian_server
  - no HTTP/MJPEG/WebSocket runtime yet
  - no www install/serving yet
```

정리:

- M3는 server parser/camera adapter 코드를 renderer 본체에서 분리해 명시적인 target ownership 으로 옮기는 단계다.
- runtime 동작은 아직 바뀌지 않았고, 이후 M4에서 HTTP skeleton 을 연결하면 된다.


## 13. 2026-04-13 M4 HTTP skeleton 구현 및 smoke 검증 완료

M4에서는 `extended_gaussianViewer_app` 안에 `RemoteStreamServer` 를 연결해, no-display viewer 프로세스와 같은 수명주기로 동작하는 HTTP skeleton 을 추가했다.

핵심 구현 범위:

- `main.cpp`
  - `--server`, `--listen-host`, `--listen-port`, `--bind`, `--port`, `--www-root` 와 M5 reserved stream 옵션을 app CLI surface 로 노출
  - `ParseServerOptions(...)` 로 canonical `ServerOptions` 를 만들고, `--server --headless` 조합은 즉시 거부
  - `RemoteStreamServer` 를 `main` 이 소유하고, viewer 생성 이후 start / loop 중 health publish / 종료 직후 stop 하도록 연결
  - `SIGINT`, `SIGTERM` 을 받아 interactive loop 를 정상 종료
- `renderer/server/ServerProtocol.*`
  - `--bind` / `--port` alias 와 `--www-root` override 를 `ServerOptions` 로 정규화
- `renderer/server/RemoteStreamServer.*` 신규 추가
  - 별도 server thread 에서 non-blocking accept loop 운영
  - `GET` / `HEAD` 만 허용하고, 나머지는 405 반환
  - `/healthz` 200 JSON 구현
  - `/`, `/index.html`, `/app.js`, `/styles.css`, `/static/*` 정적 자산 서빙 구현
  - `/stream.mjpg` 는 501 placeholder, `/control` 은 426 Upgrade Required placeholder 구현
  - percent-decoding + path traversal 방어 추가
- `ExtendedGaussianViewer.*`
  - M4 health snapshot 에 쓰기 위한 read-only getter (`getAppTimeSeconds`, `getFrameIndex`, `getCurrentPhase`) 노출
- CMake / install wiring
  - `src/projects/extended_gaussian/CMakeLists.txt` 에서 `renderer` 를 `apps` 보다 먼저 add 하도록 순서 수정
    - 이유: app CMake 가 `extended_gaussian_server` target 존재를 볼 수 있어야 app link 와 compile definition 이 적용됨
  - `apps/extended_gaussianViewer/CMakeLists.txt` 에서 app 이 `extended_gaussian_server` 를 link 하고 `SIBR_EXTENDED_GAUSSIAN_REMOTE_STREAM_BUILD=1` 을 받도록 수정
  - `www` install destination 을 `resources/extended_gaussian/server/www` 로 맞춤
  - `RemoteStreamServer` 의 install lookup 경로도 `resources/extended_gaussian/server/www` 를 우선 탐색하도록 수정

검증 명령:

```bash
cmake -S . -B build-ninja -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_CUDA_COMPILER=/usr/local/cuda-12.8/bin/nvcc \
  -DSIBR_BUILD_REMOTE_STREAM=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-ninja --target extended_gaussianViewer_app --parallel
cmake --build build-ninja --target install --parallel
./install/bin/extended_gaussianViewer_app --help
./install/bin/extended_gaussianViewer_app \
  --offscreen --nogui --server \
  --listen-host 127.0.0.1 \
  --listen-port 18080 \
  --width 640 --height 360
curl -i http://127.0.0.1:18080/healthz
curl -I http://127.0.0.1:18080/
curl -I http://127.0.0.1:18080/app.js
curl -I http://127.0.0.1:18080/styles.css
curl -I http://127.0.0.1:18080/static/app.js
curl -i http://127.0.0.1:18080/stream.mjpg
curl -i http://127.0.0.1:18080/control
```

실제 검증 결과:

- configure / build / install 통과
- `--help` 출력에 다음 옵션이 노출됨
  - `--server`
  - `--listen-host`
  - `--listen-port`
  - `--bind`
  - `--port`
  - `--www-root`
  - `--stream-width`, `--stream-height`, `--stream-fps`
- no-display runtime startup 통과
  - `Initialization of direct headless EGL`
  - `RemoteStreamServer listening on 127.0.0.1:18080`
  - resolved `www root` 가 `install/resources/extended_gaussian/server/www` 로 잡힘
- endpoint smoke
  - `/healthz`: `200 OK`, JSON body 에 `ok=true`, `service=extended_gaussian_remote_stream`, `version=m4-http-skeleton`, `renderer.initialized=true`, `renderer.has_manifest=false`, `renderer.frame_index` / `renderer.app_time_sec` 포함 확인
  - `/`: `200 OK`, `text/html`
  - `/app.js`: `200 OK`, `application/javascript`
  - `/styles.css`: `200 OK`, `text/css`
  - `/static/app.js`: `200 OK`, `application/javascript`
  - `/stream.mjpg`: `501 Not Implemented`
  - `/control`: `426 Upgrade Required`, `Upgrade: websocket`
- shutdown smoke
  - `Ctrl+C` 로 종료 시 `RemoteStreamServer stop elapsed: ... sec` 로그 확인
  - 프로세스 exit code `0`

M4 completion report:

```text
- milestone: M4 HTTP skeleton
- runtime shape: same-process server thread owned by main.cpp
- no-display startup: pass (direct EGL + offscreen + nogui)
- static assets: pass (install/resources/extended_gaussian/server/www)
- /healthz: pass
- /, /app.js, /styles.css, /static/*: pass
- /stream.mjpg: placeholder 501
- /control: placeholder 426 Upgrade Required
- shutdown via signal: pass
- deferred scope:
  - actual MJPEG encoding / multipart streaming
  - WebSocket upgrade and control session handling
  - concurrent long-lived connection model hardening
  - auth / TLS
```

정리:

- M4는 viewer 와 renderer/server 사이에 최소한의 runtime glue 를 연결해, no-display viewer process 위에 HTTP skeleton 을 올리는 단계로 닫는다.
- 이 단계에서 브라우저 reference client 와 health endpoint 는 실제로 동작한다.
- 실제 스트림 frame delivery 와 WebSocket control 은 M5/M6 로 넘긴다.


## 14. 2026-04-13 M5 MJPEG stream 구현 및 smoke 검증 완료

M5에서는 M4의 HTTP skeleton 위에 실제 MJPEG frame delivery 경로를 연결했다.
이 단계에서 `/stream.mjpg` 는 placeholder 가 아니라 실제 multipart JPEG stream 을 내보내며, same-process server thread 와 render loop 사이에 latest-only frame pipeline 이 추가되었다.

핵심 구현 범위:

- `renderer/server/JpegEncoder.*` 신규 추가
  - JPEG encoder backend 를 분리했다.
  - system `TurboJPEG` 가 없으면 configure 시 `extlibs/libjpeg-turbo/` 아래에 vendored `libjpeg-turbo` build 를 자동 준비하고, 그 라이브러리를 우선 사용한다.
- `renderer/server/MjpegStreamer.*` 신규 추가
  - render thread 에서 `IRenderTarget` 을 PBO ring 으로 비동기 readback
  - latest-only raw queue + encoder worker thread
  - resize 후 JPEG encode
  - latest encoded frame 을 stream clients 에 fan-out
- `renderer/server/RemoteStreamServer.*`
  - `/stream.mjpg` GET/HEAD 구현
  - client session thread model 추가
  - `/healthz` 에 stream metrics (`active_clients`, `frames_captured`, `frames_published`, `latest_sequence`, `jpeg_backend`) 노출
  - stop 시 accept loop / client sessions / encoder wait 가 함께 정리되도록 lifetime 정리
- `apps/extended_gaussianViewer/main.cpp`
  - `viewer.onRender(window)` 직후 `viewer.getGaussianViewRenderTarget()` 를 source 로 MJPEG capture submit
  - shutdown 시 current GL context 아래에서 `releaseRenderThreadResources()` 를 먼저 호출한 뒤 `server->stop()` 하도록 유지
  - `--headless --server` 는 더 이상 finite snapshot mode 가 아니라 offscreen no-GUI long-running server mode 로 동작
  - `--server --path <model_dir>` 조합일 때 GUI import 없이 model directory 를 자동 load 하도록 추가
- `ExtendedGaussianViewer.*`
  - M4에서 추가한 read-only getters 를 그대로 사용하고, capture source 는 계속 `"Gaussian View"` render target 으로 고정
- `renderer/server/CMakeLists.txt`
  - M5 source files (`JpegEncoder`, `MjpegStreamer`) 를 `extended_gaussian_server` target 에 추가
  - configure summary 에 JPEG backend probe 결과를 남김

검증 명령:

```bash
cmake --build build-ninja --target extended_gaussianViewer_app --parallel
LD_LIBRARY_PATH="$(find build-ninja -type f -name '*.so*' | sed 's#/[^/]*$##' | sort -u | paste -sd:):install/bin:install/lib" ./build-ninja/src/projects/extended_gaussian/apps/extended_gaussianViewer/extended_gaussianViewer_app   --headless --server   --listen-host 127.0.0.1   --listen-port 18080   --render-width 640 --render-height 360   --stream-width 640 --stream-height 360 --stream-fps 5   --path ../gaussian-splatting/eval/bonsai
curl -sS http://127.0.0.1:18080/healthz
curl -sS --max-time 6 -D /tmp/eg-m5-one.headers http://127.0.0.1:18080/stream.mjpg -o /tmp/eg-m5-one.mjpg
curl -sS --max-time 8 -D /tmp/eg-m5-c1.headers http://127.0.0.1:18080/stream.mjpg -o /tmp/eg-m5-c1.mjpg &
curl -sS --max-time 8 -D /tmp/eg-m5-c2.headers http://127.0.0.1:18080/stream.mjpg -o /tmp/eg-m5-c2.mjpg &
curl -sS http://127.0.0.1:18080/healthz
curl -sS -o /dev/null -w 'root=%{http_code}
' http://127.0.0.1:18080/
curl -sS -o /dev/null -w 'control=%{http_code}
' http://127.0.0.1:18080/control
curl -sSI http://127.0.0.1:18080/stream.mjpg
```

실제 검증 결과:

- `cmake --build build-ninja --target extended_gaussianViewer_app --parallel` 통과
- `cmake --build build-ninja --target install --parallel` 통과
  - Linux에서는 `cmake --install build-ninja` 가 누락된 타깃을 빌드하지 않으므로, canonical install 명령은 build-target install 이다.
  - app만 install할 때는 `cmake --build build-ninja --target extended_gaussianViewer_app_install --parallel` 을 사용한다.
  - 필요한 타깃을 미리 빌드한 뒤에는 `cmake --install build-ninja` 도 exit `0` 으로 통과한다.
  - `install_runtime.cmake` 의 Linux `INSTALL_PDB` early-return 과 executable postfix 처리 버그를 함께 수정해 app 전용 install target이 dependency bundle 과 `www/` install rule 을 실제로 생성하도록 정리했다.
  - 상세 코드 비교: `docs/source_diff_details_738e9d3_to_733ee7d/`
- no-display startup 통과
  - `Initialization of direct headless EGL`
  - `GaussianView CUDA/GL interop enabled.`
  - `Loading 1076487 Gaussians (SH Degree: 3)`
  - `RemoteStreamServer listening on 127.0.0.1:18080 ... JPEG backend: TurboJPEG`
- `/healthz`:
  - `200 OK`
  - `version=m5-mjpeg-stream`
  - `jpeg_backend=TurboJPEG`
  - `stream.width=640`, `stream.height=360`, `stream.fps=5`
- installed app smoke:
  - `./install/bin/extended_gaussianViewer_app --help` 통과
  - installed binary 로 `--headless --server --path ../gaussian-splatting/eval/bonsai` 실행 후 `/healthz` 에서 `jpeg_backend=TurboJPEG` 와 installed `www_root` 확인
- single-client `/stream.mjpg` smoke:
  - `200 OK`
  - `Content-Type: multipart/x-mixed-replace; boundary=ExGaussBoundary`
  - 6초 수신 결과 `boundary=29`, `jpeg=29`, `bytes=1218803`
  - `X-Sequence` 가 `1..10...` 으로 증가함을 확인
- two-client concurrent smoke:
  - 두 클라이언트 모두 `200 OK` + same boundary
  - 각 클라이언트에서 `boundary=39`, `jpeg=39`, `bytes=1639092`
  - live `/healthz` 에서 `active_clients=2`, `frames_captured=40`, `frames_published=40`, `latest_sequence=40` 확인
- 기존 M4 contract regression check:
  - `/`: `200 OK`
  - `/control`: `426 Upgrade Required`
  - `HEAD /stream.mjpg`: `200 OK`, multipart content-type
- shutdown smoke:
  - `Ctrl+C` 후 `RemoteStreamServer stop elapsed: 0.0053944 sec`
  - exit code `0`

M5 completion report:

```text
- milestone: M5 MJPEG stream
- render source: "Gaussian View" render target, post-render pre-swap capture
- no-display startup: pass (direct EGL + headless + server)
- model bootstrap for server mode: pass (--server --path auto-load)
- /healthz: pass with stream metrics
- /stream.mjpg: pass (single client)
- /stream.mjpg concurrent clients: pass (2 clients)
- /, static assets: pass
- /control: still placeholder 426 Upgrade Required
- shutdown via signal: pass
- deferred scope:
  - WebSocket upgrade and control session handling
  - remote camera application into viewer/update loop
  - JPEG backend hard requirement policy finalization
  - browser-side UX hardening / reconnect policy
```

정리:

- M5는 M4 skeleton 위에 실제 MJPEG frame transport 를 얹는 단계로 닫는다.
- 이 시점부터 no-display Ubuntu process 하나만으로 실제 Gaussian scene 을 브라우저로 스트리밍할 수 있다.
- remote control 적용은 M6에서 별도 lifecycle 로 연결한다.


## 15. 2026-04-13 M6 WebSocket control 구현 및 smoke 검증 완료

M6에서는 M5의 MJPEG stream 위에 실제 WebSocket camera control 경로를 연결했다.
이 단계에서 `/control` 은 더 이상 placeholder HTTP 426 endpoint 가 아니라, text JSON control payload 를 받아 viewer main/update loop 에 카메라 변경을 적용하는 실제 runtime endpoint 가 된다.

핵심 구현 범위:

- `renderer/ExtendedGaussianViewer.*`
  - `"Gaussian View"` subview 의 현재 카메라를 읽는 getter 와 새 카메라를 적용하는 setter 를 추가했다.
  - interactive camera handler 가 있으면 handler 를 통해 state 를 갱신하고, 그렇지 않으면 subview camera 를 직접 갱신한다.
- `apps/extended_gaussianViewer/main.cpp`
  - `/healthz` 에 현재 `"Gaussian View"` camera pose 를 함께 내보내도록 연결했다.
  - `viewer.onUpdate(...)` 직후, `viewer.onRender(...)` 직전에 server 의 latest control mailbox 를 소비하도록 연결했다.
  - `ParseControlMessageJson(...)` / `TryBuildInputCamera(...)` 로 payload 를 검증하고, apply 는 main thread 에서만 수행한다.
- `renderer/server/RemoteStreamServer.*`
  - `/control` WebSocket upgrade 를 실제 구현했다.
  - connection 직후 `ready` text JSON 을 전송하고, binary payload 는 거부한다.
  - valid `set_camera_pose` payload 는 latest-only mailbox 에 queue 하고 `ack` text JSON 을 반환한다.
  - invalid payload 는 `error` text JSON 으로 응답한다.
  - `/healthz` 에 control metrics (`active_control_clients`, `messages_received`, `messages_queued`, `messages_rejected`, `messages_superseded`, `messages_applied`, `apply_failures`, `latest_received_sequence`, `last_applied_sequence`, `message_pending`) 와 current renderer camera pose 를 노출한다.
  - shutdown 시 accept loop, MJPEG sessions 와 함께 control WebSocket sessions 도 clean stop 되도록 lifetime 을 정리했다.

검증 명령:

```bash
cmake --build build-ninja --target extended_gaussianViewer_app --parallel
cmake --build build-ninja --target install --parallel
./install/bin/extended_gaussianViewer_app   --headless --server   --listen-host 127.0.0.1   --listen-port 18080   --render-width 640 --render-height 360   --stream-width 320 --stream-height 180   --stream-fps 2   --path ../gaussian-splatting/eval/bonsai
curl -sf http://127.0.0.1:18080/healthz
curl -s -D /tmp/m6_control_get_headers.txt -o /tmp/m6_control_get_body.txt http://127.0.0.1:18080/control
node /tmp/m6_ws_smoke.js
curl --max-time 2 -s http://127.0.0.1:18080/stream.mjpg -o /tmp/m6_stream_after_control.bin
curl -sf http://127.0.0.1:18080/healthz
```

실제 검증 결과:

- `cmake --build build-ninja --target extended_gaussianViewer_app --parallel` 통과
- `cmake --build build-ninja --target install --parallel` 통과
- installed binary no-display startup 통과
  - `Initialization of direct headless EGL`
  - `GaussianView CUDA/GL interop enabled.`
  - `Loading 1076487 Gaussians (SH Degree: 3)`
  - `RemoteStreamServer listening on 127.0.0.1:18080 ... JPEG backend: TurboJPEG`
- `/healthz`
  - `200 OK`
  - `version=m6-websocket-control`
  - `renderer.camera_pose_available=true`
  - current camera pose JSON 노출 확인
- plain `GET /control`
  - `426 Upgrade Required`
  - JSON body: `{"ok":false,"type":"error","error":"WebSocket upgrade required for /control."}`
- WebSocket `/control` smoke
  - connect 직후 `ready` text JSON 수신
  - invalid payload 전송 시 `error` text JSON 수신
    - `"Missing required numeric field 'fovy'."`
  - valid payload 전송 시 `ack` text JSON 수신
    - `request_type=set_camera_pose`
    - `status=queued`
    - `queue_mode=latest_only`
    - `sequence=1`
- apply result 확인
  - 후속 `/healthz` 에서 `control.messages_received=2`
  - `control.messages_queued=1`
  - `control.messages_rejected=1`
  - `control.messages_applied=1`
  - `control.apply_failures=0`
  - `control.last_applied_sequence=1`
  - `control.message_pending=false`
  - `renderer.camera_pose.position=[0.5,-1.0,8.0]`
  - `renderer.camera_pose.fovy=1.0`
- control 적용 후 `/stream.mjpg` 재확인
  - multipart stream 출력 계속 유지
  - `/tmp/m6_stream_after_control.bin` capture 생성 확인
- shutdown
  - signal 후 clean stop 확인
- 상세 코드 비교: `docs/source_diff_details_733ee7d_to_d30707f/`

M6 completion report:

```text
- milestone: M6 websocket camera control
- /control websocket upgrade: pass
- ready/ack/error text frames: pass
- latest-only queue semantics: pass
- main-thread camera apply: pass
- invalid payload rejection without apply: pass
- /healthz control metrics + camera pose: pass
- control after stream start: pass
- clean shutdown with control sessions: pass
- validated runtime path this turn: installed binary, no-display --headless --server
```

상세 코드 비교: `docs/source_diff_details_733ee7d_to_d30707f/`

정리:

- M6는 browser-side control contract 와 viewer camera apply path 를 실제 runtime 으로 연결하는 단계로 닫는다.
- 이 시점부터 headless Ubuntu process 하나로 MJPEG preview 와 WebSocket camera control 이 모두 동작한다.
- 남은 후속 범위는 UX 보강, reconnect/backoff, auth, richer control verbs 같은 확장 사항이다.


## 16. 2026-04-13 M7 integration verification

M7에서는 기능을 더 붙이지 않고, M1-M6 결과를 merge 판단 가능한 상태로 검증하고 문서화하는 데 집중했다.

이번 턴에서 추가한 코드/도구 범위:

- `apps/extended_gaussianViewer/main.cpp`
  - pending control message consume 시 `received_at` 를 함께 받아 receive-to-apply latency 를 기록하도록 연결했다.
  - 성공 apply 된 control sequence 를 `submitRenderedFrame(...)` 에 넘겨 MJPEG part header 와 health summary 가 같은 control sequence 를 공유하도록 정리했다.
- `renderer/server/MjpegStreamer.*`
  - encoded frame metadata 에 `control_sequence`, `capture_to_raw_ready_ms`, `encode_ms`, `capture_to_encoded_ms`, `encoded_unix_time_ms` 를 추가했다.
  - stats 에 encoded bytes total/last/average 와 timing summary 를 추가했다.
- `renderer/server/RemoteStreamServer.*`
  - `/healthz` 에 stream/control timing summary 와 encoded byte stats 를 추가했다.
  - `recordControlMessageApplied(...)` 가 receive-to-apply latency summary 를 유지하도록 정리했다.
  - MJPEG part header 에 `X-Control-Sequence`, `X-Capture-To-Raw-Ready-Ms`, `X-Encode-Ms`, `X-Capture-To-Encoded-Ms`, `X-Encoded-Unix-Ms` 를 추가했다.
- `tools/remote_stream/*`
  - `measure_mjpeg.py`: part header timing, `X-Control-Sequence`, frames JSONL/CSV, control-to-visible proxy 계산 지원
  - `collect_runtime_stats.py`: `/healthz` JSONL/CSV + `/proc/<pid>` RSS/VmSize/thread/fd sampling 지원
  - `ws_control_smoke.mjs`, `ws_pose_flood.mjs`: M6/M7 control smoke / flood evidence 수집
- 문서
  - `docs/extended_gaussian_remote_browser_stream_verification_report_ko.md`
  - `docs/extended_gaussian_ubuntu24_remote_browser_stream_user_guide_ko.md`
  - `docs/extended_gaussian_remote_browser_stream_known_issues_ko.md`
  - `README.md`, `docs/extended_gaussian_ubuntu24_toolchain_status_ko.md` 업데이트

실제 검증 결과 (loopback, installed binary 기준):

```text
build / install:
- cmake --build build-ninja --target extended_gaussianViewer_app --parallel: PASS
- cmake --build build-ninja --target install --parallel: PASS

server bring-up:
- direct EGL init: PASS
- CUDA/GL interop enabled: PASS
- model load (bonsai, 1,076,487 gaussians): PASS
- bind host/port: 127.0.0.1:18180 (18080은 기존 listener 사용 중이라 회피)

HTTP/static:
- /: 200
- /app.js: 200
- /styles.css: 200
- /healthz: 200, version=m7-integration-verification
- GET /control: 426 Upgrade Required

single-client MJPEG (15s):
- frames_received=205
- measured_fps=13.6408
- first_frame_delay_sec=0.1158
- encode_p95_ms=6.914
- capture_to_encoded_p95_ms=18.925

two-client MJPEG (12s each):
- client A frames_received=166, fps=13.8114
- client B frames_received=166, fps=13.8189
- active_clients=2 유지 확인

WebSocket smoke:
- ready: PASS
- invalid payload -> error: PASS
- valid payload -> ack: PASS
- health correlation: messages_applied / last_applied_sequence / camera_pose update 확인

WebSocket orbit flood (10s, 5Hz):
- sentPayloads=50
- ackCount=50
- errorCount=0
- receive_to_apply p95=7.575ms

control-to-encoded proxy:
- ack sequence=52
- first MJPEG frame with X-Control-Sequence=52 observed
- ack_to_encoded_ms=69

120s loopback soak:
- frames_received=1690
- measured_fps=14.0780
- health samples=24, failures=0
- RSS drift=+28 KB (+0.005%)
```

M7 merge gate 관점 정리:

- loopback protocol / process / performance evidence는 통과했다.
- 아직 browser executable 이 현재 shell PATH 에 없어서 LAN browser page open 과 browser-visible control-to-visible 은 `SKIP_ENV` 로 남겼다.
- 1-hour soak 와 clean SHA rerun 은 이번 턴에서 수행하지 못해 `BLOCKED` 로 남겼다.
- 최종 상태는 `implementation ready, verification partial pass` 로 기록한다.

상세 코드 비교: `docs/source_diff_details_d30707f_to_0d9f177/`


## 2026-04-15 M8 browser camera control

M8에서는 서버 C++ 경로를 건드리지 않고, 브라우저 reference client (`www/`) 위에 **실시간 키보드/마우스 카메라 제어 UX** 를 올렸다.

수정 파일:

- `src/projects/extended_gaussian/renderer/server/www/app.js`
- `src/projects/extended_gaussian/renderer/server/www/index.html`
- `src/projects/extended_gaussian/renderer/server/www/styles.css`

적용한 변경:

- `app.js`
  - `CameraController` 를 추가하고 `WASD`, `Q/E`, 화살표 키, preview 드래그/휠 입력을 `set_camera_pose` WebSocket payload 로 변환하도록 구현했다.
  - `requestAnimationFrame` 기반 tick 루프에서 최대 약 30 Hz (`33 ms`) 로 pose 를 전송하도록 throttle 했다.
  - `ready.camera_pose` 수신 시 form/controller state 를 동기화하고, `ack` 는 status line 을 갱신하지 않도록 해서 연속 제어 시 UI가 과도하게 흔들리지 않게 했다.
  - `input/textarea/select/contenteditable` 포커스 시 키 입력을 무시하고, `window.blur` 에서 pressed key state 를 초기화해 stuck key 를 방지했다.
  - `wheel` 은 `deltaMode` 를 정규화해 마우스와 트랙패드 간 감도 차이를 줄였고, pitch 는 `|dot(forward, WORLD_UP)| < 0.99` 에서 clamp 했다.
  - form input 과 payload textarea 가 현재 camera pose 와 같이 움직이도록 동기화 경로를 추가했다.
- `index.html`
  - `Camera Control` 패널, enable/disable 버튼, 이동/회전 속도 슬라이더, 키 범례를 추가했다.
- `styles.css`
  - active 버튼 상태, preview 활성 강조, key legend / `kbd`, range slider 스타일을 추가했다.

검증 결과:

- `node --check src/projects/extended_gaussian/renderer/server/www/app.js`: PASS
- 설치 서버를 `--www-root src/projects/extended_gaussian/renderer/server/www` 로 실행한 뒤 `/`, `/app.js`, `/styles.css` 정적 자산 서빙 확인: PASS
- source 자산 기준 smoke
  - `/` 에 `toggle-camera`, `move-speed`, `rotate-speed` 존재
  - `/app.js` 에 `CameraController`, `toggleCameraControl` 존재
  - `/styles.css` 에 `button.active`, `.key-legend`, `#stream-preview.camera-active` 존재
- 사용자 로컬 브라우저 수동 확인
  - `Open Stream` / `Connect WebSocket` / `Enable Camera Control` 후 조작 결과 확인: PASS

운영상 주의:

- 브라우저 제어는 `/control` WebSocket contract 재사용만 수행하며, 인증/TLS는 여전히 서버 쪽 미구현이다.
- 다른 컴퓨터에서 접속하려면 `--listen-host 127.0.0.1` 대신 Tailscale IP 또는 `0.0.0.0` 로 다시 실행해야 한다. 이 부분은 M8 변경 범위가 아니라 기존 서버 bind 정책 문제다.

상세 코드 비교: `docs/source_diff_details_7627561_to_53ed390/`


## 2026-04-15 manifest remote stream phase/status bridge

이번 단계에서는 기존 headless remote browser stream 을 single-model only 상태에서 한 단계 확장해, manifest 기반 스트리밍에서도 browser 쪽에서 phase 와 streaming 상태를 직접 다룰 수 있게 정리했다.

추가된 runtime contract:

- WebSocket control
  - 새 control verb: `set_phase`
  - `ack.request_type` 으로 `set_camera_pose` / `set_phase` 구분
- WebSocket `ready`
  - `has_manifest`, `current_phase`, `available_phases`, `total_assets` 추가
- `/healthz`
  - `renderer.current_phase`
  - `renderer.available_phases`
  - `renderer.total_assets`
  - `renderer.streaming.{required_gpu,warm_cpu,pending_disk_loads,pending_gpu_uploads,pending_gpu_evictions,cpu_resident_bytes,gpu_resident_bytes,skipped_instances,swap_hits,swap_misses}` 추가
- browser `www` client
  - `Manifest Status` 패널
  - phase dropdown + custom input
  - `Apply Phase` button
  - `Start/Stop Health Poll` button
  - 1 Hz `/healthz` polling 으로 streaming stats live 갱신

핵심 구현 포인트:

- `ServerProtocol` 은 `set_phase` payload 를 별도 분기로 파싱하고 camera pose validation 과 분리했다.
- `ExtendedGaussianViewer` 는 remote control 경로에서 바로 사용할 수 있도록 phase setter/getter 와 manifest summary getter 를 공개했다.
- `main.cpp` 는 `SetPhase` 를 viewer `_currentPhase` 변경으로 연결하고, health snapshot 에 `SwapManager::Stats` 를 실었다.
- `RemoteStreamServer` 는 `/healthz`, `ready`, `ack` 를 모두 확장해 browser client 가 별도 ImGui 없이도 manifest 상태를 관찰할 수 있게 했다.
- browser client 는 `set_phase` 단발 요청만 status line 에 노출하고, 연속 `set_camera_pose` ack 는 여전히 숨겨 camera control UX 의 노이즈를 줄였다.

실제 검증:

```text
build:
- cmake --build build-ninja --target extended_gaussianViewer_app --parallel: PASS
- cmake --install build-ninja: PASS

browser asset sanity:
- node --check src/projects/extended_gaussian/renderer/server/www/app.js: PASS
- GET / on runtime server: Manifest Status / phase-select / toggle-health-poll 확인

manifest runtime (local temporary manifest, absolute model path -> ../CityGaussianV1/output/mc_small_aerial_c36):
- /healthz: current_phase=alpha, available_phases=[alpha,beta], total_assets=1 확인
- ws_control_smoke.mjs: ready.has_manifest=true 확인
- ws_control_smoke.mjs: ack.request_type=set_phase 확인
- follow-up /healthz: current_phase=default_phase 반영 확인
```

운영상 정리:

- 이제 manifest mode 에서도 headless browser client 만으로 현재 phase 와 streaming 상태를 볼 수 있고, phase 전환을 시도할 수 있다.
- phase validation 은 intentionally 없음. desktop ImGui 와 같은 정책으로 arbitrary string 을 허용하고, rule 이 없으면 결과는 no-op 이다.
- 보안/TLS/auth 부재는 여전히 기존 서버 제약 그대로다. public bind 에서는 조심해야 한다.

상세 코드 비교: `docs/source_diff_details_44a8416_to_manifest_remote_stream/`
