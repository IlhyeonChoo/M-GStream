const state = {
  socket: null,
};

const MIN_VECTOR_NORM = 1e-6;
const PARALLEL_THRESHOLD = 0.999;

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
    if (state.socket === socket) {
      state.socket = null;
    }
  });

  socket.addEventListener("error", () => {
    setStatus("WebSocket connection error.", true);
  });

  socket.addEventListener("message", (event) => {
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

function applyDefaults() {
  $("http-origin").value = currentHttpOrigin();
  $("ws-url").value = currentWsUrl();
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
}

bindActions();
applyDefaults();
