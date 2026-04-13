# `738e9d3` -> `733ee7d` 상세 코드 비교 문서

비교 기준:

- 기준 커밋: `738e9d3e445940dc12ce44db7aa167d2febbad79`
- 현재 커밋: `733ee7dab29b6a5c5759dd4de7511671699c443f`
- 비교 범위: `cmake/linux/`, `src/projects/extended_gaussian/`
- 주제: `M5 후속 Linux TurboJPEG / install-runtime 정리`

## 문서 구성

- [01_turbojpeg_dependency_and_backend_selection.md](./01_turbojpeg_dependency_and_backend_selection.md)
  - Linux dependency layer에서 `TurboJPEG`를 vendored target으로 끌어올린 변화
  - `extended_gaussian_server`가 ad hoc 탐색 대신 공용 imported target을 쓰게 된 변화
- [02_linux_install_runtime_and_app_bundle.md](./02_linux_install_runtime_and_app_bundle.md)
  - Linux `install_runtime.cmake`의 install script 생성/실행 버그 수정
  - viewer app install bundle에 `libturbojpeg.so*` 와 `www/`가 실제로 들어가게 된 변화

## 읽는 법

- 이번 비교는 C++ 렌더링 로직 변경이 아니라, Linux build/install/runtime packaging 계층의 정리다.
- 각 파일 항목은 가능한 한 아래 순서를 유지한다.
  1. `초기 코드`
  2. `현재 코드`
  3. `바뀐 이유`
- 코드 블록은 전체 파일이 아니라 이번 수정의 의도가 드러나는 핵심 발췌만 담는다.
- branch progress 와 modification log에는 검증 결과를 적고, 이 디렉터리에는 왜 그 코드가 바뀌었는지를 남긴다.
