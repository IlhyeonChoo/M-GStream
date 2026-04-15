const state = {
  socket: null,
  cameraController: null,
};

const MIN_VECTOR_NORM = 1e-6;
const PARALLEL_THRESHOLD = 0.999;
const CAMERA_PITCH_ALIGNMENT_LIMIT = 0.99;
const DEFAULT_FOVY = Math.PI / 4.0;
const DEFAULT_MOVE_SPEED = 0.6;
const DEFAULT_ROTATE_SPEED = 30.0;
const DEFAULT_MOUSE_SENSITIVITY = 0.15;
const DEFAULT_SEND_INTERVAL_MS = 33;
const WHEEL_ZOOM_FACTOR = 0.002;
const WORLD_UP = [0, 1, 0];
const CONTROL_KEY_CODES = new Set([
  "KeyW",
  "KeyS",
  "KeyA",
  "KeyD",
  "KeyQ",
  "KeyE",
  "ArrowUp",
  "ArrowDown",
  "ArrowLeft",
  "ArrowRight",
]);

function $(id) {
  return document.getElementById(id);
}

function currentHttpOrigin() {
  if (window.location.origin && window.location.origin !== "null") {
    return window.location.origin;
  }
  return "http://127.0.0.1:8080";
}

function currentWsUrl() {
  try {
    const url = new URL(currentHttpOrigin());
    url.protocol = url.protocol === "https:" ? "wss:" : "ws:";
    url.pathname = "/control";
    url.search = "";
    url.hash = "";
    return url.toString();
  } catch (error) {
    return "ws://127.0.0.1:8080/control";
  }
}

function setStatus(text, isError = false) {
  const statusLine = $("status-line");
  statusLine.textContent = text;
  statusLine.dataset.error = isError ? "true" : "false";
}

function parseVectorInput(text) {
  const value = JSON.parse(text);
  if (!Array.isArray(value) || value.length !== 3) {
    throw new Error("Expected a JSON array with exactly 3 numeric values.");
  }

  return value.map((entry) => {
    const numeric = Number(entry);
    if (!Number.isFinite(numeric)) {
      throw new Error("Vector entries must be finite numbers.");
    }
    return numeric;
  });
}

function vectorNorm(vector) {
  return Math.sqrt(vector.reduce((sum, value) => sum + value * value, 0));
}

function dot(lhs, rhs) {
  return lhs.reduce((sum, value, index) => sum + value * rhs[index], 0);
}

function vec3Add(lhs, rhs) {
  return lhs.map((value, index) => value + rhs[index]);
}

function vec3Sub(lhs, rhs) {
  return lhs.map((value, index) => value - rhs[index]);
}

function vec3Scale(vector, scalar) {
  return vector.map((value) => value * scalar);
}

function vec3Normalize(vector) {
  const norm = vectorNorm(vector);
  if (norm <= MIN_VECTOR_NORM) {
    return [0, 0, 0];
  }
  return vector.map((value) => value / norm);
}

function vec3Cross(lhs, rhs) {
  return [
    lhs[1] * rhs[2] - lhs[2] * rhs[1],
    lhs[2] * rhs[0] - lhs[0] * rhs[2],
    lhs[0] * rhs[1] - lhs[1] * rhs[0],
  ];
}

function rotateAroundAxis(vector, axis, angleRad) {
  const normalizedAxis = vec3Normalize(axis);
  if (vectorNorm(normalizedAxis) <= MIN_VECTOR_NORM) {
    return vector.slice();
  }

  const cosTheta = Math.cos(angleRad);
  const sinTheta = Math.sin(angleRad);
  const term1 = vec3Scale(vector, cosTheta);
  const term2 = vec3Scale(vec3Cross(normalizedAxis, vector), sinTheta);
  const term3 = vec3Scale(normalizedAxis, dot(normalizedAxis, vector) * (1.0 - cosTheta));
  return vec3Add(vec3Add(term1, term2), term3);
}

function normalizeWheelDelta(event) {
  if (event.deltaMode === 1) {
    return event.deltaY * 16.0;
  }
  if (event.deltaMode === 2) {
    return event.deltaY * window.innerHeight;
  }
  return event.deltaY;
}

function isTextEntryTarget(target) {
  if (!(target instanceof Element)) {
    return false;
  }
  if (target.closest("input, textarea, select")) {
    return true;
  }
  return target.isContentEditable;
}

