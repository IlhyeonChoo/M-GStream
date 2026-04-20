# Ubuntu 24.04 Remote Browser Stream User Guide

작성일: 2026-04-13  
대상 브랜치: `develop/ubuntu24-remote-browser-stream`

## 1. 범위

이 문서는 Ubuntu 24.04에서 `M_GStreamViewer_app`를 headless 또는 GUI 모드로 실행하고,
HTTP/MJPEG/WebSocket 기반 remote browser stream 기능을 사용하는 절차를 정리한다.

현재 범위:

- build / install
- local 또는 user-provided sample model bring-up
- `--headless --server` 실행
- reference page 기준 `/stream.mjpg`와 `/control` 사용
- `/healthz`와 보조 측정 도구로 상태 확인

범위 밖:

- public Internet deployment
- TLS / authentication / authorization
- production browser UX
- WebRTC / H.264 / NVENC

## 2. 보안 경고

이 서버에는 public exposure에 필요한 인증, 권한 분리, TLS, rate limit이 없다.

- loopback
- trusted LAN
- trusted VPN

에서만 사용한다.

직접 public Internet에 노출하지 않는다.

## 3. 전제 조건

- Ubuntu 24.04
- NVIDIA driver + CUDA 12.8 호환 환경
- `build-ninja/`가 이미 configure 되어 있어야 한다.
- trained Gaussian model directory 또는 manifest JSON이 필요하다.

기대하는 모델 디렉터리 구조:

```text
<modelPath>/
  cfg_args
  point_cloud/
    iteration_<N>/
      point_cloud.ply
```

## 4. Build / install

Canonical commands:

```sh
cmake --build build-ninja --target M_GStreamViewer_app --parallel
cmake --build build-ninja --target install --parallel
cmake --install build-ninja
```

설치 후 기본 실행 파일:

```text
install/bin/M_GStreamViewer_app
```

설치된 reference client root:

```text
install/resources/M_GStream/server/www
```

## 5. 모델 준비

모델 디렉터리를 직접 지정하는 방식:

```sh
install/bin/M_GStreamViewer_app   --headless   --server   --path <modelPath>
```

manifest JSON을 사용하는 방식:

```sh
install/bin/M_GStreamViewer_app   --headless   --server   --manifest <manifest.json>
```

## 6. Headless server 실행

Loopback only:

```sh
install/bin/M_GStreamViewer_app   --headless   --server   --listen-host 127.0.0.1   --listen-port 8080   --render-width 1280   --render-height 720   --stream-width 1280   --stream-height 720   --stream-fps 15   --path <modelPath>
```

Trusted LAN expose:

```sh
install/bin/M_GStreamViewer_app   --headless   --server   --listen-host 0.0.0.0   --listen-port 8080   --render-width 1280   --render-height 720   --stream-width 1280   --stream-height 720   --stream-fps 15   --path <modelPath>
```

M4 alias도 유지된다.

```text
--bind 127.0.0.1
--port 8080
```

## 7. GUI + server 실행

GUI와 server를 함께 띄우려면 `--headless`를 빼고 `--server`를 유지한다.

```sh
install/bin/M_GStreamViewer_app   --server   --listen-host 127.0.0.1   --listen-port 8080   --path <modelPath>
```

## 8. Browser 접속

같은 머신:

```text
http://127.0.0.1:8080/
```

LAN의 다른 머신:

```text
http://<server-lan-ip>:8080/
```

Reference page가 기본적으로 사용하는 경로:

- stream: `/stream.mjpg`
- control: `/control`
- health: `/healthz`

## 9. Endpoint 요약

### 9.1 `/`

- reference HTML page
- installed `www/` asset root에서 서빙

### 9.2 `/healthz`

JSON으로 다음 정보를 제공한다.

- server listen host / port / www root
- stream client count, frame counters, configured size / FPS
- stream timing summary (`capture_to_raw_ready`, `encode`, `capture_to_encoded`)
- control client count, queued / rejected / applied counters
- control timing summary (`receive_to_apply`)
- renderer frame index, app time, current camera pose

### 9.3 `/stream.mjpg`

- `multipart/x-mixed-replace`
- JPEG backend는 `TurboJPEG` 또는 fallback backend
- multi-client latest-frame fan-out 구조
- part header에 `X-Sequence`, `X-Control-Sequence`, timing header 포함

### 9.4 `/control`

- text WebSocket only
- valid payload: `ack`
- invalid payload: `error`
- connect 직후: `ready`
- queue policy: `latest_only`

## 10. Camera control payload

현재 지원하는 control message:

```json
{
  "type": "set_camera_pose",
  "position": [5, 5, 5],
  "forward": [-0.577, -0.577, -0.577],
  "up": [0, 1, 0],
  "fovy": 0.78539816339
}
```

계약 규칙:

- `0 < fovy < pi`
- `position`, `forward`, `up`는 길이 3 숫자 배열
- `forward`, `up`는 zero vector 금지
- `forward`, `up`는 parallel / near-parallel 금지

예제 payload는 여기 있다.

```text
src/projects/M_GStream/renderer/server/examples/control_messages/
```

## 11. 진단과 측정

빠른 상태 확인:

```sh
curl -fsS http://127.0.0.1:8080/healthz | python3 -m json.tool
curl -I http://127.0.0.1:8080/
curl -I http://127.0.0.1:8080/app.js
curl -I http://127.0.0.1:8080/styles.css
curl -i http://127.0.0.1:8080/control
```

M7 보조 도구:

```sh
uv run python tools/remote_stream/measure_mjpeg.py   --url http://127.0.0.1:8080/stream.mjpg   --duration-sec 30   --summary-json /tmp/eg_stream_summary.json   --frames-csv /tmp/eg_stream_frames.csv   --frames-jsonl /tmp/eg_stream_frames.jsonl

uv run python tools/remote_stream/collect_runtime_stats.py   --url http://127.0.0.1:8080/healthz   --pid <viewer_pid>   --duration-sec 600   --interval-sec 10   --output-jsonl /tmp/eg_healthz.jsonl   --output-csv /tmp/eg_healthz.csv   --summary-json /tmp/eg_healthz_summary.json

node tools/remote_stream/ws_control_smoke.mjs   --url ws://127.0.0.1:8080/control   --send-file src/projects/M_GStream/renderer/server/examples/control_messages/invalid_missing_fovy.json   --send-file src/projects/M_GStream/renderer/server/examples/control_messages/valid_set_camera_pose_default.json   --output-json /tmp/eg_ws_smoke.json

node tools/remote_stream/ws_pose_flood.mjs   --url ws://127.0.0.1:8080/control   --healthz-url http://127.0.0.1:8080/healthz   --mode orbit   --duration-sec 10   --rate-hz 5   --summary-json /tmp/eg_ws_orbit.json
```

## 12. 종료

- foreground process: `Ctrl+C`
- loopback/local test 후 port가 해제되었는지 `ss -ltnp | rg ':8080'`로 확인한다.

## 13. Troubleshooting

대표 이슈:

- configure 전에 `build.ninja`가 생성되지 않음
- CUDA compiler / host GCC mismatch
- rasterizer FetchContent clone 실패
- missing `libturbojpeg`
- EGL initialization failed
- no-display startup failure
- port already in use
- browser cannot reach server on LAN
- `/stream.mjpg` blank or first frame slow
- `/control` returns `426`
- invalid camera pose rejected

자세한 known issues와 후속 항목은 별도 문서를 본다.

```text
docs/M_GStream_remote_browser_stream_known_issues_ko.md
```
