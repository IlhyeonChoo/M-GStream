# `262de5b` -> `0dd4e74` 상세 코드 비교 문서

비교 기준:

- 기준 커밋: `262de5b0bb4637e59b05517052129593169a5ec9`
- 현재 커밋: `0dd4e740b8f0b387b8284315bb5b8d5544b30f55`
- 비교 범위: `src/`
- 마일스톤: `M1 build bootstrap`

## 문서 구성

- [01_core_portability.md](./01_core_portability.md)
  - Embree / FFmpeg / video utility portability 수정
- [02_extended_gaussian_build_and_viewer.md](./02_extended_gaussian_build_and_viewer.md)
  - `extended_gaussian` renderer build와 viewer/loader portability 수정

## 읽는 법

- 이 비교는 Ubuntu 24에서 **configure/build/install/gui smoke를 가능하게 만든 최소 portability fix**를 정리한다.
- 각 파일 항목은 가능한 한 아래 순서를 유지한다.
  1. `초기 코드`
  2. `현재 코드`
  3. `바뀐 이유`
- 코드 블록은 전체 파일이 아니라 M1에서 실제로 portability에 영향을 준 부분만 발췌한다.
