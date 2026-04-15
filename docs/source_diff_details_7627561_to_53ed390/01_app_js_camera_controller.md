# app.js browser camera controller 상세

이 문서는 M8에서 `app.js` 가 수동 payload formatter 수준에서 벗어나, **실시간 키보드/마우스 카메라 제어를 WebSocket `set_camera_pose` 전송으로 연결하는 client runtime** 으로 확장된 변화를 정리한다.

## 디렉터리: `src/projects/extended_gaussian/renderer/server/www`

### 파일: `src/projects/extended_gaussian/renderer/server/www/app.js`

#### 초기 코드

```javascript
const state = {
  socket: null,
};

function connectSocket() {
  ...
  socket.addEventListener("message", (event) => {
    setStatus(`WebSocket message received: ${event.data}`);
  });
}

function sendPayload() {
  const payloadText = $("payload").value.trim();
  const payload = payloadText ? payloadText : JSON.stringify(buildPayloadObject(), null, 2);
  ...
  state.socket.send(payload);
  setStatus("Control payload sent.");
}
```

초기 `app.js` 는 form 에 입력된 pose 를 JSON payload 로 만들어 한 번 전송하는 **수동 protocol reference client** 였다. 입력 이벤트를 지속 처리하는 상태 기계나 camera pose 동기화 로직은 없었다.

#### 현재 코드

```javascript
const state = {
  socket: null,
  cameraController: null,
};

const CONTROL_KEY_CODES = new Set([
  "KeyW", "KeyS", "KeyA", "KeyD",
  "KeyQ", "KeyE",
  "ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight",
]);
```

```javascript
class CameraController {
  constructor() {
    this.active = false;
    this.position = [0, 0, 0];
    this.forward = [0, 0, -1];
    this.up = [0, 1, 0];
    this.fovy = DEFAULT_FOVY;
    this.keysPressed = {};
    this.moveSpeed = DEFAULT_MOVE_SPEED;
    this.rotateSpeed = DEFAULT_ROTATE_SPEED;
    this.lastFrameTime = null;
    this.animFrameId = null;
    this.lastSendTime = 0;
    this.sendIntervalMs = DEFAULT_SEND_INTERVAL_MS;
    this.dirty = false;
    this.mouseDragging = false;
    ...
  }

  processKeys(dt) {
    ...
    if (this.keysPressed.KeyW) {
      position = vec3Add(position, vec3Scale(forward, moveAmount));
      moved = true;
    }
    ...
    if (this.keysPressed.ArrowLeft) {
      forward = rotateAroundAxis(forward, WORLD_UP, rotateAmount);
      rotated = true;
    }
    ...
    if (moved || rotated) {
      this.dirty = true;
    }
  }

  tick(timestamp) {
    ...
    this.processKeys(dt);
    if (this.dirty && timestamp - this.lastSendTime >= this.sendIntervalMs) {
      this.sendPose();
      this.syncFormFields();
      this.dirty = false;
      this.lastSendTime = timestamp;
    }
    ...
  }
}
```

```javascript
socket.addEventListener("message", (event) => {
  try {
    const message = JSON.parse(event.data);
    if (message.type === "ready") {
      if (message.camera_pose) {
        syncPoseUi(message.camera_pose);
        if (state.cameraController) {
          state.cameraController.initFromPose(message.camera_pose);
        }
      }
      setStatus("WebSocket ready. Camera pose synchronized.");
      return;
    }
    if (message.type === "error") {
      setStatus(`WebSocket error: ${message.error}`, true);
      return;
    }
    if (message.type === "ack") {
      return;
    }
  } catch (error) {
    // Ignore parse failures and fall back to the raw message text below.
  }
  setStatus(`WebSocket message received: ${event.data}`);
});
```

```javascript
function toggleCameraControl() {
  ...
  controller.initFromPose(pose);
  controller.moveSpeed = Number($("move-speed").value);
  controller.rotateSpeed = Number($("rotate-speed").value);
  controller.syncFormFields();
  controller.activate();
  setCameraControlButtonState(true);
  setStatus("Camera control enabled.");
}
```

#### 바뀐 이유

- M6/M7 시점의 `/control` 은 이미 `set_camera_pose` 와 `ready.camera_pose` 계약을 제공하지만, 브라우저는 여전히 **수동 form 전송 도구** 에 머물러 있었다.
- M8에서는 서버 프로토콜을 바꾸지 않고, 브라우저 쪽에 `CameraController` 를 추가해 입력 이벤트를 `set_camera_pose` payload 로 직접 변환하도록 했다.
- `WASD`, `Q/E`, 화살표, drag, wheel 을 지원하면서도 기존 validation (`validatePayloadObject`) 을 계속 재사용해 서버 rejection 과 클라이언트 동작이 어긋나지 않게 했다.
- `ready.camera_pose` 수신 시 form/controller 를 동기화하고, `ack` 는 status line 갱신을 생략하도록 해서 연속 제어 시 불필요한 UI 흔들림을 줄였다.
- `blur` 초기화, text-entry focus 무시, `deltaMode` 정규화, pitch clamp 를 넣은 이유는 실제 브라우저 입력에서 흔한 stuck key / excessive zoom / near-parallel pose 문제를 사전에 막기 위해서다.

## 요약

- M8에서 `app.js` 는 단순 payload formatter 에서 **실시간 browser camera runtime** 으로 바뀌었다.
- 변경의 핵심은 새 protocol 을 만드는 것이 아니라, 기존 `set_camera_pose` 계약을 사용자가 직접 조작할 수 있는 UX 로 바꾼 점이다.
