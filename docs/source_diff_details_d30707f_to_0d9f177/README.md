# `d30707f` -> `0d9f177` 상세 코드 비교 문서

비교 기준:

- 기준 커밋: `d30707f079665f06ebc99ba6d0ba4b780a3377e2`
- 현재 커밋: `0d9f177bd19071b5ad417e3fb915e0ef3d5ea31d`
- 비교 범위: `src/projects/extended_gaussian/`, `tools/remote_stream/`
- 마일스톤: `M7 integration verification`

## 문서 구성

- [01_main_control_timing_and_visible_sequence.md](./01_main_control_timing_and_visible_sequence.md)
  - `main.cpp` 에서 control apply timing 과 visible control sequence 를 render submit 경로로 연결한 변화
- [02_remote_stream_server_verification_metrics.md](./02_remote_stream_server_verification_metrics.md)
  - `RemoteStreamServer` 에 추가된 health summary, timing aggregation, MJPEG part tracing header
- [03_mjpeg_streamer_timing_headers.md](./03_mjpeg_streamer_timing_headers.md)
  - `MjpegStreamer` 가 raw-readback/encode/encoded 결과를 계측하고 per-frame metadata 로 내보내도록 바뀐 변화
- [04_remote_stream_measurement_tools.md](./04_remote_stream_measurement_tools.md)
  - M7 검증을 위해 추가된 `tools/remote_stream/*` helper 들의 역할과 코드 경계

## 읽는 법

- 이 비교는 M7을 **기능 추가 단계가 아니라, M1-M6 runtime 을 수치와 증거로 고정하기 위한 계측 단계**로 본다.
- 각 항목은 가능한 한 아래 순서를 유지한다.
  1. `초기 코드`
  2. `현재 코드`
  3. `바뀐 이유`
- 코드 블록은 전체 파일이 아니라, M7에서 verification contract 가 실제로 달라진 핵심 발췌만 담는다.
- verification report, user guide, known issues 같은 문서 자체는 이 디렉터리 밖의 docs commit 에서 추적하고, 여기서는 코드/도구 경로만 비교한다.
