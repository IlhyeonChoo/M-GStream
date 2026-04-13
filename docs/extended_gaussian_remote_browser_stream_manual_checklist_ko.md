# Remote Browser Stream Manual Checklist

작성일: 2026-04-08  
적용 시점: M5에서 `/stream.mjpg`가 실제로 연결된 이후. `/control` WebSocket 적용 항목은 M6 이후에 사용한다.

## 1. 목적

이 저장소에는 remote browser stream용 자동 테스트 스위트가 없다.
따라서 실제 기능 브랜치에서는 수동 검증 절차를 먼저 고정해두는 편이 중요하다.

이 문서는 후속 브랜치가 실제 runtime을 붙인 뒤
최소한 무엇을 확인해야 하는지 체크리스트 형태로 정리한다.

## 2. 사전 조건

검증 전에 아래 조건을 먼저 맞춘다.

- headless 또는 onscreen renderer가 실제 프레임을 생성할 수 있어야 한다.
- M5 기준으로는 HTTP endpoint가 최소 `/`, `/stream.mjpg`, `/healthz`를 제공해야 한다.
- M6 이후에는 WebSocket endpoint가 `/control`에 실제로 열려 있어야 한다.
- M6 이후에는 viewer 또는 camera handler와 remote control 적용 경로가 실제로 연결되어 있어야 한다.
- M6 검증 시에는 테스트용 payload 샘플이 준비되어 있어야 한다.
  - `src/projects/extended_gaussian/renderer/server/examples/control_messages/`

## 3. 기본 bring-up

### 3.1 서버 시작

- 서버가 지정한 `listen_host` / `listen_port`로 실제 bind되는지 확인한다.
- 프로세스 시작 직후 fatal exit 없이 유지되는지 확인한다.
- startup log에 HTTP endpoint 정보가 남는지 확인한다.
- M6 이후에는 WebSocket endpoint 정보도 함께 확인한다.

### 3.2 health / static asset

- 브라우저에서 `/`가 열리는지 확인한다.
- reference client 또는 후속 UI가 정상 로드되는지 확인한다.
- 정적 자산 404가 없는지 확인한다.

## 4. MJPEG 스트림

### 4.1 최초 미리보기

- 브라우저에서 `/stream.mjpg`를 직접 열었을 때 이미지 스트림이 시작되는지 확인한다.
- 첫 프레임이 과도하게 오래 지연되지 않는지 확인한다.
- 연결 직후 프레임이 고정된 정지 이미지가 아니라 계속 갱신되는지 확인한다.

### 4.2 크기 / FPS 기대치

- 실제 stream frame size가 `stream_width` / `stream_height` 설정과 맞는지 확인한다.
- 프레임 전달이 `stream_fps` 기대치와 대체로 맞는지 관찰한다.
- 해상도 변경 시 stream output이 따라 바뀌는지 확인한다.

### 4.3 재연결

- 브라우저 탭 새로고침 후 스트림이 다시 붙는지 확인한다.
- 스트림 연결을 여러 번 열고 닫아도 서버가 누수 없이 유지되는지 확인한다.

### 4.4 health / metrics sanity

- `/healthz`의 `stream.active_clients`가 실제 연결 수와 맞는지 확인한다.
- `/healthz`의 `frames_captured`, `frames_published`, `latest_sequence`가 스트림 수신 중 증가하는지 확인한다.
- single client와 two-client 상황 모두에서 metrics가 비정상적으로 멈추지 않는지 확인한다.

## 5. WebSocket control (M6 이후)

### 5.1 연결 / 해제

- `/control`에 최초 WebSocket 연결이 성공하는지 확인한다.
- 클라이언트 disconnect 시 서버가 정상적으로 세션 정리를 하는지 확인한다.
- 재연결이 반복되어도 서버가 hang 또는 crash하지 않는지 확인한다.

### 5.2 정상 payload

- `valid_set_camera_pose_default.json` 전송 시 카메라가 즉시 이동하는지 확인한다.
- 여러 정상 payload를 연속 전송해도 순서대로 반영되는지 확인한다.
- 전송 직후 MJPEG preview가 새 시점으로 갱신되는지 확인한다.

## 6. 실패 payload 처리

아래 샘플은 모두 **카메라 상태를 바꾸지 않고** 거부되어야 한다.

- `invalid_missing_fovy.json`
- `invalid_position_length.json`
- `invalid_fovy_out_of_range.json`
- `invalid_forward_zero.json`
- `invalid_forward_up_near_parallel.json`
- `invalid_trailing_content.txt`

각 케이스에서 확인할 항목:

- 서버가 parse / validation 실패를 감지하는지
- 실패 이유가 내부 로그 또는 디버그 응답에 남는지
- 이전 camera pose가 그대로 유지되는지
- 이후 정상 payload를 보내면 다시 회복되는지

## 7. 장시간 / 안정성 점검

- 스트림 연결을 일정 시간 유지했을 때 frame delivery가 끊기지 않는지 확인한다.
- WebSocket을 장시간 열어둔 상태에서 payload를 간헐적으로 보내도 정상 동작하는지 확인한다.
- 스트림 클라이언트가 여러 번 붙었다 떨어져도 프로세스 메모리가 비정상적으로 증가하지 않는지 확인한다.
- 카메라를 연속 이동시킬 때 기존 known issue인 OOM 재현 여부를 별도로 기록한다.

## 8. 진단 로그에 남겨야 할 것

수동 검증 결과를 남길 때는 아래 항목을 같이 기록한다.

- commit SHA
- 실행 명령
- `listen_host`, `listen_port`, `stream_width`, `stream_height`, `stream_fps`
- 사용한 브라우저와 버전
- 사용한 payload 샘플 파일 이름
- 성공/실패 시점의 서버 로그
- 장시간 테스트 지속 시간

## 9. pass 기준

최소 pass 기준은 다음과 같다.

### M5 minimum pass

- `/stream.mjpg`가 실제로 갱신되는 프레임을 내보낸다.
- single client와 two-client 연결 모두에서 multipart JPEG part가 반복 수신된다.
- `/healthz`의 stream metrics가 실제 연결 수와 frame publish 진행을 반영한다.
- 재연결과 장시간 연결에서 즉시 crash 또는 hang가 없다.

### M6 additional pass

- `/control` WebSocket 연결/해제가 안정적으로 동작한다.
- 정상 payload는 camera update로 이어진다.
- 실패 payload는 camera state를 바꾸지 않고 거부된다.
