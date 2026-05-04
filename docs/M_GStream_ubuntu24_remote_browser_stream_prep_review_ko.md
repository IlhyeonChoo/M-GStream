# Ubuntu 24.04 Remote Browser Stream Prep Review

작성일: 2026-04-07

## 목적

이 문서는 `plan/2026-04-08-ubuntu24-remote-browser-stream-prep.md`와
`docs/M_GStream_modification_log_ko.md`를 기준으로,
`renderer/server/` 선행 작업 범위를 코드리뷰한 결과를 정리한다.

리뷰의 초점은 다음 네 가지다.

- 계획 문서와 실제 코드가 같은 계약을 표현하는지
- 기존 빌드/호스트 환경에서 즉시 깨지는 지점이 있는지
- 후속 브랜치가 재사용할 공용 파서/어댑터가 너무 느슨하거나 과도하게 엄격하지 않은지
- 문서에 적힌 검증 주장과 현재 저장소 상태가 일치하는지

## 검토 범위

### 기준 문서

- `plan/2026-04-08-ubuntu24-remote-browser-stream-prep.md`
- `docs/M_GStream_modification_log_ko.md`

### 코드 범위

- `src/projects/M_GStream/renderer/server/ServerProtocol.hpp`
- `src/projects/M_GStream/renderer/server/ServerProtocol.cpp`
- `src/projects/M_GStream/renderer/server/CameraPoseAdapter.hpp`
- `src/projects/M_GStream/renderer/server/CameraPoseAdapter.cpp`
- `src/projects/M_GStream/renderer/server/www/index.html`
- `src/projects/M_GStream/renderer/server/www/app.js`
- `src/projects/M_GStream/renderer/server/www/styles.css`

### 제외 범위

- 실제 HTTP / MJPEG / WebSocket 서버 구현
- headless EGL 경로
- `main.cpp`, `MGStreamViewer`, `RenderingSystem`, `GaussianView` 연결
- OOM 이슈 수정

## 리뷰 결과 요약

현재 변경은 "기존 hot path는 건드리지 않고 계약 선점용 신규 파일만 추가"라는 목표에는 대체로 맞다.
다만 그대로 머지 후보로 보기에는 다음 문제가 남아 있다.

- Windows / Visual Studio 기본 빌드 경로에서 새 코드가 바로 깨질 수 있다.
- control message parser가 trailing garbage를 허용해 wire contract가 느슨하다.
- camera pose 검증 규칙이 계획 문서보다 더 엄격하게 구현되어 문서와 코드 계약이 어긋난다.
- browser reference client가 parser 계약을 그대로 반영하지 않아 디버깅 기준점으로 쓰기 어렵다.

## 상세 findings

### 1. 높음: Windows / Visual Studio 기본 빌드와 C++ 표준이 충돌한다

증상:

- 새 함수 선언/정의가 `std::string_view`를 사용한다.
- 그런데 저장소 top-level CMake는 Windows + Visual Studio/MSBuild 경로에서 여전히 C++14를 사용한다.

근거:

- `src/projects/M_GStream/renderer/server/ServerProtocol.hpp`
  - `ParseControlMessageJson(std::string_view payload)`
- `src/projects/M_GStream/renderer/server/ServerProtocol.cpp`
  - `ParseControlMessageJson(std::string_view payload)`
- `CMakeLists.txt`
  - Windows / MSBuild 분기에서 `set(CMAKE_CXX_STANDARD 14)`

재현 메모:

- `c++ -std=gnu++14 -fsyntax-only ... ServerProtocol.cpp`로 확인 시
  `std::string_view` 미지원 오류가 발생했다.
- 같은 파일은 `c++ -std=gnu++17 -fsyntax-only ...`에서는 통과했다.

의미:

- 이번 선행 작업은 문서상으로 "기존 빌드에 포함되어 컴파일 가능해야 한다"가 완료 조건인데,
  현재 상태는 Linux/Ninja/C++17 쪽 가정에만 맞고 Windows 주 경로에는 안전하지 않다.
- 특히 이 저장소는 AGENTS 문서에서 Windows 10/11 + Visual Studio 2019를 기본 호스트로 둔다.

권장 조치:

- `std::string_view`를 `const std::string&`로 낮추거나
- 해당 타깃/전역 C++ 표준을 Windows 경로에서도 17 이상으로 끌어올리는 변경을 별도로 확정해야 한다.
- 선행 작업 범위를 "신규 파일만 추가"로 유지하려면 전자가 더 저충돌이다.

### 2. 중간: control parser가 trailing garbage를 허용한다

증상:

- parser가 JSON object 하나를 읽은 뒤 입력 끝까지 소비됐는지 확인하지 않는다.
- 그 결과 `{"type":"set_camera_pose"} trailing` 같은 payload도 성공으로 처리될 수 있다.

근거:

- `src/projects/M_GStream/renderer/server/ServerProtocol.cpp`
  - `picojson::parse(rootValue, stream)` 호출 후 추가 입력 확인 없음
- `extlibs/picojson/picojson/picojson.hpp`
  - `picojson::parse(value&, std::istream&)`는 trailing content를 자동 차단하지 않음

재현 메모:

- 간단한 `picojson` 실험에서 `{"type":"set_camera_pose"} trailing` 입력이
  빈 에러 문자열과 함께 성공 처리되는 것을 확인했다.

의미:

