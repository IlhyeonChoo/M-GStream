# `733ee7d` -> `d30707f` 상세 코드 비교 문서

비교 기준:

- 기준 커밋: `733ee7dab29b6a5c5759dd4de7511671699c443f`
- 현재 커밋: `d30707f079665f06ebc99ba6d0ba4b780a3377e2`
- 비교 범위: `src/projects/extended_gaussian/`
- 마일스톤: `M6 WebSocket control`

## 문서 구성

- [01_main_and_viewer_camera_apply.md](./01_main_and_viewer_camera_apply.md)
  - `main.cpp` 와 `ExtendedGaussianViewer` 에 추가된 camera export / main-thread apply 경로
- [02_remote_stream_server_websocket_control.md](./02_remote_stream_server_websocket_control.md)
  - `RemoteStreamServer` 의 WebSocket upgrade, latest-only mailbox, health/control metrics

## 읽는 법

- 이 비교는 M6 범위를 **브라우저 control contract 를 viewer main/update loop 에 실제로 연결하는 단계**로 본다.
- 각 파일 항목은 가능한 한 아래 순서를 유지한다.
  1. `초기 코드`
  2. `현재 코드`
  3. `바뀐 이유`
- 코드 블록은 전체 파일이 아니라 M6의 control wiring 변화가 드러나는 핵심 발췌만 담는다.
- reference client 문서 갱신은 별도 docs commit 에서 다루고, 여기서는 코드 경로만 비교한다.