function validatePayloadObject(payloadObject) {
  if (!(payloadObject.fovy > 0 && payloadObject.fovy < Math.PI)) {
    throw new Error("FOV Y must be in the open interval (0, pi).");
  }

  const forwardNorm = vectorNorm(payloadObject.forward);
  if (forwardNorm <= MIN_VECTOR_NORM) {
    throw new Error("Forward vector must be non-zero.");
  }

  const upNorm = vectorNorm(payloadObject.up);
  if (upNorm <= MIN_VECTOR_NORM) {
    throw new Error("Up vector must be non-zero.");
  }

  const alignment = Math.abs(dot(payloadObject.forward, payloadObject.up) / (forwardNorm * upNorm));
  if (alignment >= PARALLEL_THRESHOLD) {
    throw new Error("Forward and up vectors must not be parallel or near-parallel.");
  }
}

function payloadObjectFromPose(pose, type = $("message-type").value) {
  const payloadObject = {
    type,
    position: pose.position.map((value) => Number(value)),
    forward: pose.forward.map((value) => Number(value)),
    up: pose.up.map((value) => Number(value)),
    fovy: Number(pose.fovy),
  };
  validatePayloadObject(payloadObject);
  return payloadObject;
}

function copyPoseToInputs(pose) {
  $("position").value = JSON.stringify(pose.position);
  $("forward").value = JSON.stringify(pose.forward);
  $("up").value = JSON.stringify(pose.up);
  $("fovy").value = String(pose.fovy);
}

function syncPayloadTextareaFromPose(pose) {
  const payloadObject = payloadObjectFromPose(pose);
  $("payload").value = JSON.stringify(payloadObject, null, 2);
}

function syncPoseUi(pose) {
  copyPoseToInputs(pose);
  syncPayloadTextareaFromPose(pose);
}

function setCameraControlButtonState(active) {
  const button = $("toggle-camera");
  button.textContent = active ? "Disable Camera Control" : "Enable Camera Control";
  button.classList.toggle("active", active);
}

function buildPayloadObject() {
  const fovy = Number($("fovy").value);
  if (!Number.isFinite(fovy)) {
    throw new Error("FOV Y must be a finite number.");
  }

  const payloadObject = {
    type: $("message-type").value,
    position: parseVectorInput($("position").value),
    forward: parseVectorInput($("forward").value),
    up: parseVectorInput($("up").value),
    fovy,
  };

  validatePayloadObject(payloadObject);
  return payloadObject;
}

function formatPayload() {
  const payloadObject = buildPayloadObject();
  $("payload").value = JSON.stringify(payloadObject, null, 2);
  setStatus("Payload formatted.");
}

