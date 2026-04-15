# `7627561` -> `53ed390` 상세 코드 비교 문서

비교 기준:

- 기준 커밋: `762756153dcf856dcb36fe871a639e8f57eb3888`
- 현재 커밋: `53ed390d70947d109b69d59fbc467451beeeafec`
- 비교 범위: `src/projects/extended_gaussian/renderer/server/www/`
- 마일스톤: `M8 browser camera control`

## 문서 구성

- [01_app_js_camera_controller.md](./01_app_js_camera_controller.md)
  - `app.js` 에 추가된 브라우저 입력 처리, pose 동기화, 30 Hz WebSocket 제어 경로
- [02_browser_control_panel_ui.md](./02_browser_control_panel_ui.md)
  - `index.html` / `styles.css` 에 추가된 Camera Control 패널과 활성 상태 UI

## 읽는 법

- 이 비교는 M8을 **서버 protocol 을 바꾸지 않고 browser reference client 위에 인터랙티브 카메라 UX 를 추가한 단계**로 본다.
- 각 항목은 가능한 한 아래 순서를 유지한다.
  1. `초기 코드`
  2. `현재 코드`
  3. `바뀐 이유`
- 코드 블록은 전체 파일이 아니라, browser camera control contract 가 실제로 바뀐 핵심 발췌만 담는다.
- branch log / modification log 는 별도 docs commit 에서 추적하고, 여기서는 `www` 코드 경계만 비교한다.
