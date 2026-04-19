const state = {
  socket: null,
  cameraController: null,
  healthPollTimer: null,
  loadWatchTimer: null,
  statusError: false,
  manifestInfo: null,
  browse: {
    currentPath: "",
    parentPath: "",
    entries: [],
    selectedEntry: null,
    pendingLoadSequence: 0,
  },
  rendererStatus: {
    content_loaded: false,
    loaded_source_kind: "",
    loaded_source_path: "",
    load_state: "idle",
    last_load_error: "",
    last_load_sequence: 0,
  },
};

const MIN_VECTOR_NORM = 1e-6;
const PARALLEL_THRESHOLD = 0.999;
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
  "KeyI",
  "KeyJ",
  "KeyK",
  "KeyL",
  "KeyU",
  "KeyO",
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

function configuredHttpOrigin() {
  const value = $("http-origin").value.trim();
  return value || currentHttpOrigin();
}

function setStatus(text, isError = false) {
  state.statusError = Boolean(isError);
  const statusLine = $("status-line");
  statusLine.textContent = text;
  statusLine.dataset.error = isError ? "true" : "false";
  const chipText = $("status-chip-text");
  if (chipText) {
    chipText.textContent = text;
  }
  updateStatusChip();
}

function updateStatusChip() {
  const chip = document.getElementById("status-chip");
  if (!chip) return;
  const ok = !state.statusError && state.socket && state.socket.readyState === WebSocket.OPEN;
  chip.classList.remove("topbar__chip--status--live", "topbar__chip--status--down"); chip.classList.add(ok ? "topbar__chip--status--live" : "topbar__chip--status--down");
}

function setBrowseStatus(text, isError = false) {
  const statusLine = $("browse-status");
  statusLine.textContent = text;
  statusLine.dataset.error = isError ? "true" : "false";
}

function setCameraControlButtonState(active) {
  const button = $("toggle-camera");
  button.textContent = active ? "Disable Camera Control" : "Enable Camera Control";
  button.classList.toggle("active", active);
}

function setHealthPollButtonState(active) {
  const button = $("toggle-health-poll");
  button.textContent = active ? "Stop Health Poll" : "Start Health Poll";
  button.classList.toggle("active", active);
}

function setLoadButtonState(disabled, label = "Load Selected") {
  const button = $("load-selected");
  button.disabled = disabled;
  button.textContent = label;
}