function openStream() {
  const origin = $("http-origin").value.trim();
  const streamUrl = new URL("/stream.mjpg", origin).toString();
  $("stream-preview").src = streamUrl;
  setStatus(`Stream opened: ${streamUrl}`);
}

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
    this.lastMouseX = 0;
    this.lastMouseY = 0;
    this.mouseSensitivity = DEFAULT_MOUSE_SENSITIVITY;

    this.boundKeyDown = this.onKeyDown.bind(this);
    this.boundKeyUp = this.onKeyUp.bind(this);
    this.boundBlur = this.onBlur.bind(this);
    this.boundMouseDown = this.onMouseDown.bind(this);
    this.boundMouseMove = this.onMouseMove.bind(this);
    this.boundMouseUp = this.onMouseUp.bind(this);
    this.boundWheel = this.onWheel.bind(this);
    this.boundTick = this.tick.bind(this);
  }

  initFromPose(pose) {
    const payloadObject = payloadObjectFromPose({
      position: pose.position,
      forward: pose.forward,
      up: pose.up,
      fovy: pose.fovy,
    }, "set_camera_pose");
    const basis = this.rebuildBasis(payloadObject.forward, payloadObject.up);
    this.position = payloadObject.position.slice();
    this.forward = basis.forward;
    this.up = basis.up;
    this.fovy = payloadObject.fovy;
    this.dirty = true;
  }

  getPose() {
    return {
      position: this.position.slice(),
      forward: this.forward.slice(),
      up: this.up.slice(),
      fovy: this.fovy,
    };
  }

  rebuildBasis(forwardVector, upHint = this.up) {
    const normalizedForward = vec3Normalize(forwardVector);
    if (vectorNorm(normalizedForward) <= MIN_VECTOR_NORM) {
      throw new Error("Forward vector must be non-zero.");
    }

    let right = vec3Cross(normalizedForward, WORLD_UP);
    if (vectorNorm(right) <= MIN_VECTOR_NORM) {
      right = vec3Cross(normalizedForward, upHint);
    }
    right = vec3Normalize(right);
    if (vectorNorm(right) <= MIN_VECTOR_NORM) {
      throw new Error("Camera basis became degenerate.");
    }

    const rebuiltUp = vec3Normalize(vec3Cross(right, normalizedForward));
    if (vectorNorm(rebuiltUp) <= MIN_VECTOR_NORM) {
      throw new Error("Camera up vector became degenerate.");
    }

    return {
      forward: normalizedForward,
      up: rebuiltUp,
      right,
    };
  }

  canApplyPitch(forwardVector) {
    const normalizedForward = vec3Normalize(forwardVector);
    return Math.abs(dot(normalizedForward, WORLD_UP)) < CAMERA_PITCH_ALIGNMENT_LIMIT;
  }

  onKeyDown(event) {
    if (!this.active || isTextEntryTarget(event.target) || !CONTROL_KEY_CODES.has(event.code)) {
      return;
    }
    event.preventDefault();
    this.keysPressed[event.code] = true;
  }

  onKeyUp(event) {
    if (!CONTROL_KEY_CODES.has(event.code)) {
      return;
    }
    delete this.keysPressed[event.code];
  }

  onBlur() {
    this.keysPressed = {};
    this.mouseDragging = false;
  }

  processKeys(dt) {
    let position = this.position.slice();
    let forward = this.forward.slice();
    const up = this.up.slice();
    let moved = false;
    let rotated = false;

    const moveAmount = this.moveSpeed * dt;
    const moveRight = vec3Normalize(vec3Cross(forward, up));

    if (this.keysPressed.KeyW) {
      position = vec3Add(position, vec3Scale(forward, moveAmount));
      moved = true;
    }
    if (this.keysPressed.KeyS) {
      position = vec3Sub(position, vec3Scale(forward, moveAmount));
      moved = true;
    }
    if (this.keysPressed.KeyA && vectorNorm(moveRight) > MIN_VECTOR_NORM) {
      position = vec3Sub(position, vec3Scale(moveRight, moveAmount));
      moved = true;
    }
    if (this.keysPressed.KeyD && vectorNorm(moveRight) > MIN_VECTOR_NORM) {
      position = vec3Add(position, vec3Scale(moveRight, moveAmount));
      moved = true;
    }
    if (this.keysPressed.KeyQ) {
      position = vec3Sub(position, vec3Scale(WORLD_UP, moveAmount));
      moved = true;
    }
    if (this.keysPressed.KeyE) {
      position = vec3Add(position, vec3Scale(WORLD_UP, moveAmount));
      moved = true;
    }

    const rotateAmount = this.rotateSpeed * (Math.PI / 180.0) * dt;
    if (this.keysPressed.ArrowLeft) {
      forward = rotateAroundAxis(forward, WORLD_UP, rotateAmount);
      rotated = true;
    }
    if (this.keysPressed.ArrowRight) {
      forward = rotateAroundAxis(forward, WORLD_UP, -rotateAmount);
      rotated = true;
    }

    const pitchAxisCandidate = vec3Cross(forward, WORLD_UP);
    if (vectorNorm(pitchAxisCandidate) > MIN_VECTOR_NORM) {
      const pitchAxis = vec3Normalize(pitchAxisCandidate);
      if (this.keysPressed.ArrowUp) {
        const candidateForward = rotateAroundAxis(forward, pitchAxis, rotateAmount);
        if (this.canApplyPitch(candidateForward)) {
          forward = candidateForward;
          rotated = true;
        }
      }
      if (this.keysPressed.ArrowDown) {
        const candidateForward = rotateAroundAxis(forward, pitchAxis, -rotateAmount);
        if (this.canApplyPitch(candidateForward)) {
          forward = candidateForward;
          rotated = true;
        }
      }
    }

    if (moved) {
      this.position = position;
    }
    if (rotated) {
      const basis = this.rebuildBasis(forward, up);
      this.forward = basis.forward;
      this.up = basis.up;
    }
    if (moved || rotated) {
      this.dirty = true;
    }
  }

  onMouseDown(event) {
    if (!this.active || event.button !== 0 || event.target !== $("stream-preview")) {
      return;
    }
    this.mouseDragging = true;
    this.lastMouseX = event.clientX;
    this.lastMouseY = event.clientY;
    event.preventDefault();
  }

  onMouseMove(event) {
    if (!this.active || !this.mouseDragging) {
      return;
    }

    const deltaX = event.clientX - this.lastMouseX;
    const deltaY = event.clientY - this.lastMouseY;
    if (deltaX === 0 && deltaY === 0) {
      return;
    }

    let forward = this.forward.slice();
    const up = this.up.slice();
    const radiansPerPixel = this.mouseSensitivity * (Math.PI / 180.0);

    if (deltaX !== 0) {
      forward = rotateAroundAxis(forward, WORLD_UP, -deltaX * radiansPerPixel);
    }

    const pitchAxisCandidate = vec3Cross(forward, WORLD_UP);
    if (deltaY !== 0 && vectorNorm(pitchAxisCandidate) > MIN_VECTOR_NORM) {
      const pitchAxis = vec3Normalize(pitchAxisCandidate);
      const candidateForward = rotateAroundAxis(forward, pitchAxis, -deltaY * radiansPerPixel);
      if (this.canApplyPitch(candidateForward)) {
        forward = candidateForward;
      }
    }

    const basis = this.rebuildBasis(forward, up);
    this.forward = basis.forward;
    this.up = basis.up;
    this.lastMouseX = event.clientX;
    this.lastMouseY = event.clientY;
    this.dirty = true;
    event.preventDefault();
  }

  onMouseUp() {
    this.mouseDragging = false;
  }

  onWheel(event) {
    if (!this.active) {
      return;
    }
    event.preventDefault();
    const zoomDelta = normalizeWheelDelta(event);
    if (zoomDelta === 0) {
      return;
    }
    this.position = vec3Add(this.position, vec3Scale(this.forward, -zoomDelta * WHEEL_ZOOM_FACTOR));
    this.dirty = true;
  }

  sendPose() {
    if (!state.socket || state.socket.readyState !== WebSocket.OPEN) {
      throw new Error("WebSocket is not connected.");
    }
    const payloadObject = payloadObjectFromPose(this.getPose());
    state.socket.send(JSON.stringify(payloadObject));
  }

  syncFormFields() {
    syncPoseUi(this.getPose());
  }

  activate() {
    if (this.active) {
      return;
    }

    document.addEventListener("keydown", this.boundKeyDown);
    document.addEventListener("keyup", this.boundKeyUp);
    document.addEventListener("mousedown", this.boundMouseDown);
    document.addEventListener("mousemove", this.boundMouseMove);
    document.addEventListener("mouseup", this.boundMouseUp);
    window.addEventListener("blur", this.boundBlur);
    $("stream-preview").addEventListener("wheel", this.boundWheel, { passive: false });
    $("stream-preview").classList.add("camera-active");

    this.lastFrameTime = null;
    this.lastSendTime = 0;
    this.dirty = true;
    this.active = true;
    this.animFrameId = requestAnimationFrame(this.boundTick);
  }

  deactivate() {
    if (!this.active) {
      this.keysPressed = {};
      this.mouseDragging = false;
      return;
    }

    document.removeEventListener("keydown", this.boundKeyDown);
    document.removeEventListener("keyup", this.boundKeyUp);
    document.removeEventListener("mousedown", this.boundMouseDown);
    document.removeEventListener("mousemove", this.boundMouseMove);
    document.removeEventListener("mouseup", this.boundMouseUp);
    window.removeEventListener("blur", this.boundBlur);
    $("stream-preview").removeEventListener("wheel", this.boundWheel, { passive: false });
    if (this.animFrameId !== null) {
      cancelAnimationFrame(this.animFrameId);
    }

    this.animFrameId = null;
    this.lastFrameTime = null;
    this.keysPressed = {};
    this.mouseDragging = false;
    $("stream-preview").classList.remove("camera-active");
    this.active = false;
  }

  tick(timestamp) {
    if (!this.active) {
      return;
    }

    if (this.lastFrameTime === null) {
      this.lastFrameTime = timestamp;
    }
    const dt = Math.min((timestamp - this.lastFrameTime) / 1000.0, 0.1);
    this.lastFrameTime = timestamp;

    this.processKeys(dt);
    if (this.dirty && timestamp - this.lastSendTime >= this.sendIntervalMs) {
      try {
        this.sendPose();
        this.syncFormFields();
        this.dirty = false;
        this.lastSendTime = timestamp;
      } catch (error) {
        this.deactivate();
        setCameraControlButtonState(false);
        setStatus(error.message, true);
        return;
      }
    }

    if (this.active) {
      this.animFrameId = requestAnimationFrame(this.boundTick);
    }
  }
}

