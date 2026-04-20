# `develop/ubuntu24-remote-browser-stream` 후속 저충돌 작업 계획

작성일: 2026-04-08  
기준 브랜치: `develop/ubuntu24-remote-browser-stream`

## 1. 문서 목적

이 문서는 현재 브랜치에서 이미 추가한 server prep 작업 이후,
추가로 진행할 수 있는 작업을 **충돌 가능성 기준으로 재분류**하기 위한 계획 문서다.

핵심 질문은 다음 두 가지다.

- 지금 이 브랜치에서 더 해둘 가치가 있는 작업이 무엇인가
- 그 작업을 먼저 해두더라도 이후 다른 브랜치의 실제 구현이 끝난 뒤 머지할 때
  충돌이 크게 늘어나지 않는 이유가 무엇인가

즉, 이 문서는 "기능 구현 계획"이 아니라
"현재 브랜치에서 안전하게 선점 가능한 후속 작업 계획"을 정리한다.

## 2. 현재 상태 요약

현재 브랜치에서는 이미 다음 선행 작업을 마쳤다.

- server 공용 옵션 구조 정의
- browser control message wire contract 정의
- remote camera pose와 `sibr::InputCamera` 간 변환 유틸 정의
- browser reference client 초안 추가
- 리뷰 문서 지적 반영

반대로 아직 구현하지 않은 핵심 기능은 다음과 같다.

- 실제 `--server` 실행 분기
- headless EGL / offscreen renderer 연결
- HTTP endpoint 구현
- MJPEG 스트리밍 구현
- WebSocket endpoint 구현
- viewer / render loop와의 실제 연결

따라서 지금 시점에서 의미 있는 후속 작업은
"실제 runtime을 건드리지 않으면서 후속 브랜치가 재사용할 기준과 검증 자산을 늘리는 일"에 한정된다.

## 3. 작업 선별 기준

이 문서에서 "지금 해도 된다"고 보는 작업은 아래 조건을 만족해야 한다.

- 기존 hot path 파일을 건드리지 않는다.
- `main.cpp`, `ExtendedGaussianViewer`, `RenderingSystem`, `GaussianView`를 수정하지 않는다.
- 기존 CMake / install / packaging 경로를 되도록 건드리지 않는다.
- 새 문서, 새 샘플, 새 픽스처, 새 독립 유틸 같은 **추가형 변경**으로 끝난다.
- 후속 브랜치가 그대로 가져다 쓰거나 기준 문서로 참조할 수 있다.

반대로 아래 조건에 걸리면 지금 브랜치에서 선행 작업으로 처리하지 않는다.

- 런타임 분기와 직접 연결된다.
- headless renderer나 네트워크 서버의 실제 생명주기를 결정한다.
- 기존 실행 경로와 렌더링 루프를 수정해야 한다.
- 다른 브랜치가 거의 확실히 같은 파일을 동시에 수정할 가능성이 높다.

## 4. 지금 추가로 진행 가능한 작업

### 4.1 Ubuntu 24.04 configure / toolchain 이슈 재현 문서화

#### 작업 내용

다음 내용을 별도 문서로 정리한다.

- 현재 Ubuntu 24.04 서버에서 `cmake -S . -B build-ninja -G Ninja`가 실패하는 정확한 지점
- 실패 로그의 핵심 부분
- 원인이 이번 브랜치 코드가 아니라 기존 CUDA compiler identification 단계라는 점
- 이후 headless / server 브랜치가 확인해야 할 환경 변수와 버전 조합

#### 필요한 이유

현재 브랜치에서 가장 큰 외부 블로커는 기능 코드가 아니라 툴체인 상태다.
이 정보를 문서로 먼저 고정해두면,
다음 브랜치가 headless EGL 또는 server runtime 작업을 시작할 때
"새로 넣은 코드가 문제인지, 기존 환경이 문제인지"를 빠르게 분리할 수 있다.

#### 충돌 가능성이 낮은 이유

이 작업은 `docs/` 아래 신규 문서 추가만으로 끝낼 수 있다.
핵심 렌더러 코드, app entry, runtime wiring 파일을 수정하지 않으므로
다른 브랜치가 같은 파일을 수정할 가능성이 거의 없다.

#### 추천 산출물

- `docs/M_GStream_ubuntu24_toolchain_status_ko.md`

### 4.2 control message 정상 / 실패 예제 픽스처 정리

#### 작업 내용

