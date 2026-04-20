# Remote Browser Stream Verification Report

작성일: 2026-04-13  
대상 브랜치: `develop/ubuntu24-remote-browser-stream`  
검증 기준 SHA: `5db46573a9c4` + local M7 verification edits (dirty worktree)

## 1. 목적

이 문서는 Ubuntu 24.04 loopback 환경에서 `M_GStream` remote browser stream 기능의 M7 통합 검증 결과를 한 곳에 고정한다.

M7의 본질은 새 기능 추가가 아니라 다음을 merge 판단 가능한 상태로 만드는 것이다.

- build / install evidence
- `/healthz`, `/stream.mjpg`, `/control` runtime evidence
- multi-client / soak / performance evidence
- 남은 환경 의존 항목과 blocker 구분

## 2. 검증 경계

이번 실행에서 실제로 검증한 범위:

- `build-ninja/` 기준 build / install
- installed binary 기준 headless direct EGL bring-up
- `/`, `/healthz`, `/app.js`, `/styles.css`
- single-client MJPEG
- two-client MJPEG
- WebSocket `ready` / `error` / `ack`
- valid pose apply 후 `/healthz` correlation
- `X-Control-Sequence` / `X-Encoded-Unix-Ms` 기반 control-to-encoded proxy latency
- 120초 loopback soak + RSS drift 샘플

이번 실행에서 닫지 못한 범위:

- 실제 browser-visible control-to-visible 증거
- 실제 LAN peer browser 접속 증거
- 1-hour soak
- clean SHA 기준 rerun

## 3. 대상 환경

| 항목 | 값 |
|---|---|
| OS | Ubuntu 24.04 |
| build tree | `build-ninja/` |
| app binary | `install/bin/M_GStreamViewer_app` |
| GPU path | direct headless EGL + NVIDIA OpenGL 4.6 |
| trust boundary | loopback / trusted LAN / trusted VPN |
| browser executable on PATH | 없음 (`chromium`, `firefox`, `google-chrome*` 미발견) |
| validation port | `18180` (`18080`은 기존 listener 사용 중이라 회피) |

샘플 모델:

```text
sample_model_name: bonsai
sample_model_source_url: local workspace sample under ../gaussian-splatting/eval/bonsai
sample_model_license: not recorded in this workspace
sample_model_download_date: unknown
sample_model_local_path: /home/ilhyeonchu/ReCompose3D/3DGS/gaussian-splatting/eval/bonsai
sample_model_size_bytes: 794044599
sample_manifest_path: not used in this run
```

## 4. 실행 명령

Build / install:

```sh
cmake --build build-ninja --target M_GStreamViewer_app --parallel
cmake --build build-ninja --target install --parallel
```

Server bring-up:

```sh
./install/bin/M_GStreamViewer_app   --headless   --server   --listen-host 127.0.0.1   --listen-port 18180   --render-width 1280   --render-height 720   --stream-width 1280   --stream-height 720   --stream-fps 15   --path ../gaussian-splatting/eval/bonsai
```

보조 도구:

```sh
uv run python tools/remote_stream/measure_mjpeg.py ...
uv run python tools/remote_stream/collect_runtime_stats.py ...
node tools/remote_stream/ws_pose_flood.mjs ...
node tools/remote_stream/ws_control_smoke.mjs ...
```

## 5. 검증 상태 요약

| area | status | note |
|---|---|---|
| build / install | RUN_PASS | `M_GStreamViewer_app` build 및 `install` target 통과 |
| headless server bring-up | RUN_PASS | installed binary direct EGL startup 통과 |
| `/` / static assets | RUN_PASS | `/`, `/app.js`, `/styles.css` 모두 `200` |
| `/healthz` | RUN_PASS | `version=m7-integration-verification`, timing summary 포함 |
| single-client MJPEG | RUN_PASS | 15초 측정, 205 frames, 13.64 FPS |
| two-client MJPEG | RUN_PASS | 12초 동시 측정, 양쪽 모두 166 frames |
| WebSocket valid / invalid payload | RUN_PASS | `ready`, `error`, `ack`, apply counter correlation 확인 |
| control-to-encoded proxy | RUN_PASS | `ack` -> first `X-Control-Sequence` visible frame 69ms |
| 120초 local soak | RUN_PASS | 1690 frames, 14.08 FPS, RSS drift +28 KB |
| LAN browser page open | SKIP_ENV | 이 shell에는 browser binary와 LAN peer evidence가 없음 |
| browser control-to-visible | SKIP_ENV | 브라우저 렌더 화면 기준 증거 없음 |
| 1-hour soak | BLOCKED | 이번 턴에서는 120초 soak까지만 수행 |
| clean SHA rerun | BLOCKED | worktree가 dirty 상태라 clean commit 기준 rerun 아님 |

## 6. Scenario Table

상태 값:

- `RUN_PASS`
- `RUN_FAIL`
- `SKIP_SCOPE`
- `SKIP_ENV`
- `BLOCKED`