function disconnectSocket() {
  if (state.socket) {
    state.socket.close();
    state.socket = null;
  }
}

function connectSocket() {
  disconnectSocket();

  const wsUrl = $("ws-url").value.trim();
  const socket = new WebSocket(wsUrl);
  state.socket = socket;

  socket.addEventListener("open", () => {
    setStatus(`WebSocket connected: ${wsUrl}`);
  });

  socket.addEventListener("close", () => {
    setStatus("WebSocket: disconnected");
    if (state.cameraController && state.cameraController.active) {
      state.cameraController.deactivate();
      setCameraControlButtonState(false);
    }
    if (state.socket === socket) {
      state.socket = null;
    }
  });

  socket.addEventListener("error", () => {
    setStatus("WebSocket connection error.", true);
  });

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
}

function sendPayload() {
  const payloadText = $("payload").value.trim();
  const payload = payloadText ? payloadText : JSON.stringify(buildPayloadObject(), null, 2);
  $("payload").value = payload;

  if (!state.socket || state.socket.readyState !== WebSocket.OPEN) {
    throw new Error("WebSocket is not connected.");
  }

  state.socket.send(payload);
  setStatus("Control payload sent.");
}

function toggleCameraControl() {
  if (!state.socket || state.socket.readyState !== WebSocket.OPEN) {
    throw new Error("WebSocket is not connected.");
  }

  if (!state.cameraController) {
    state.cameraController = new CameraController();
  }

  const controller = state.cameraController;
  if (controller.active) {
    controller.deactivate();
    setCameraControlButtonState(false);
    setStatus("Camera control disabled.");
    return;
  }

  const pose = {
    position: parseVectorInput($("position").value),
    forward: parseVectorInput($("forward").value),
    up: parseVectorInput($("up").value),
    fovy: Number($("fovy").value),
  };
  controller.initFromPose(pose);
  controller.moveSpeed = Number($("move-speed").value);
  controller.rotateSpeed = Number($("rotate-speed").value);
  controller.syncFormFields();
  controller.activate();
  setCameraControlButtonState(true);
  setStatus("Camera control enabled.");
}

