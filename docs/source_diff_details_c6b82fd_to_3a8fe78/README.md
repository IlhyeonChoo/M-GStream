# `c6b82fd` -> `3a8fe78` 상세 코드 비교 문서

비교 기준:

- 기준 커밋: `c6b82fd56b780c2d04ad170cc6359599a774f287`
- 현재 커밋: `3a8fe78626d52f94984ef3a7c8b88b56f6a0db67`
- 비교 범위: `src/projects/extended_gaussian/`
- 마일스톤: `M4 HTTP skeleton`

## 문서 구성

- [01_project_and_app_wiring.md](./01_project_and_app_wiring.md)
  - top-level CMake 순서, viewer app link/install wiring
- [02_main_and_viewer_health.md](./02_main_and_viewer_health.md)
  - app entry CLI surface, server lifecycle, viewer health getter
- [03_server_protocol_and_http_runtime.md](./03_server_protocol_and_http_runtime.md)
  - server option canonicalization, `RemoteStreamServer`, HTTP/static asset skeleton

## 읽는 법

- 이 비교는 M4 범위를 **viewer process와 같은 수명주기의 HTTP skeleton 연결**로 본다.
- 각 파일 항목은 가능한 한 아래 순서를 유지한다.
  1. `초기 코드`
  2. `현재 코드`
  3. `바뀐 이유`
- 기준 커밋에 파일이 없던 경우 `초기 코드` 는 `파일 없음` 으로 적었다.
- 코드 블록은 전체 파일이 아니라 M4의 runtime wiring 변화가 드러나는 핵심 발췌만 담는다.
- 실제 MJPEG frame delivery 와 WebSocket control session 은 아직 M4 범위에 포함하지 않는다.
