# Remote Camera Contract

작성일: 2026-04-08  
기준 구현:

- `src/projects/extended_gaussian/renderer/server/ServerProtocol.hpp`
- `src/projects/extended_gaussian/renderer/server/ServerProtocol.cpp`
- `src/projects/extended_gaussian/renderer/server/CameraPoseAdapter.hpp`
- `src/projects/extended_gaussian/renderer/server/CameraPoseAdapter.cpp`

## 1. 목적

이 문서는 remote browser stream 브랜치가 사용할 camera control payload의 의미를
구현 기준으로 고정하기 위한 계약 문서다.

후속 브랜치가 브라우저 UI, WebSocket handler, camera sync 로직을 따로 구현하더라도
아래 의미는 동일하게 유지하는 것을 목표로 한다.

## 2. payload shape

현재 지원하는 control message는 1종이다.

- `type = "set_camera_pose"`

shape:

```json
{
  "type": "set_camera_pose",
  "position": [0.0, 0.0, 0.0],
  "forward": [0.0, 0.0, -1.0],
  "up": [0.0, 1.0, 0.0],
  "fovy": 0.78539816339
}
```

## 3. 각 필드의 의미

### 3.1 `position`

- camera origin의 world-space 위치
- 길이 3의 finite numeric array

### 3.2 `forward`

- camera가 바라보는 world-space 방향 벡터
- 길이 자체는 중요하지 않고 방향만 사용한다.
- zero vector는 허용하지 않는다.

### 3.3 `up`

- camera의 up reference vector
- world up을 의미하는 고정 축이 아니라,
  camera basis를 구성할 때 사용할 입력 벡터다.
- zero vector는 허용하지 않는다.

### 3.4 `fovy`

- vertical field of view
- 단위는 radians
- 허용 범위는 open interval `(0, pi)`

## 4. 좌표계 해석

이 문서의 좌표계 설명은 `TryBuildInputCamera(...)` 구현에서
입력 벡터를 추가 축 변환 없이 바로 `setLookAt(position, position + forward, correctedUp)`에
전달하는 현재 동작을 기준으로 한다.

즉, 현재 계약 해석은 다음과 같다.

- `position`, `forward`, `up`는 **viewer scene이 사용하는 것과 같은 world-space 기준**으로 해석한다.
- 별도의 left-handed / right-handed 변환이나 축 swap은 현재 adapter에 없다.

위 해석은 구현 기반 설명이며,
후속 브랜치가 다른 좌표계를 쓰는 입력 장치를 붙일 경우에는
클라이언트 측에서 이 계약으로 변환한 뒤 전송해야 한다.

## 5. validation 규칙

현재 구현은 아래 조건을 모두 만족해야 payload를 유효한 camera pose로 본다.

- `position`, `forward`, `up`의 각 성분은 finite여야 한다.
- `fovy`는 finite여야 한다.
- `0 < fovy < pi`
- `forward.norm() > 1e-6`
- `up.norm() > 1e-6`
- `abs(dot(normalize(forward), normalize(up))) < 0.999`

즉, `forward`와 `up`는 정확히 평행한 경우뿐 아니라
**near-parallel**인 경우도 거부한다.

## 6. `RemoteCameraPose -> InputCamera` 변환 규칙

`TryBuildInputCamera(...)`는 아래 순서로 camera basis를 구성한다.

1. `forward` 정규화
2. `up` 정규화
3. `right = normalize(cross(forward, up))`
4. `corrected_up = normalize(cross(right, forward))`
5. `setLookAt(position, position + forward, corrected_up)`

즉, 원래 입력 `up` 벡터가 그대로 보존되는 것이 아니라,
직교화된 `corrected_up`가 실제 `InputCamera`에 반영된다.

## 7. projection 관련 규칙

adapter는 pose와 `fovy` 외에 아래 projection 속성도 함께 다룬다.

- `aspect`
- `znear`
- `zfar`

규칙:

- 기존 `camera.aspect()`가 finite이고 양수면 유지
- 아니면 `aspect = 1.0`
- 기존 `camera.znear()`가 finite이고 양수면 유지
- 아니면 `znear = 0.1`
- 기존 `camera.zfar()`가 finite이고 `zfar > znear`면 유지
- 아니면 `zfar = 1000.0`

## 8. `InputCamera -> RemoteCameraPose` 내보내기 규칙

`ExportRemoteCameraPose(...)`는 현재 `InputCamera`에서 다음 값을 그대로 읽어낸다.

- `position = camera.position()`
- `forward = camera.dir()`
- `up = camera.up()`
- `fovy = camera.fovy()`

즉, 브라우저 쪽 "copy current pose" 또는 state sync 기능이 붙으면
이 함수 결과를 그대로 control payload reference로 사용할 수 있다.

## 9. 클라이언트 구현 시 주의점

- 브라우저 입력 장치가 yaw/pitch만 제공하더라도,
  서버에는 반드시 `forward`와 `up`를 모두 보내야 한다.
- 입력 장치가 world up 하나만 가진 경우에도,
  최종 전송 전에는 현재 camera orientation 기준의 `up`를 계산하는 편이 안전하다.
- `fovy`는 degree가 아니라 radians다.
- trailing content가 붙은 raw payload는 parser에서 거부된다.

## 10. 지금 고정하는 결론

- remote camera payload는 단순 위치 전송이 아니라
  **world-space pose + up reference + radians fovy** 계약이다.
- adapter는 입력 벡터를 그대로 쓰지 않고
  직교 기저로 정리한 뒤 `InputCamera`에 반영한다.
- 후속 브랜치는 이 의미 계약을 유지한 상태에서만
  브라우저 UI, WebSocket handler, camera sync 로직을 올려야 한다.
