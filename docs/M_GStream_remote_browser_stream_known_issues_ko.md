# Remote Browser Stream Known Issues and Backlog

작성일: 2026-04-13  
대상 브랜치: `develop/ubuntu24-remote-browser-stream`

## 1. 목적

이 문서는 remote browser stream의 known issue, scope-out, 후속 backlog를 한 곳에 모아 둔다.

M7에서는 blocker와 non-blocking follow-up을 숨기지 않고 분리해서 기록한다.

## 2. Known issues / backlog

| id | severity | found in | symptom | status | owner milestone | follow-up |
|---|---|---|---|---|---|---|
| K1 | high | `src/projects/M_GStream/renderer/subsystem/rendering_system/GaussianView.cpp:78` | `resizeFunctional(...)` / `cudaMalloc(...)` scratch growth 경로가 장시간 camera motion에서 OOM 후보로 남아 있다. | known risk | post-M7 | large-scene / long-run motion에서 별도 재현과 allocator 전략 검토 |
| K2 | high | deployment | auth, authorization, TLS, user isolation, rate limiting이 없다. | by design | deployment follow-up | reverse proxy + auth + TLS 별도 계획으로 분리 |
| K3 | medium | browser UX | shipping UI는 reference client 수준이며 reconnect/backoff, multi-user UX, mobile gestures를 보장하지 않는다. | scope-out | post-M7 | browser product UI 별도 작업 |
| K4 | medium | control concurrency | multi-controller lease / ownership 정책이 없다. | scope-out | post-M7 | controller arbitration 설계 |
| K5 | medium | stream scalability | 1-2 client target은 우선이지만 4/8 client, slow-client tuning은 미완이다. | tuning pending | post-M7 | send-path / drop-threshold / slow-client 정책 조정 |
| K6 | medium | deployment packaging | model/sample acquisition은 user-provided path 기준이며 public redistribution sample을 저장소에 포함하지 않는다. | by design | docs follow-up | license-clear sample guidance 별도 정리 |
| K7 | low | browser integration | 다른 origin에 embed하면 mixed-content/CORS 제약을 스스로 해결해야 한다. | known limitation | deployment follow-up | reverse proxy / same-origin deployment guide |

## 3. M7 blocker 기준

아래는 known issue가 아니라 M7 blocker로 취급한다.

- build failure
- install failure
- startup crash
- `/healthz` unavailable
- `/stream.mjpg` unusable
- `/control` valid pose apply failure
- invalid pose가 server crash로 이어짐
- target scenario에서 crash / hang / unbounded RSS or VRAM growth
- clean SHA 또는 environment evidence 없이 merge-ready 선언

## 4. Security reminder

현재 서버는 public Internet에 직접 노출할 준비가 되어 있지 않다.

- loopback
- trusted LAN
- trusted VPN

이외 환경은 deployment scope 밖으로 둔다.

## 5. M7 문서 관계

- 운영 절차: `docs/M_GStream_ubuntu24_remote_browser_stream_user_guide_ko.md`
- 실행 결과: `docs/M_GStream_remote_browser_stream_verification_report_ko.md`

즉, user guide는 "어떻게 실행하는가"를 다루고,
verification report는 "무엇이 실제로 검증됐는가"를 다루며,
이 문서는 "무엇이 아직 리스크로 남아 있는가"를 다룬다.