`set_camera_pose` 계약에 대해 다음 예제를 별도 샘플 파일이나 문서로 정리한다.

- 정상 payload 예제
- 필수 키 누락 예제
- 배열 길이 오류 예제
- `fovy` 범위 오류 예제
- zero vector 예제
- parallel / near-parallel 예제
- trailing content 예제

#### 필요한 이유

지금은 parser 코드와 문서 설명만 존재한다.
후속 브랜치에서 WebSocket handler나 브라우저 UI를 구현할 때
"어떤 입력이 성공하고 어떤 입력이 실패해야 하는가"를
테스트 벡터 형태로 바로 참조할 수 있으면 계약이 흔들릴 가능성이 줄어든다.

#### 충돌 가능성이 낮은 이유

이 작업은 새 JSON 샘플 파일 또는 새 문서 추가로 해결할 수 있다.
기존 parser 구현 자체를 수정할 필요가 없고,
후속 브랜치는 이 샘플을 소비만 하면 되므로 merge 충돌 가능성이 낮다.

#### 추천 산출물

- `docs/M_GStream_remote_control_examples_ko.md`
- 또는 `src/projects/M_GStream/renderer/server/examples/` 아래 JSON 샘플 파일

### 4.3 브라우저 스트림 수동 검증 체크리스트 작성

#### 작업 내용

실제 `/stream.mjpg` 및 `/control` 구현이 붙은 뒤 사람이 확인해야 할 절차를 정리한다.

예를 들면 다음 항목을 포함한다.

- 브라우저에서 MJPEG preview가 열리는지
- WebSocket 연결/해제가 정상인지
- 정상 payload 전송 시 카메라가 이동하는지
- invalid payload 전송 시 parser 오류가 노출되는지
- 장시간 연결 시 frame delivery가 유지되는지

#### 필요한 이유

이 저장소에는 테스트 스위트가 없다.
결국 실제 기능 브랜치에서는 수동 검증이 중심이 되므로,
검증 절차를 먼저 문서화해두는 것은 후속 구현 품질에 직접 도움이 된다.

#### 충돌 가능성이 낮은 이유

검증 체크리스트는 순수 문서 변경이다.
실제 네트워크 서버 구현이나 렌더링 코드를 건드리지 않으므로
다른 브랜치와 충돌할 이유가 거의 없다.

#### 추천 산출물

- `docs/M_GStream_remote_browser_stream_manual_checklist_ko.md`

### 4.4 카메라 좌표계 / 의미 계약 명시

#### 작업 내용

`position`, `forward`, `up`, `fovy`가 의미하는 바를 별도 문서로 고정한다.

정리 대상은 다음과 같다.

- 벡터의 기준 좌표계
- `forward`가 시선 방향이라는 점
- `up`이 월드 업 벡터가 아니라 카메라 업 기준이라는 점
- `fovy`가 radians라는 점
- `RemoteCameraPose -> InputCamera` 변환 시 기저를 직교화한다는 점

#### 필요한 이유

실제 브라우저 입력 장치나 다른 클라이언트가 붙기 시작하면
가장 먼저 흔들리는 부분이 좌표계 해석이다.
지금은 구현 코드에만 규칙이 있으므로,
후속 브랜치가 UI와 runtime을 연결하기 전에 의미 계약을 문서로 분리해둘 필요가 있다.

#### 충돌 가능성이 낮은 이유

이 작업 역시 문서 추가만으로 끝날 수 있다.
카메라 어댑터 코드 자체를 다시 고치지 않고도
후속 브랜치의 해석 기준을 먼저 고정할 수 있다.

#### 추천 산출물

- `docs/M_GStream_remote_camera_contract_ko.md`

## 5. 조건부로 가능한 작업

아래 작업은 아예 불가능한 것은 아니지만,
지금 브랜치에서 선행 작업으로 넣을 때는 범위를 매우 조심해야 한다.

### 5.1 browser reference client 사용 문구 / 링크 정리

#### 작업 내용

현재 `renderer/server/www/`의 참조 클라이언트에 대해,
무엇을 가정하고 무엇은 아직 미구현인지 더 분명하게 설명하는 문서 또는 README를 추가한다.

#### 필요한 이유

후속 브랜치가 이 파일들을 실제 정적 자산으로 승격할 때,
"이 파일이 최종 제품 UI가 아니라 참조용 초안"이라는 맥락이 유지되어야 한다.

#### 왜 조건부인가