- 실제 WebSocket 핸들러에서 프레이밍 버그나 잘못 이어붙인 메시지가 들어와도
  parser가 정상 payload처럼 받아들일 수 있다.
- "wire format을 문서와 코드에서 동일하게 정의"한다는 선행 작업 취지와도 잘 맞지 않는다.

권장 조치:

- parse 이후 남은 비공백 문자가 있는지 검사해 명시적으로 거부한다.
- parser 검증 항목에 trailing garbage 케이스를 추가한다.

### 3. 중간: camera pose 검증 규칙이 문서보다 더 엄격하다

증상:

- 계획 문서는 `forward`, `up`에 대해 zero vector 금지와 평행 금지만 고정했다.
- 실제 구현은 정규화 후 `abs(dot) >= 0.999`면 거부한다.

근거:

- 계획 문서:
  - `plan/2026-04-08-ubuntu24-remote-browser-stream-prep.md`
  - 규칙: zero vector 금지, 평행 금지
- 구현:
  - `src/projects/M_GStream/renderer/server/CameraPoseAdapter.cpp`
  - `kParallelThreshold = 0.999f`
  - `alignment >= kParallelThreshold`면 실패

의미:

- 구현은 "정확히 평행"뿐 아니라 "거의 평행"도 거부한다.
- 후속 브랜치가 계획 문서만 보고 클라이언트를 만들면,
  문서상 유효해 보이는 입력이 코드에서는 거부될 수 있다.

권장 조치:

- 문서를 구현에 맞춰 "near-parallel 금지"로 올리거나
- 구현을 문서 표현에 맞춰 더 직접적인 평행 판정으로 완화해야 한다.
- 후속 브랜치와의 계약 고정이 목적이라면 둘 중 하나로 반드시 수렴시켜야 한다.

### 4. 낮음: browser reference client가 parser 계약을 충분히 반영하지 않는다

증상:

- 브라우저 클라이언트는 숫자/배열 길이 수준만 확인한다.
- parser가 요구하는 `0 < fovy < pi`, zero vector 금지, near/parallel 금지와 같은 제약은 반영하지 않는다.

근거:

- `src/projects/M_GStream/renderer/server/www/app.js`
  - `parseVectorInput(...)`
  - `buildPayloadObject(...)`

의미:

- 이 페이지는 "최종 UI"가 아니라 "protocol reference"가 목적이라고 문서에 적혀 있다.
- 그렇다면 parser와 동일한 실패 기준을 보여주는 편이 더 유용하다.
- 현재 상태에서는 브라우저에서 만든 payload가 서버 parser에서는 거부될 수 있다.

권장 조치:

- 최소한 다음 검증을 JS 쪽에도 넣는 편이 좋다.
  - `fovy` open interval 검사
  - zero vector 검사
  - near/parallel 검사
- 또는 명시적으로 "이 페이지는 formatter만 제공하고 최종 validation은 서버 parser 기준"이라고 적어 혼동을 줄인다.

## 문서/검증 관련 메모

### 수정 로그 문서에 대한 판단

`docs/M_GStream_modification_log_ko.md`의 2026-04-08 후속 메모는
이번 선행 작업의 목적, 범위 제외, 후속 재사용 포인트를 비교적 명확하게 적고 있다.

다만 아래 항목은 과신하면 안 된다.

- "신규 C++ 파일은 `gnu++17` 문법 검증했다"는 서술은 사실일 수 있으나,
  저장소의 기본 호스트 경로인 Windows / Visual Studio 호환성을 대체하지는 못한다.
- "기존 빌드에 포함되어 컴파일 가능해야 한다"는 계획 기준을 충족했다고 보기는 어렵다.

### 현재 워크스페이스 상태

- `build-ninja/` 디렉터리는 존재하지만 `build.ninja`는 없어 직접 빌드를 돌릴 수 없었다.
- 따라서 전체 `M_GStream` 타깃 빌드 성공 여부는 이번 리뷰에서 재확인하지 못했다.
- 대신 다음 검증만 수행했다.
  - 정적 코드 검토
  - `picojson` trailing garbage 재현 실험
  - `ServerProtocol.cpp`에 대한 C++14 / C++17 문법 차이 확인

## 권장 후속 조치

1. 가장 먼저 `std::string_view`와 Windows C++14 충돌을 해소한다.
2. `ParseControlMessageJson(...)`에 trailing garbage 거부를 추가한다.
3. camera pose validation의 near-parallel 정책을 문서와 코드 중 한쪽 기준으로 고정한다.
4. browser reference client에 parser와 같은 최소 validation을 반영하거나,
   validation 범위를 명시적으로 축소한다고 문서화한다.
5. 가능하면 실제 configure 가능한 환경에서 `M_GStream` 타깃 빌드 확인을 다시 남긴다.

## 결론

이번 선행 작업은 "기능 활성화 전 공용 계약과 참조 자산을 미리 추가한다"는 방향 자체는 적절하다.
하지만 현재 상태를 그대로 baseline contract로 간주하면,
후속 브랜치가 다음 두 리스크를 그대로 상속받게 된다.

- 플랫폼별 빌드 불일치
- parser / 문서 / reference client 사이의 계약 불일치

따라서 머지 전 최소한 다음 둘은 정리하는 편이 안전하다.

- C++ 표준 호환성 문제
- parser 계약의 엄격도와 문서 표현 불일치
