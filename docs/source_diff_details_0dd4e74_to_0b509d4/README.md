# `0dd4e74` -> `0b509d4` 상세 코드 비교 문서

비교 기준:

- 기준 커밋: `0dd4e740b8f0b387b8284315bb5b8d5544b30f55`
- 현재 커밋: `0b509d4241f8deaea97e0023441ecf3c3821afb9`
- 비교 범위: `src/`
- 마일스톤: `M2 headless EGL`

## 문서 구성

- [01_headless_window_and_entry.md](./01_headless_window_and_entry.md)
  - `Window` backend와 app entry의 headless 실행 경로
- [02_viewer_headless_runtime.md](./02_viewer_headless_runtime.md)
  - viewer 내부의 one-shot snapshot / manifest idle / capture helper
- [03_server_contract_and_reference_assets.md](./03_server_contract_and_reference_assets.md)
  - server contract parser, camera adapter, examples, reference web assets

## 읽는 법

- 이 비교는 M2 범위를 **headless render context + finite snapshot loop + server contract prep**까지로 본다.
- 각 파일 항목은 가능한 한 아래 순서를 유지한다.
  1. `초기 코드`
  2. `현재 코드`
  3. `바뀐 이유`
- `현재 코드`는 `0b509d4` 시점 기준이다. 즉 M3에서 다시 바뀐 server export macro는 여기 반영하지 않는다.