이 작업 자체는 저충돌이지만,
나중에 다른 브랜치가 `www/index.html`을 실제 제품 UI로 바꾸기 시작하면
동일 파일을 직접 수정하게 될 가능성이 있다.
따라서 가급적 `www/README.md`나 별도 문서 추가 방식이 낫고,
기존 HTML/CSS/JS를 다시 크게 손보는 쪽은 지금 단계에서 피하는 편이 안전하다.

## 6. 지금은 미루는 것이 맞는 작업

아래 작업은 실제 구현에는 필요하지만,
현재 브랜치에서 먼저 진행하면 충돌 가능성이 높아지는 쪽이다.

### 6.1 `main.cpp`의 `--server` 실행 분기 연결

#### 필요한 이유

최종적으로는 필요하다.
하지만 entry point는 다른 브랜치의 headless / runtime 작업도 거의 확실히 같이 건드리게 된다.

#### 지금 미루는 이유

`main.cpp`는 충돌 빈도가 높은 파일이다.
지금 여기서 선제적으로 수정하면,
후속 브랜치의 실행 경로 변경과 쉽게 겹친다.

### 6.2 `ExtendedGaussianViewer` / `RenderingSystem` / `GaussianView`와의 실제 연결

#### 필요한 이유

브라우저 제어가 실제 카메라와 렌더링 결과에 반영되려면 필수다.

#### 지금 미루는 이유

이 파일들은 현재 저장소에서 가장 load-bearing한 경로다.
headless renderer 브랜치와 remote stream 브랜치가 동시에 같은 파일을 수정할 가능성이 매우 높다.
따라서 지금 단계의 목표인 "저충돌 선행 작업"과 맞지 않는다.

### 6.3 headless EGL / offscreen renderer 연결

#### 필요한 이유

Ubuntu 24.04 Server에서 실제 렌더링을 하려면 결국 필요하다.

#### 지금 미루는 이유

이 작업은 플랫폼, 렌더 타깃, GL context 수명주기, frame capture 경로를 모두 결정한다.
즉, 다른 브랜치와 독립적으로 선점하기 어렵고,
실제 구현 브랜치의 핵심 설계와 직접 겹친다.

### 6.4 HTTP / MJPEG / WebSocket 서버 구현

#### 필요한 이유

브라우저 스트리밍 기능의 본체다.

#### 지금 미루는 이유

이 작업은 단독으로 존재하지 않는다.
headless renderer, frame capture, camera update loop와 동시에 맞물린다.
따라서 공용 계약 수준을 넘어서면 곧바로 runtime 통합 단계로 들어가게 되어
다른 브랜치와 충돌 가능성이 높아진다.

## 7. 권장 우선순위

현재 브랜치에서 추가로 작업한다면 다음 순서가 가장 합리적이다.

1. Ubuntu 24.04 toolchain / configure 실패 상태 문서화
2. control message 정상 / 실패 예제 픽스처 정리
3. 수동 검증 체크리스트 작성
4. 카메라 좌표계 / 의미 계약 문서화
5. 필요하면 `www/README.md` 형태의 reference client 설명 보강

이 순서는 다음 이유로 적절하다.

- 먼저 환경 블로커를 분리해두면 이후 실패 원인 판단이 빨라진다.
- 그 다음 계약 예제를 고정하면 runtime 구현 브랜치가 흔들릴 여지가 줄어든다.
- 검증 체크리스트와 좌표계 문서는 실제 통합 단계의 시행착오를 줄인다.

## 8. 결론

현재 브랜치에서 더 할 수 있는 작업은 분명히 있다.
다만 그 대부분은 **문서, 픽스처, 검증 기준**을 보강하는 일이다.

이 방향이 적절한 이유는 다음과 같다.

- 이미 공용 parser / adapter / reference client는 준비되어 있다.
- 이제 남은 고가치 저충돌 작업은 "기능 추가"보다 "해석과 검증 기준 고정"에 가깝다.
- 이런 작업은 주로 새 문서나 새 샘플 파일로 끝나므로,
  다른 브랜치가 실제 runtime 구현을 마친 뒤 머지할 때 충돌이 발생할 확률이 낮다.

반대로 실제 server mode, headless renderer, HTTP / MJPEG / WebSocket 구현은
이제부터는 기존 실행 경로와 hot path를 직접 수정해야 하므로,
현재 브랜치의 후속 저충돌 작업 범위에서는 제외하는 것이 맞다.
