# Ubuntu 24.04 Remote Browser Stream 계획

작성일: 2026-04-08  
작업 브랜치: `develop/ubuntu24-remote-browser-stream`

## 1. 목표

- Ubuntu 24.04 Server의 headless renderer 결과를 브라우저에서 지속적으로 볼 수 있게 만든다.
- 브라우저는 MJPEG 스트림을 받고, WebSocket으로 카메라 제어를 서버에 전달한다.
- 배포 범위는 신뢰된 LAN/VPN 내부 사용으로 한정한다.

## 2. 완료 조건

- 서버 프로세스가 `--server` 모드로 실행된다.
- 브라우저에서 `GET /stream.mjpg`로 연속 프레임을 볼 수 있다.
- 브라우저가 `WS /control`로 카메라 제어 메시지를 보내면 서버 렌더 결과가 바뀐다.
- manifest / phase / residency 로직이 server mode에서도 유지된다.
- 문서에는 다음이 포함된다.
  - 실행 명령
  - 포트와 bind host
  - 브라우저 접속 주소
  - 신뢰된 LAN/VPN 전제
  - baseline known issue

## 3. 구현 범위

### 3.1 server mode

- headless renderer 위에 서버 실행 모드를 추가한다.
- 다음 CLI를 추가한다.
  - `--server`
  - `--listen-host`
  - `--listen-port`
  - `--stream-width`
  - `--stream-height`
  - `--stream-fps`

### 3.2 HTTP / MJPEG

- 최소 HTTP endpoint를 제공한다.
  - `GET /`
  - `GET /stream.mjpg`
  - `GET /healthz`
- `stream.mjpg`는 연속 JPEG 프레임으로 응답한다.

### 3.3 WebSocket control

- `WS /control` endpoint를 제공한다.
- 1차 범위에서 지원하는 입력은 카메라 제어만 포함한다.
- 제어 메시지는 absolute camera pose 기준으로 고정한다.
  - position
  - forward
  - up
  - fovy

### 3.4 범위 제외

- phase 편집 UI
- instance 편집 UI
- 인증 / TLS
- 인터넷 공개 배포
- OOM 수정

## 4. 검증

- Ubuntu 24.04 Server
  - `--server` 실행
  - `GET /healthz` 확인
  - 브라우저에서 MJPEG 스트림 확인
  - WebSocket 카메라 제어 확인
- 장시간 실행 검증
  - 스트림 지속성
  - 카메라 이동 반복 시 메모리 증가 여부

## 5. baseline known issue

- 현재 viewer에서 카메라를 계속 이동하면 OOM이 발생할 수 있다.
- 관측 지점:
  - `src/projects/extended_gaussian/renderer/subsystem/rendering_system/GaussianView.cpp:81`
  - `sibr::resizeFunctional::<lambda>::operator()`
  - `cudaMalloc(ptr, 2 * N)`
- remote server mode에서도 장시간 카메라 이동 시 반드시 재검증한다.
- 재현 시 이번 브랜치 범위를 넘더라도 blocker로 분리 기록한다.
