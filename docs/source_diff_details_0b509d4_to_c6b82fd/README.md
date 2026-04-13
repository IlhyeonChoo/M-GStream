# `0b509d4` -> `c6b82fd` 상세 코드 비교 문서

비교 기준:

- 기준 커밋: `0b509d4241f8deaea97e0023441ecf3c3821afb9`
- 현재 커밋: `c6b82fd56b780c2d04ad170cc6359599a774f287`
- 비교 범위: `src/projects/extended_gaussian/`
- 마일스톤: `M3 server build surface`

## 문서 구성

- [01_project_build_and_server_target.md](./01_project_build_and_server_target.md)
  - top-level project option, renderer build graph, server static target
- [02_server_headers.md](./02_server_headers.md)
  - `renderer/server/*` header ownership과 export macro 분리

## 읽는 법

- 각 파일 항목은 가능한 한 아래 순서를 유지한다.
  1. `초기 코드`
  2. `현재 코드`
  3. `바뀐 이유`
- 기준 커밋에 파일이 없던 경우 `초기 코드` 는 `파일 없음` 으로 적었다.
- 코드 블록은 전체 파일이 아니라 M3의 build surface 변화가 드러나는 핵심 발췌만 담는다.
- 이 비교는 **runtime 기능 추가가 아니라 빌드 경계와 심볼 소유권 분리**에 초점을 둔다.
