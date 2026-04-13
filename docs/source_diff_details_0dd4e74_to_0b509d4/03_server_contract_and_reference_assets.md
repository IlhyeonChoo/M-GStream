# server contract와 reference asset 상세

이 문서는 M2에서 추가된 remote browser stream 준비물, 즉 C++ parser/adapter 계약과 브라우저 참고 자산을 정리한다.

## 디렉터리: `src/projects/extended_gaussian/renderer/server`

### 파일 묶음

- `src/projects/extended_gaussian/renderer/server/ServerProtocol.hpp`
- `src/projects/extended_gaussian/renderer/server/ServerProtocol.cpp`

#### 초기 코드

```text
기준 커밋 `0dd4e74` 에는 파일이 없었다.
```

#### 현재 코드

```cpp
struct SIBR_EXTENDED_GAUSSIAN_EXPORT ServerOptions {
    bool enabled = false;
    std::string listen_host = "127.0.0.1";
    int listen_port = 8080;
    int stream_width = 1280;
    int stream_height = 720;
    int stream_fps = 15;
};
...
ServerOptions ParseServerOptions(const CommandLineArgs& args);
ParseControlMessageResult ParseControlMessageJson(const std::string& payload);
```

```cpp
ParseControlMessageResult ParseControlMessageJson(const std::string& payload)
{
    ...
    if (type != "set_camera_pose") {
        result.error = "Unsupported control message type '" + type + "'.";
        return result;
    }
    ...
    if (!ValidateRemoteCameraPose(message.camera_pose, result.error)) {
        return result;
    }
}
```

#### 바뀐 이유

- 실제 네트워크 서버를 붙이기 전에, 브라우저와 서버가 공유할 C++ 쪽 제어 프로토콜 계약을 먼저 고정하려는 계약 우선(contract-first) 단계였다.
- `ParseServerOptions`는 나중에 `--server`, `--listen-host`, `--listen-port`, `--stream-width`, `--stream-height`, `--stream-fps`를 받을 자리를 미리 만든다.
- `ParseControlMessageJson`은 `set_camera_pose` 메시지의 wire contract를 C++ 기준으로 먼저 확정하는 역할을 맡는다.

### 파일 묶음

- `src/projects/extended_gaussian/renderer/server/CameraPoseAdapter.hpp`
- `src/projects/extended_gaussian/renderer/server/CameraPoseAdapter.cpp`

#### 초기 코드

```text
기준 커밋 `0dd4e74` 에는 파일이 없었다.
```

#### 현재 코드

```cpp
bool ValidateRemoteCameraPose(const RemoteCameraPose& pose, std::string& error);
bool TryBuildInputCamera(const RemoteCameraPose& pose, sibr::InputCamera& camera, std::string& error);
RemoteCameraPose ExportRemoteCameraPose(const sibr::InputCamera& camera);
```

```cpp
bool ValidateRemoteCameraPose(const RemoteCameraPose& pose, std::string& error)
{
    ...
    if (pose.fovy <= 0.0f || pose.fovy >= kPi) {
        error = "Camera fovy must be in the open interval (0, pi).";
        return false;
    }
    ...
    if (alignment >= kParallelThreshold) {
        error = "Camera forward and up vectors must not be parallel or near-parallel.";
        return false;
    }
}
```

#### 바뀐 이유

- parser가 JSON 구조만 맞는다고 끝나면 안 되고, 실제 camera basis로 변환 가능한 값인지도 검증해야 했다.
- 이 어댑터는 이후 WebSocket 제어가 붙었을 때 `InputCamera`와 원격 payload 사이의 경계를 담당하게 된다.
- `0b509d4` 시점에는 아직 런타임에 연결되지 않았지만, 규약을 먼저 고정해 두면 이후 서버 구현이 훨씬 단순해진다.

## 디렉터리: `src/projects/extended_gaussian/renderer/server/examples/control_messages`

### 파일 묶음

- `valid_set_camera_pose_default.json`
- `invalid_forward_zero.json`
- `invalid_forward_up_near_parallel.json`
- `invalid_fovy_out_of_range.json`
- `invalid_missing_fovy.json`
- `invalid_position_length.json`
- `invalid_trailing_content.txt`

#### 초기 코드

```text
기준 커밋 `0dd4e74` 에는 위 예제 파일들이 없었다.
```

#### 현재 코드

```json
{
  "type": "set_camera_pose",
  "position": [0.0, 0.0, 0.0],
  "forward": [0.0, 0.0, -1.0],
  "up": [0.0, 1.0, 0.0],
  "fovy": 0.78539816339
}
```

```json
{
  "type": "set_camera_pose",
  "position": [0.0, 0.0, 0.0],
  "forward": [0.0, 0.0, 0.0],
  "up": [0.0, 1.0, 0.0],
  "fovy": 0.78539816339
}
```

#### 바뀐 이유

- 새 parser/validator 계약을 수동 검증하거나 이후 테스트 fixture로 재사용할 수 있게 하려는 목적이다.
- 성공 payload 하나와 실패 payload 여러 개를 같이 둬야, 브라우저 쪽 formatter와 C++ parser가 같은 계약을 보고 움직이기 쉽다.

## 디렉터리: `src/projects/extended_gaussian/renderer/server/www`

### 파일 묶음

- `README.md`
- `index.html`
- `app.js`
- `styles.css`

#### 초기 코드

```text
기준 커밋 `0dd4e74` 에는 위 파일들이 없었다.
```

#### 현재 코드

```html
<title>Extended Gaussian Remote Stream Reference Client</title>
...
This page is a protocol reference for the future Ubuntu remote browser stream mode.
It assumes the server exposes <code>/stream.mjpg</code> and <code>/control</code>.
```

```js
const MIN_VECTOR_NORM = 1e-6;
const PARALLEL_THRESHOLD = 0.999;
...
const streamUrl = new URL("/stream.mjpg", origin).toString();
...
throw new Error("FOV Y must be in the open interval (0, pi).");
```

```md
This directory contains a protocol-reference browser client for the future Ubuntu remote browser stream mode.
It is intentionally not wired into the runtime yet.
...
## Explicit Non-Goals
- production UI flows
- authentication
- reconnect/backoff policy
- runtime packaging/install integration
```

#### 바뀐 이유

- 브라우저 쪽 실험과 프로토콜 공유를 먼저 진행하되, 실제 제품 UI/패키징/인증/재연결 정책은 이후 단계로 미루기 위해서다.
- `app.js`의 validation 규칙을 C++ parser와 맞춰 두면, 나중에 `/control` WebSocket을 붙일 때 계약 해석이 어긋날 가능성이 줄어든다.
- `README.md`에서 non-goal을 명시한 것도 같은 이유다. M2에서는 이 자산을 shipped product로 취급하지 않는다.

## 요약

- M2의 server 디렉터리 추가물은 대부분 **계약 고정과 수동 검증 보조 자산**이다.
- 이 시점의 parser/adapter/www 자산은 아직 런타임에 연결되지 않았고, M3에서 build ownership 분리, M4 이후에서 실제 서버 연결로 이어진다.