function formatBytes(bytes) {
  const numeric = Number(bytes);
  if (!Number.isFinite(numeric) || numeric < 0) {
    return "--";
  }
  return `${(numeric / (1024 * 1024)).toFixed(1)} MB`;
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

function fallbackUpHint(forwardVector) {
  const candidates = [
    WORLD_UP,
    [1, 0, 0],
    [0, 0, 1],
  ];
  let bestCandidate = WORLD_UP;
  let bestCrossNorm = -1.0;

  candidates.forEach((candidate) => {
    const crossNorm = vectorNorm(vec3Cross(forwardVector, candidate));
    if (crossNorm > bestCrossNorm) {
      bestCrossNorm = crossNorm;
      bestCandidate = candidate;
    }
  });

  return bestCandidate.slice();
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

function basenameOf(path) {
  if (typeof path !== "string" || !path.length) return "--";
  const trimmed = path.replace(/[\/]+$/, ""); const idx = Math.max(trimmed.lastIndexOf("/"), trimmed.lastIndexOf("\\"));
  const base = idx >= 0 ? trimmed.slice(idx + 1) : trimmed;
  return base || "--";
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
  const prev = document.getElementById("live-pose-preview"); if (prev) prev.textContent = $("payload").value;
}

function syncPoseUi(pose) {
  copyPoseToInputs(pose);
  syncPayloadTextareaFromPose(pose);
}

function updateManifestPanel(info) {
  const panel = $("manifest-panel");
  const select = $("phase-select");
  const custom = $("phase-custom");
  const totalAssets = $("total-assets");

  state.manifestInfo = info || null;
  if (!info || !info.has_manifest) {
    panel.style.display = "none";
    select.innerHTML = '<option value="">(none)</option>';
    select.value = "";
    custom.value = "";
    totalAssets.value = "--";
    clearHealthPoll();
    updateStreamingStats(null);
    return;
  }

  panel.style.display = "";
  const availablePhases = Array.isArray(info.available_phases) ? info.available_phases : [];
  const currentPhase = typeof info.current_phase === "string" ? info.current_phase : "";
  const uniquePhases = [];
  const seen = new Set();
  availablePhases.forEach((phase) => {
    if (typeof phase !== "string" || seen.has(phase)) {
      return;
    }
    seen.add(phase);
    uniquePhases.push(phase);
  });

  select.innerHTML = '<option value="">(none)</option>';
  uniquePhases.forEach((phase) => {
    const option = document.createElement("option");
    option.value = phase;
    option.textContent = phase;
    select.appendChild(option);
  });

  if (currentPhase && seen.has(currentPhase)) {
    select.value = currentPhase;
    custom.value = "";
  } else {
    select.value = "";
    custom.value = currentPhase;
  }

  totalAssets.value = Number.isFinite(Number(info.total_assets)) ? String(info.total_assets) : "--";
}

function updateStreamingStats(renderer) {
  const streaming = renderer && renderer.streaming ? renderer.streaming : null;
  const values = {
    "stat-required-gpu": streaming ? String(streaming.required_gpu ?? "--") : "--",
    "stat-warm-cpu": streaming ? String(streaming.warm_cpu ?? "--") : "--",
    "stat-pending-disk": streaming ? String(streaming.pending_disk_loads ?? "--") : "--",
    "stat-pending-upload": streaming ? String(streaming.pending_gpu_uploads ?? "--") : "--",
    "stat-pending-evict": streaming ? String(streaming.pending_gpu_evictions ?? "--") : "--",
    "stat-cpu-resident": streaming ? formatBytes(streaming.cpu_resident_bytes) : "--",
    "stat-gpu-resident": streaming ? formatBytes(streaming.gpu_resident_bytes) : "--",
    "stat-swap-hits": streaming ? String(streaming.swap_hits ?? "--") : "--",
    "stat-swap-misses": streaming ? String(streaming.swap_misses ?? "--") : "--",
  };

  Object.entries(values).forEach(([id, value]) => {
    $(id).value = value;
  });
}

function updateLoadStatePanel(renderer) {
  const status = renderer || {};
  state.rendererStatus = {
    content_loaded: Boolean(status.content_loaded),
    loaded_source_kind: typeof status.loaded_source_kind === "string" ? status.loaded_source_kind : "",
    loaded_source_path: typeof status.loaded_source_path === "string" ? status.loaded_source_path : "",
    load_state: typeof status.load_state === "string" ? status.load_state : "idle",
    last_load_error: typeof status.last_load_error === "string" ? status.last_load_error : "",
    last_load_sequence: Number.isFinite(Number(status.last_load_sequence)) ? Number(status.last_load_sequence) : 0,
  };

  $("load-state").value = state.rendererStatus.load_state;
  $("loaded-source-path").value = state.rendererStatus.loaded_source_path;
  $("last-load-error").value = state.rendererStatus.last_load_error;

  const sceneStateChip = $("scene-state-chip");
  const loadState = state.rendererStatus.load_state;
  const chipSuffix = loadState === "loaded" ? "loaded" : loadState === "loading" ? "loading" : loadState === "error" ? "error" : "idle";
  if (sceneStateChip) { sceneStateChip.textContent = loadState; sceneStateChip.className = `scene-card__chip-value scene-card__chip-value--${chipSuffix}`; }
  const sceneTypeChip = $("scene-type-chip"); if (sceneTypeChip) sceneTypeChip.textContent = state.rendererStatus.loaded_source_kind || "--";
  const sceneLoadedName = $("scene-loaded-name"); if (sceneLoadedName) sceneLoadedName.textContent = basenameOf(state.rendererStatus.loaded_source_path);
  const loadedFileChip = $("loaded-file-chip"); if (loadedFileChip) loadedFileChip.hidden = !state.rendererStatus.content_loaded;
  const loadedFileChipText = $("loaded-file-chip-text"); if (loadedFileChipText) loadedFileChipText.textContent = basenameOf(state.rendererStatus.loaded_source_path);
}

function applyRendererState(renderer) {
  updateManifestPanel(renderer ? {
    has_manifest: renderer.has_manifest,
    current_phase: renderer.current_phase,
    available_phases: renderer.available_phases,
    total_assets: renderer.total_assets,
  } : null);
  updateStreamingStats(renderer);
  updateLoadStatePanel(renderer);
}

function rendererStateFromReadyMessage(message) {
  return {
    has_manifest: message.has_manifest,
    current_phase: message.current_phase,
    available_phases: message.available_phases,
    total_assets: message.total_assets,
    content_loaded: message.content_loaded,
    loaded_source_kind: message.loaded_source_kind,
    loaded_source_path: message.loaded_source_path,
    load_state: message.load_state,
    last_load_error: message.last_load_error,
    last_load_sequence: message.last_load_sequence,
  };
}

async function pollHealthz() {
  const response = await fetch(new URL("/healthz", configuredHttpOrigin()).toString(), { cache: "no-store" });
  if (!response.ok) {
    throw new Error(`Health request failed: HTTP ${response.status}`);
  }

  const payload = await response.json();
  if (!payload.ok || !payload.renderer) {
    throw new Error("Health response did not include renderer state.");
  }

  applyRendererState(payload.renderer);
  return payload.renderer;
}

function clearHealthPoll() {
  if (state.healthPollTimer !== null) {
    window.clearInterval(state.healthPollTimer);
    state.healthPollTimer = null;
  }
  setHealthPollButtonState(false);
}

function toggleHealthPoll() {
  if (state.healthPollTimer !== null) {
    clearHealthPoll();
    setStatus("Health polling stopped.");
    return;
  }

  pollHealthz()
    .then(() => {
      state.healthPollTimer = window.setInterval(() => {
        pollHealthz().catch((error) => {
          setStatus(error.message, true);
          clearHealthPoll();
        });
      }, 1000);
      setHealthPollButtonState(true);
      setStatus("Health polling started.");
    })
    .catch((error) => {
      setStatus(error.message, true);
    });
}

function clearLoadWatch() {
  if (state.loadWatchTimer !== null) {
    window.clearInterval(state.loadWatchTimer);
    state.loadWatchTimer = null;
  }
  state.browse.pendingLoadSequence = 0;
  updateLoadButtonAvailability();
}

function startLoadWatch(sequence) {
  clearLoadWatch();
  state.browse.pendingLoadSequence = sequence;
  setLoadButtonState(true, `Loading (seq=${sequence})`);
  state.loadWatchTimer = window.setInterval(async () => {
    try {
      const renderer = await pollHealthz();
      const currentSequence = Number(renderer.last_load_sequence ?? 0);
      if (currentSequence < sequence || renderer.load_state === "loading") {
        return;
      }
      clearLoadWatch();
      if (renderer.load_state === "loaded") {
        setStatus(`Content loaded (seq=${sequence}).`);
      } else {
        setStatus(`Load failed (seq=${sequence}): ${renderer.last_load_error || "unknown error"}`, true);
      }
    } catch (error) {
      clearLoadWatch();
      setStatus(error.message, true);
    }
  }, 500);
}

function updateLoadButtonAvailability() {
  const selected = state.browse.selectedEntry;
  const hasPendingLoad = state.browse.pendingLoadSequence !== 0;
  const canLoad = Boolean(selected && selected.loadable_as && selected.loadable_as !== "none");
  if (hasPendingLoad) {
    setLoadButtonState(true, `Loading (seq=${state.browse.pendingLoadSequence})`);
    return;
  }
  setLoadButtonState(!canLoad, "Load Selected");
}

function setSelectedBrowseEntry(entry) {
  state.browse.selectedEntry = entry || null;
  $("browse-selected-path").value = entry ? entry.path : "";
  $("browse-selected-kind").value = entry ? entry.kind : "";
  $("browse-selected-loadable").value = entry ? entry.loadable_as : "";
  updateLoadButtonAvailability();
  renderBrowseEntries();
}

function createBadge(text, loadable = false, kind = "") {
  const badge = document.createElement("span");
  badge.className = "file-panel__badge";
  if (loadable) badge.classList.add("file-panel__badge--loadable");
  if (kind === "directory") badge.classList.add("file-panel__badge--directory");
  badge.textContent = text;
  return badge;
}

function renderBrowseEntries() {
  const list = $("browse-entry-list");
  list.innerHTML = "";

  if (!state.browse.entries.length) {
    const empty = document.createElement("p");
    empty.className = "file-panel__footer-status";
    empty.textContent = "No entries found in this directory.";
    list.appendChild(empty);
    return;
  }

  state.browse.entries.forEach((entry) => {
    const row = document.createElement("div");
    row.className = "file-panel__entry";
    if (state.browse.selectedEntry && state.browse.selectedEntry.path === entry.path) {
      row.classList.add("file-panel__entry--selected");
    }

    const icon = document.createElementNS("http://www.w3.org/2000/svg", "svg");
    icon.setAttribute("class", "icon");
    icon.setAttribute("aria-hidden", "true");
    const use = document.createElementNS("http://www.w3.org/2000/svg", "use");
    use.setAttribute("href", `#icon-${entry.kind === "directory" ? "folder" : "file"}`);
    icon.appendChild(use);
    row.appendChild(icon);

    const main = document.createElement("div");

    const name = document.createElement("div");
    name.className = "file-panel__entry-name";
    name.textContent = entry.name;
    main.appendChild(name);

    const path = document.createElement("div");
    path.className = "file-panel__entry-path";
    path.textContent = entry.path;
    main.appendChild(path);

    const badges = document.createElement("div");
    badges.className = "file-panel__entry-badges";
    badges.appendChild(createBadge(entry.kind, false, entry.kind));
    if (entry.loadable_as && entry.loadable_as !== "none") {
      badges.appendChild(createBadge(`load as ${entry.loadable_as}`, true, entry.kind));
    }
    main.appendChild(badges);

    const actions = document.createElement("div");
    actions.className = "file-panel__header-actions";

    if (entry.kind === "directory") {
      const openButton = document.createElement("button");
      openButton.type = "button";
      openButton.className = "file-panel__header-btn";
      openButton.textContent = "Open";
      openButton.addEventListener("click", () => browsePath(entry.path, false));
      actions.appendChild(openButton);
    }

    const selectButton = document.createElement("button");
    selectButton.type = "button";
    selectButton.className = "file-panel__header-btn";
    selectButton.textContent = "Select";
    selectButton.addEventListener("click", () => setSelectedBrowseEntry(entry));
    actions.appendChild(selectButton);

    if (entry.loadable_as && entry.loadable_as !== "none") {
      const loadButton = document.createElement("button");
      loadButton.type = "button";
      loadButton.className = "file-panel__header-btn";
      loadButton.textContent = "Load";
      loadButton.disabled = state.browse.pendingLoadSequence !== 0;
      loadButton.addEventListener("click", () => {
        setSelectedBrowseEntry(entry);
        loadSelectedContent().catch((error) => setStatus(error.message, true));
      });
      actions.appendChild(loadButton);
    }

    row.appendChild(main);
    row.appendChild(actions);
    list.appendChild(row);
  });
}

async function browsePath(path = "", preserveSelection = true) {
  const url = new URL("/api/fs/list", configuredHttpOrigin());
  if (path) {
    url.searchParams.set("path", path);
  }

  const previousSelectedPath = preserveSelection && state.browse.selectedEntry ? state.browse.selectedEntry.path : "";
  const response = await fetch(url.toString(), { cache: "no-store" });
  if (!response.ok) {
    let errorMessage = `Browse request failed: HTTP ${response.status}`;
    try {
      const payload = await response.json();
      if (payload.error) {
        errorMessage = payload.error;
      }
    } catch (error) {
      // Ignore non-JSON errors.
    }
    throw new Error(errorMessage);
  }

  const payload = await response.json();
  if (!payload.ok || !Array.isArray(payload.entries)) {
    throw new Error("Browse response did not contain directory entries.");
  }

  state.browse.currentPath = typeof payload.current_path === "string" ? payload.current_path : "";
  state.browse.parentPath = typeof payload.parent_path === "string" ? payload.parent_path : state.browse.currentPath;
  state.browse.entries = payload.entries.map((entry) => ({
    name: String(entry.name || ""),
    path: String(entry.path || ""),
    kind: String(entry.kind || ""),
    loadable_as: String(entry.loadable_as || "none"),
  }));

  $("browse-current-path").value = state.browse.currentPath;
  $("browse-parent-path").value = state.browse.parentPath;
  $("browse-current-path-display").textContent = state.browse.currentPath || "/";

  const selectionPath = previousSelectedPath || (state.browse.selectedEntry ? state.browse.selectedEntry.path : "");
  const selectedEntry = selectionPath
    ? state.browse.entries.find((entry) => entry.path === selectionPath) || null
    : null;
  state.browse.selectedEntry = selectedEntry;
  if (!selectedEntry) {
    $("browse-selected-path").value = "";
    $("browse-selected-kind").value = "";
    $("browse-selected-loadable").value = "";
  }
  updateLoadButtonAvailability();
  renderBrowseEntries();
  setBrowseStatus(`Loaded ${state.browse.entries.length} entries from ${state.browse.currentPath || "(root)"}.`);
}

async function browseRoot() {
  await browsePath("__ROOT__", false);
}

async function browseInitialPath() {
  await browsePath("", false);
}

async function browseUp() {
  const nextPath = state.browse.parentPath || state.browse.currentPath;
  await browsePath(nextPath, false);
}

function ensureSocketConnected() {
  if (!state.socket || state.socket.readyState !== WebSocket.OPEN) {
    throw new Error("WebSocket is not connected.");
  }
}

function loadSelectedContent() {
  ensureSocketConnected();
  const entry = state.browse.selectedEntry;
  if (!entry || !entry.loadable_as || entry.loadable_as === "none") {
    throw new Error("Select a loadable model directory or manifest first.");
  }

  state.socket.send(JSON.stringify({
    type: "load_content",
    source_kind: entry.loadable_as,
    path: entry.path,
  }));
  setStatus(`Load request sent: ${entry.path}`);
}

function applyPhase() {
  ensureSocketConnected();
  const phase = $("phase-custom").value.trim() || $("phase-select").value;
  state.socket.send(JSON.stringify({ type: "set_phase", phase }));
  if (state.manifestInfo) {
    updateManifestPanel({ ...state.manifestInfo, has_manifest: true, current_phase: phase });
  }
  setStatus(`Phase control sent: ${phase || "(none)"}`);
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
  const origin = configuredHttpOrigin();
  const streamUrl = new URL("/stream.mjpg", origin).toString();
  $("stream-preview").src = streamUrl;
  const toggleStream = $("toggle-stream");
  if (toggleStream) {
    toggleStream.dataset.state = "open";
    toggleStream.textContent = "⏹ Stop Stream";
  }
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

    let referenceUp = vectorNorm(upHint) > MIN_VECTOR_NORM ? vec3Normalize(upHint) : fallbackUpHint(normalizedForward);
    if (Math.abs(dot(normalizedForward, referenceUp)) >= PARALLEL_THRESHOLD) {
      referenceUp = fallbackUpHint(normalizedForward);
    }

    let right = vec3Cross(normalizedForward, referenceUp);
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
    let basis = this.rebuildBasis(this.forward, this.up);
    let forward = basis.forward;
    let up = basis.up;
    let moved = false;
    let rotated = false;

    const moveAmount = this.moveSpeed * dt;
    const moveRight = basis.right;

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
    const applyYaw = (angleRad) => {
      forward = rotateAroundAxis(forward, up, angleRad);
      basis = this.rebuildBasis(forward, up);
      forward = basis.forward;
      up = basis.up;
      rotated = true;
    };
    const applyPitch = (angleRad) => {
      const pitchAxis = basis.right;
      forward = rotateAroundAxis(forward, pitchAxis, angleRad);
      up = rotateAroundAxis(up, pitchAxis, angleRad);
      basis = this.rebuildBasis(forward, up);
      forward = basis.forward;
      up = basis.up;
      rotated = true;
    };
    const applyRoll = (angleRad) => {
      up = rotateAroundAxis(up, forward, angleRad);
      basis = this.rebuildBasis(forward, up);
      forward = basis.forward;
      up = basis.up;
      rotated = true;
    };

    if (this.keysPressed.KeyJ) {
      applyYaw(rotateAmount);
    }
    if (this.keysPressed.KeyL) {
      applyYaw(-rotateAmount);
    }
    if (this.keysPressed.KeyI) {
      applyPitch(-rotateAmount);
    }
    if (this.keysPressed.KeyK) {
      applyPitch(rotateAmount);
    }
    if (this.keysPressed.KeyU) {
      applyRoll(rotateAmount);
    }
    if (this.keysPressed.KeyO) {
      applyRoll(-rotateAmount);
    }

    if (moved) {
      this.position = position;
    }
    if (rotated) {
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
    let up = this.up.slice();
    const radiansPerPixel = this.mouseSensitivity * (Math.PI / 180.0);
    let basis = this.rebuildBasis(forward, up);

    if (deltaX !== 0) {
      forward = rotateAroundAxis(forward, up, -deltaX * radiansPerPixel);
      basis = this.rebuildBasis(forward, up);
      forward = basis.forward;
      up = basis.up;
    }

    if (deltaY !== 0) {
      const pitchAxis = basis.right;
      forward = rotateAroundAxis(forward, pitchAxis, -deltaY * radiansPerPixel);
      up = rotateAroundAxis(up, pitchAxis, -deltaY * radiansPerPixel);
    }

    basis = this.rebuildBasis(forward, up);
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
    ensureSocketConnected();
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
  updateStatusChip();
}

function connectSocket() {
  disconnectSocket();

  const wsUrl = $("ws-url").value.trim();
  const socket = new WebSocket(wsUrl);
  state.socket = socket;

  socket.addEventListener("open", () => {
    setStatus(`WebSocket connected: ${wsUrl}`);
    updateStatusChip();
  });

  socket.addEventListener("close", () => {
    setStatus("WebSocket: disconnected");
    if (state.cameraController && state.cameraController.active) {
      state.cameraController.deactivate();
      setCameraControlButtonState(false);
    }
    clearHealthPoll();
    clearLoadWatch();
    updateStatusChip();
    if (state.socket === socket) {
      state.socket = null;
    }
  });

  socket.addEventListener("error", () => {
    setStatus("WebSocket connection error.", true);
    updateStatusChip();
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
        applyRendererState(rendererStateFromReadyMessage(message));
        if (!state.browse.currentPath) {
          browseInitialPath().catch((error) => setBrowseStatus(error.message, true));
        }
        setStatus("WebSocket ready. Camera pose synchronized.");
        return;
      }
      if (message.type === "error") {
        setStatus(`WebSocket error: ${message.error}`, true);
        return;
      }
      if (message.type === "ack") {
        if (message.request_type === "set_phase") {
          setStatus(`Phase request queued (seq=${message.sequence}).`);
        } else if (message.request_type === "load_content") {
          startLoadWatch(Number(message.sequence));
          setStatus(`Load request queued (seq=${message.sequence}).`);
        }
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

  ensureSocketConnected();
  state.socket.send(payload);
  setStatus("Control payload sent.");
}

function toggleCameraControl() {
  ensureSocketConnected();

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
  state.statusError = false;
  $("http-origin").value = currentHttpOrigin();
  $("ws-url").value = currentWsUrl();
  $("move-speed").value = String(DEFAULT_MOVE_SPEED);
  $("rotate-speed").value = String(DEFAULT_ROTATE_SPEED);
  clearHealthPoll();
  clearLoadWatch();
  updateManifestPanel(null);
  updateStreamingStats(null);
  updateLoadStatePanel(null);
  setCameraControlButtonState(false);
  state.browse.currentPath = "";
  state.browse.parentPath = "";
  state.browse.entries = [];
  state.browse.selectedEntry = null;
  $("browse-current-path").value = "";
  $("browse-parent-path").value = "";
  $("browse-current-path-display").textContent = "/";
  $("browse-selected-path").value = "";
  $("browse-selected-kind").value = "";
  $("browse-selected-loadable").value = "";
  $("left-sidebar").classList.add("sidebar--open");
  $("sidebar-left-tab").hidden = true;
  $("right-sidebar").classList.remove("sidebar--open");
  $("sidebar-right-tab").hidden = false;
  $("conn-dropdown").hidden = true;
  $("toggle-conn").setAttribute("aria-expanded", "false");
  $("file-panel").hidden = true;
  $("toggle-file-panel").classList.remove("is-open");
  $("toggle-file-panel").setAttribute("aria-expanded", "false");
  $("stream-preview").src = "";
  renderBrowseEntries();
  setBrowseStatus("Browse the server filesystem and choose a model directory or manifest.");
  syncPoseUi({
    position: [0, 0, 0],
    forward: [0, 0, -1],
    up: [0, 1, 0],
    fovy: DEFAULT_FOVY,
  });
  const sceneLoadedName = $("scene-loaded-name"); if (sceneLoadedName) sceneLoadedName.textContent = "--";
  const sceneTypeChip = $("scene-type-chip"); if (sceneTypeChip) sceneTypeChip.textContent = "--";
  const sceneStateChip = $("scene-state-chip"); if (sceneStateChip) { sceneStateChip.textContent = "idle"; sceneStateChip.className = "scene-card__chip-value scene-card__chip-value--idle"; }
  $("loaded-file-chip").hidden = true;
  const toggleStream = $("toggle-stream"); if (toggleStream) { toggleStream.dataset.state = "closed"; toggleStream.textContent = "▶ Open Stream"; }
  const moveSpeedValue = document.querySelector('.slider-row__value[data-for="move-speed"]'); if (moveSpeedValue) moveSpeedValue.textContent = Number($("move-speed").value).toFixed(2);
  const rotateSpeedValue = document.querySelector('.slider-row__value[data-for="rotate-speed"]'); if (rotateSpeedValue) rotateSpeedValue.textContent = String(Math.round(Number($("rotate-speed").value)));
  setStatus("disconnected"); updateStatusChip();
  formatPayload();
  browseInitialPath().catch((error) => setBrowseStatus(error.message, true));
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
  $("browse-root").addEventListener("click", () => {
    browseRoot().catch((error) => setBrowseStatus(error.message, true));
  });
  $("browse-up").addEventListener("click", () => {
    browseUp().catch((error) => setBrowseStatus(error.message, true));
  });
  $("browse-refresh").addEventListener("click", () => {
    browsePath(state.browse.currentPath || "", true).catch((error) => setBrowseStatus(error.message, true));
  });
  $("load-selected").addEventListener("click", () => {
    try {
      loadSelectedContent();
    } catch (error) {
      setStatus(error.message, true);
    }
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
  $("apply-phase").addEventListener("click", () => {
    try {
      applyPhase();
    } catch (error) {
      setStatus(error.message, true);
    }
  });
  $("toggle-health-poll").addEventListener("click", () => {
    try {
      toggleHealthPoll();
    } catch (error) {
      setStatus(error.message, true);
    }
  });
  $("move-speed").addEventListener("input", (event) => {
    if (state.cameraController) state.cameraController.moveSpeed = Number(event.target.value);
    const valueLabel = document.querySelector('.slider-row__value[data-for="move-speed"]'); if (valueLabel) valueLabel.textContent = Number(event.target.value).toFixed(2);
  });
  $("rotate-speed").addEventListener("input", (event) => {
    if (state.cameraController) state.cameraController.rotateSpeed = Number(event.target.value);
    const valueLabel = document.querySelector('.slider-row__value[data-for="rotate-speed"]'); if (valueLabel) valueLabel.textContent = String(Math.round(Number(event.target.value)));
  });
  $("phase-select").addEventListener("change", (event) => {
    if (event.target.value) {
      $("phase-custom").value = "";
    }
  });
  $("phase-custom").addEventListener("input", (event) => {
    if (event.target.value.trim()) {
      $("phase-select").value = "";
    }
  });

  $("toggle-conn").addEventListener("click", (event) => {
    const dropdown = $("conn-dropdown"); dropdown.hidden = !dropdown.hidden;
    event.currentTarget.setAttribute("aria-expanded", dropdown.hidden ? "false" : "true");
  });
  $("toggle-stream").addEventListener("click", () => {
    const btn = $("toggle-stream"); const currentState = btn.dataset.state || "closed";
    if (currentState === "closed") {
      try { openStream(); btn.dataset.state = "open"; btn.textContent = "⏹ Stop Stream"; } catch (error) { setStatus(error.message, true); }
    } else {
      $("stream-preview").src = ""; btn.dataset.state = "closed"; btn.textContent = "▶ Open Stream"; setStatus("Stream closed.");
    }
  });
  $("toggle-left").addEventListener("click", () => {
    const sidebar = $("left-sidebar"); const tab = $("sidebar-left-tab"); tab.hidden = sidebar.classList.toggle("sidebar--open");
  });
  $("sidebar-left-tab").addEventListener("click", () => { $("left-sidebar").classList.add("sidebar--open"); $("sidebar-left-tab").hidden = true; });
  $("toggle-right").addEventListener("click", () => {
    const sidebar = $("right-sidebar"); const tab = $("sidebar-right-tab"); tab.hidden = sidebar.classList.toggle("sidebar--open");
  });
  $("sidebar-right-tab").addEventListener("click", () => { $("right-sidebar").classList.add("sidebar--open"); $("sidebar-right-tab").hidden = true; });
  $("toggle-file-panel").addEventListener("click", (event) => {
    const panel = $("file-panel");
    const willOpen = panel.hidden;
    panel.hidden = !panel.hidden;
    event.currentTarget.classList.toggle("is-open", !panel.hidden); event.currentTarget.setAttribute("aria-expanded", panel.hidden ? "false" : "true");
    if (willOpen && !state.browse.currentPath) {
      browseInitialPath().catch((error) => setBrowseStatus(error.message, true));
    }
  });
}

bindActions();
applyDefaults();
