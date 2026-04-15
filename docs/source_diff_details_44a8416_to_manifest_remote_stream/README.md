# `44a8416` -> `manifest remote stream` 상세 코드 비교 문서

비교 기준:

- 기준 커밋: `44a8416`
- 현재 작업 범위: manifest remote stream phase control / health surface / browser status panel
- 비교 범위:
  - `src/projects/extended_gaussian/renderer/server/ServerProtocol.{hpp,cpp}`
  - `src/projects/extended_gaussian/renderer/ExtendedGaussianViewer.{hpp,cpp}`
  - `src/projects/extended_gaussian/apps/extended_gaussianViewer/main.cpp`
  - `src/projects/extended_gaussian/renderer/server/RemoteStreamServer.{hpp,cpp}`
  - `src/projects/extended_gaussian/renderer/server/www/{index.html,app.js,styles.css}`
  - `src/projects/extended_gaussian/renderer/server/examples/control_messages/valid_set_phase_default.json`

## 문서 구성

- [01_protocol_and_viewer_phase_control.md](./01_protocol_and_viewer_phase_control.md)
  - `set_phase` protocol 추가와 viewer phase API 노출
- [02_main_and_server_health_surface.md](./02_main_and_server_health_surface.md)
  - `main.cpp` wiring, `/healthz` / `ready` / `ack` 확장, smoke payload 예제
- [03_browser_manifest_status_panel.md](./03_browser_manifest_status_panel.md)
  - browser manifest status panel, phase apply, `healthz` polling UI

## 읽는 법

- 이 비교는 remote browser stream 위에 manifest 상태 관찰과 phase 전환 경로를 추가한 단계로 본다.
- 각 문서는 가능하면 아래 순서를 유지한다.
  1. `초기 코드`
  2. `현재 코드`
  3. `바뀐 이유`
- 코드 블록은 전체 파일이 아니라 실제 contract 가 바뀐 핵심 발췌만 담는다.