function applyDefaults() {
  $("http-origin").value = currentHttpOrigin();
  $("ws-url").value = currentWsUrl();
  $("move-speed").value = String(DEFAULT_MOVE_SPEED);
  $("rotate-speed").value = String(DEFAULT_ROTATE_SPEED);
  setCameraControlButtonState(false);
  syncPoseUi({
    position: [0, 0, 0],
    forward: [0, 0, -1],
    up: [0, 1, 0],
    fovy: DEFAULT_FOVY,
  });
  formatPayload();
}

function bindActions() {
  $("apply-defaults").addEventListener("click", applyDefaults);
  $("open-stream").addEventListener("click", () => {
    try {
      openStream();
    } catch (error) {
      setStatus(error.message, true);
    }
  });
  $("connect-ws").addEventListener("click", () => {
    try {
      connectSocket();
    } catch (error) {
      setStatus(error.message, true);
    }
  });
  $("disconnect-ws").addEventListener("click", () => {
    disconnectSocket();
    setStatus("WebSocket: disconnected");
  });
  $("format-payload").addEventListener("click", () => {
    try {
      formatPayload();
    } catch (error) {
      setStatus(error.message, true);
    }
  });
  $("send-payload").addEventListener("click", () => {
    try {
      sendPayload();
    } catch (error) {
      setStatus(error.message, true);
    }
  });
  $("toggle-camera").addEventListener("click", () => {
    try {
      toggleCameraControl();
    } catch (error) {
      setStatus(error.message, true);
    }
  });
  $("move-speed").addEventListener("input", (event) => {
    if (state.cameraController) {
      state.cameraController.moveSpeed = Number(event.target.value);
    }
  });
  $("rotate-speed").addEventListener("input", (event) => {
    if (state.cameraController) {
      state.cameraController.rotateSpeed = Number(event.target.value);
    }
  });
}

bindActions();
applyDefaults();
