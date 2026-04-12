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