| id | checklist item | status | actual result | evidence |
|---|---|---|---|---|
| V01 | build / install | RUN_PASS | `cmake --build build-ninja --target M_GStreamViewer_app --parallel` 통과, `cmake --build build-ninja --target install --parallel` 통과 | local shell run, branch log / mod log append |
| V02 | sample model structure | RUN_PASS | `../gaussian-splatting/eval/bonsai` 에 `cfg_args`, `point_cloud/iteration_30000/point_cloud.ply` 존재 | local filesystem check |
| V03 | headless server bring-up | RUN_PASS | direct EGL init, CUDA/GL interop enabled, 1,076,487 gaussians load, listen `127.0.0.1:18180` | server startup log |
| V04 | `/` / static asset | RUN_PASS | `/` `200`, `/app.js` `200`, `/styles.css` `200` | local `curl -I` |
| V05 | `/healthz` | RUN_PASS | `version=m7-integration-verification`, renderer / stream / control timing summary 확인 | local `curl` |
| V06 | single-client MJPEG | RUN_PASS | 15.03초, 205 frames, 13.64 FPS, first frame 115.8ms | `/tmp/M_GStream_m7/single_stream_summary.json` |
| V07 | two-client MJPEG | RUN_PASS | client A/B 각 166 frames, `active_clients_min=max=2` | `/tmp/M_GStream_m7/two_client_a_summary.json`, `two_client_b_summary.json`, `two_client_healthz.jsonl` |
| V08 | GET `/control` upgrade 안내 | RUN_PASS | `426 Upgrade Required`, JSON error body 확인 | local `curl -i` |
| V09 | WS valid payload apply | RUN_PASS | valid payload `ack sequence=2`, `/healthz`에서 `messages_applied=2`, pose 변경 확인 | `/tmp/M_GStream_m7/ws_smoke_summary.json` |
| V10 | WS invalid payload reject | RUN_PASS | `Missing required numeric field 'fovy'.` error, rejected counter 증가 | `/tmp/M_GStream_m7/ws_smoke_summary.json` |
| V11 | control flood / latest-only loop | RUN_PASS | 10초 orbit, 50 payload / 50 ack, `messages_applied=51`, `receive_to_apply.p95=7.575ms` | `/tmp/M_GStream_m7/ws_orbit_summary.json` |
| V12 | control-to-encoded proxy | RUN_PASS | `ack sequence=52` 이후 첫 `control_sequence=52` frame의 `ack_to_encoded_ms=69` | `/tmp/M_GStream_m7/control_visible_ws.json`, `control_visible_frames.jsonl` |
| V13 | 120초 local soak | RUN_PASS | 120.05초, 1690 frames, 14.08 FPS, health sample 24회 / failure 0회, RSS drift +28 KB (0.005%) | `/tmp/M_GStream_m7/soak_*` |
| V14 | LAN browser page open | SKIP_ENV | browser binary 없음, LAN peer 미사용 | local PATH check |
| V15 | browser control-to-visible | SKIP_ENV | 브라우저 화면 기준 증거 없음 | same reason |
| V16 | 1-hour soak | BLOCKED | 120초 soak만 수행 | follow-up 필요 |
| V17 | clean SHA rerun | BLOCKED | verification instrumentation 및 문서 변경이 아직 commit 전 | `git status` |

## 7. 성능 / 안정성 요약

| metric | target | measured | status | note |
|---|---|---|---|---|
| single-client stream FPS | target 15 FPS의 90% 이상 | 13.64 FPS (90.9%) | RUN_PASS | 15초, 205 frames |
| two-client stream FPS | target 15 FPS의 90% 이상 | 13.81 FPS (92.1%) | RUN_PASS | 두 클라이언트 동일 |
| 120초 soak stream FPS | target 15 FPS의 90% 이상 | 14.08 FPS (93.9%) | RUN_PASS | loopback only |
| JPEG encode latency p95 | <= 30ms | 6.914ms / 6.830ms / 6.652ms | RUN_PASS | single / two-client / soak |
| capture-to-encoded latency p95 | <= 80ms | 18.925ms / 18.933ms / 18.160ms | RUN_PASS | same order |
| WS receive-to-apply p95 | <= 50ms loopback | 7.575ms | RUN_PASS | orbit flood summary |
| control-to-encoded proxy | reference only | 69ms | RUN_PASS | browser-visible full metric 아님 |
| 120초 RSS drift | <= 5% reference | +28 KB, +0.005% | RUN_PASS | PID 491641 기준 |
| 1-hour RSS / VRAM drift | <= 5% reference 또는 원인 기록 | not run | BLOCKED | follow-up 필요 |

## 8. 남은 blocker / follow-up

즉시 남아 있는 M7 blocker:

1. 실제 browser / LAN evidence 부재
2. 1-hour soak 부재
3. clean SHA 기준 rerun 부재

비 blocker follow-up:

- browser-visible control-to-visible 측정을 reference client나 devtools capture와 묶기
- 4-client 이상 및 slow-client tuning은 post-M7 backlog로 유지
- deployment hardening은 별도 milestone로 유지

## 9. merge 판단

현재 결론:

- protocol / process / loopback runtime 경로는 실질적으로 통과했다.
- 성능 수치도 M7 목표치 이내다.
- 하지만 M7 merge gate를 엄격히 닫으려면 browser/LAN/1-hour soak/clean SHA rerun이 추가로 필요하다.

따라서 현재 merge 판단은 다음과 같다.

```text
implementation status: ready
verification status: partial pass, environment blockers remain
merge gate: not fully closed yet
```
