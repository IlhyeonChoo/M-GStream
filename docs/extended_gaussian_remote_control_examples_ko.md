# Remote Control Message Examples

작성일: 2026-04-08  
대상 브랜치: `develop/ubuntu24-remote-browser-stream`

## 1. 목적

이 문서는 `set_camera_pose` wire contract의 정상/실패 예제를
후속 브랜치가 바로 재사용할 수 있게 정리한 참조 문서다.

실제 샘플 payload 파일은 다음 디렉터리에 둔다.

- `src/projects/extended_gaussian/renderer/server/examples/control_messages/`

이 샘플들은 주로 다음 두 곳에서 재사용한다.

- future WebSocket handler 수동 검증
- browser reference client / 실제 제품 UI 구현 시 입력 계약 확인

## 2. 기준 구현

현재 기준 구현은 다음 두 함수가 결정한다.

- `ParseControlMessageJson(...)`
- `ValidateRemoteCameraPose(...)`

이 문서의 expected result는 위 구현의 현재 에러 문자열과 규칙을 기준으로 적는다.

## 3. 정상 예제

### 3.1 canonical success

파일:

- `src/projects/extended_gaussian/renderer/server/examples/control_messages/valid_set_camera_pose_default.json`

의도:

- 문서와 구현이 공통으로 쓰는 baseline payload

기대 결과:

- parse 성공
- `type == set_camera_pose`
- `position`, `forward`, `up`, `fovy`가 그대로 `RemoteCameraPose`에 반영됨

## 4. 실패 예제

### 4.1 필수 키 누락

파일:

- `src/projects/extended_gaussian/renderer/server/examples/control_messages/invalid_missing_fovy.json`

의도:

- required scalar field 누락 케이스

기대 결과:

- parse 실패
- expected error
  - `Missing required numeric field 'fovy'.`

### 4.2 배열 길이 오류

파일:

- `src/projects/extended_gaussian/renderer/server/examples/control_messages/invalid_position_length.json`

의도:

- vector field 길이가 3이 아닐 때 거부되는지 확인

기대 결과:

- parse 실패
- expected error
  - `Field 'position' must contain exactly three numbers.`

### 4.3 `fovy` 범위 오류

파일:

- `src/projects/extended_gaussian/renderer/server/examples/control_messages/invalid_fovy_out_of_range.json`

의도:

- `0 < fovy < pi` open interval 규칙 확인

기대 결과:

- parse 실패
- expected error
  - `Field 'fovy' must be in the open interval (0, pi).`

### 4.4 zero vector 오류

파일:

- `src/projects/extended_gaussian/renderer/server/examples/control_messages/invalid_forward_zero.json`

의도:

- zero vector 금지 규칙 확인

기대 결과:

- parse 실패
- expected error
  - `Camera forward vector must be non-zero.`

### 4.5 near-parallel 오류

파일:

- `src/projects/extended_gaussian/renderer/server/examples/control_messages/invalid_forward_up_near_parallel.json`

의도:

- `forward` / `up`가 평행 또는 near-parallel이면 거부되는지 확인

기대 결과:

- parse 실패
- expected error
  - `Camera forward and up vectors must not be parallel or near-parallel.`

### 4.6 trailing content 오류

파일:

- `src/projects/extended_gaussian/renderer/server/examples/control_messages/invalid_trailing_content.txt`

의도:

- JSON object 뒤에 비공백 content가 붙으면 거부되는지 확인

주의:

- 이 케이스는 의도적으로 raw payload string 예제로 저장한다.
- 형식 자체가 "valid JSON file"이 아니므로 `.txt`를 사용한다.

기대 결과:

- parse 실패
- expected error
  - `Control message must not contain trailing content.`

## 5. 사용 지침

후속 브랜치가 WebSocket handler를 붙이면,
최소한 아래 기준으로 샘플을 다시 돌리는 편이 좋다.

- 정상 예제는 서버가 성공 응답 또는 정상 카메라 업데이트를 발생시켜야 한다.
- 실패 예제는 parse 단계에서 거부되어야 하며,
  camera state를 바꾸지 않아야 한다.
- error string을 그대로 외부 API에 노출할 필요는 없지만,
  내부 로그 기준점으로는 동일 문자열을 유지하는 편이 디버깅에 유리하다.

## 6. 확장 후보

현재 follow-up 범위에는 포함하지 않았지만,
필요하면 아래 예제를 같은 디렉터리에 추가할 수 있다.

- non-finite vector entry
- unsupported `type`
- `up` zero vector
- required vector field missing
