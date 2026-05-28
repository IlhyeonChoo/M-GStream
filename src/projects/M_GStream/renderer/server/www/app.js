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
  search: {
    active: false,
    query: "",
    results: [],
    rootPath: "",
    truncated: false,
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

const TWEAK_DEFAULTS = { theme: "dark", sidebarWidth: 220 };
const STATE_DEFAULTS = { activePanel: "sidebar" };

function loadJson(key, def) {
  try {
    const raw = localStorage.getItem(key);
    if (raw === null) return { ...def };
    const parsed = JSON.parse(raw);
    return { ...def, ...parsed };
  } catch (_) { return { ...def }; }
}
function saveJson(key, value) {
  try { localStorage.setItem(key, JSON.stringify(value)); } catch (_) { /* no-op */ }
}

const tweaks = loadJson("mgstream-tweaks", TWEAK_DEFAULTS);
const persisted = loadJson("mgstream-state", STATE_DEFAULTS);
const uiState = {
  theme: tweaks.theme === "light" ? "light" : "dark",
  sidebarWidth: Math.max(160, Math.min(300, Number(tweaks.sidebarWidth) || 220)),
  activePanel: ["sidebar", "viewer", "scenes"].includes(persisted.activePanel) ? persisted.activePanel : "sidebar",
  infoTab: "connection",
  infoPanelCollapsed: false,
};

function applyTheme(theme) {
  uiState.theme = theme === "light" ? "light" : "dark";
  const root = $("app");
  if (root) {
    root.classList.toggle("theme-dark", uiState.theme === "dark");
    root.classList.toggle("theme-light", uiState.theme === "light");
  }
  const themeBtn = $("theme-toggle");
  if (themeBtn) themeBtn.textContent = uiState.theme === "dark" ? "☾" : "☀";
  saveJson("mgstream-tweaks", { theme: uiState.theme, sidebarWidth: uiState.sidebarWidth });
  if (typeof updateStatusChip === "function") updateStatusChip();
}

const ACTIVE_PANEL_LABEL = {
  sidebar: { icon: "⌨", text: "Camera Control · Keyboard active" },
  viewer:  { icon: "▶", text: "Viewer · Keyboard navigation" },
  scenes:  { icon: "⊞", text: "Browse Scenes · Panel active" },
};

function updateActivePanelPill() {
  const pill = document.getElementById("active-panel-pill");
  if (!pill) return;
  const label = ACTIVE_PANEL_LABEL[uiState.activePanel];
  if (!label) { pill.hidden = true; return; }
  pill.hidden = false;
  const icon = document.getElementById("active-panel-pill-icon");
  const text = document.getElementById("active-panel-pill-text");
  if (icon) icon.textContent = label.icon;
  if (text) text.textContent = label.text;
}

function setActivePanel(next) {
  if (!["sidebar", "viewer", "scenes"].includes(next)) return;
  uiState.activePanel = next;
  saveJson("mgstream-state", { activePanel: next });
  const root = $("app");
  if (root) root.dataset.activePanel = next;
  const targets = {
    sidebar: document.getElementById("left-sidebar"),
    viewer:  document.getElementById("stream-stage"),
    scenes:  document.getElementById("file-panel"),
  };
  Object.entries(targets).forEach(([key, el]) => {
    if (!el) return;
    el.classList.toggle("panel-focus-ring", key === next);
    el.classList.toggle("is-active", key === next);
  });
  updateActivePanelPill();
}

function setInfoTab(tab) {
  if (!["connection", "camera", "scene"].includes(tab)) return;
  uiState.infoTab = tab;
  document.querySelectorAll(".info-panel__tab").forEach((el) => {
    el.classList.toggle("is-active", el.dataset.tab === tab);
  });
  document.querySelectorAll(".info-panel__pane").forEach((el) => {
    el.classList.toggle("is-active", el.dataset.pane === tab);
  });
}

function setInfoPanelCollapsed(collapsed) {
  uiState.infoPanelCollapsed = Boolean(collapsed);
  const panel = document.getElementById("info-panel");
  if (panel) panel.classList.toggle("is-collapsed", uiState.infoPanelCollapsed);
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

const STATUS_COLORS = {
  dark: { connected: "#4d9e6e", disconnected: "#a05050", connecting: "#a08a3a", streaming: "#4d7fc4" },
  light: { connected: "#2f7a4f", disconnected: "#b84242", connecting: "#a07a1e", streaming: "#2a6bc0" },
};

function currentWsStatus() {
  if (!state.socket) return "disconnected";
  switch (state.socket.readyState) {
    case WebSocket.CONNECTING: return "connecting";
    case WebSocket.OPEN: return state.statusError ? "disconnected" : "connected";
    default: return "disconnected";
  }
}

function updateStatusChip() {
  const chip = document.getElementById("status-chip");
  if (!chip) return;
  const ws = currentWsStatus();
  const streamOpen = ($("toggle-stream") || {}).dataset && $("toggle-stream").dataset.state === "open";
  const status = streamOpen && ws !== "disconnected" ? "streaming" : ws;
  const live = status === "connected" || status === "streaming" || status === "connecting";
  chip.classList.remove("topbar__chip--status--live", "topbar__chip--status--down");
  chip.classList.add(live ? "topbar__chip--status--live" : "topbar__chip--status--down");
  const dot = document.getElementById("status-dot");
  if (dot) {
    const palette = STATUS_COLORS[uiState.theme] || STATUS_COLORS.dark;
    dot.style.background = palette[status] || palette.disconnected;
    dot.style.boxShadow = `0 0 6px ${palette[status] || palette.disconnected}`;
    dot.classList.toggle("is-pulsing", status === "connecting" || status === "streaming");
  }
  const wsBtn = document.getElementById("connect-ws");
  if (wsBtn) {
    wsBtn.classList.toggle("is-connected", ws === "connected");
    wsBtn.dataset.connected = ws === "connected" ? "true" : "false";
    const label = wsBtn.querySelector("span");
    if (label) label.textContent = ws === "connected" ? "Disconnect WS" : (ws === "connecting" ? "Connecting…" : "Connect WS");
  }
  const chipText = document.getElementById("status-chip-text");
  if (chipText) {
    const labels = { connected: "Connected", disconnected: "Disconnected", connecting: "Connecting…", streaming: "Streaming" };
    chipText.textContent = labels[status] || "Disconnected";
  }
  const streamBtn = document.getElementById("toggle-stream");
  if (streamBtn) {
    const open = streamBtn.dataset.state === "open";
    streamBtn.classList.toggle("is-open", open);
  }
  updateActivePanelPill();
}

function setBrowseStatus(text, isError = false) {
  const statusLine = $("browse-status");
  statusLine.textContent = text;
  statusLine.dataset.error = isError ? "true" : "false";
}

function setCameraControlButtonState(active) {
  const button = $("toggle-camera");
  const label = button.querySelector("span");
  if (label) {
    label.textContent = active ? "Camera Control ON" : "Camera Control OFF";
  } else {
    button.textContent = active ? "Camera Control ON" : "Camera Control OFF";
  }
  button.classList.toggle("active", active);
  button.setAttribute("aria-pressed", active ? "true" : "false");
  const pill = document.getElementById("camera-state-pill");
  if (pill) {
    pill.textContent = active ? "ENABLED" : "DISABLED";
    pill.classList.toggle("info-panel__pill--enabled", active);
    pill.classList.toggle("info-panel__pill--disabled", !active);
  }
  if (active) setActivePanel("sidebar");
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

function syncRendererCameraPose(renderer, allowActiveController = false) {
  if (!renderer || !renderer.camera_pose_available || !renderer.camera_pose) {
    return;
  }

  const controller = state.cameraController;
  if (controller && controller.active && !allowActiveController) {
    return;
  }

  syncPoseUi(renderer.camera_pose);
  if (controller) {
    controller.initFromPose(renderer.camera_pose);
  }
}

function applyRendererState(renderer) {
  syncRendererCameraPose(renderer, false);
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
        syncRendererCameraPose(renderer, true);
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
  const canLoad = isBrowseEntryLoadable(selected);
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

function createBadge(text, loadable = false, kind = "", hint = "") {
  const badge = document.createElement("span");
  badge.className = "file-panel__badge";
  if (loadable) badge.classList.add("file-panel__badge--loadable");
  if (kind === "directory") badge.classList.add("file-panel__badge--directory");
  const label = document.createElement("span");
  label.textContent = text;
  badge.appendChild(label);
  if (hint) {
    const hintNode = document.createElement("small");
    hintNode.textContent = ` ${hint}`;
    badge.appendChild(hintNode);
  }
  return badge;
}

function isCandidateLoadable(loadableAs) {
  return loadableAs === "candidate_model_dir" || loadableAs === "candidate_manifest";
}

function isResolvedLoadable(loadableAs) {
  return loadableAs === "model_dir" || loadableAs === "manifest";
}

function isBrowseEntryLoadable(entry) {
  return Boolean(entry && entry.loadable_as && entry.loadable_as !== "none");
}

function describeLoadableBadge(loadableAs) {
  if (loadableAs === "candidate_model_dir") {
    return { text: "load as model_dir", hint: "(unverified)" };
  }
  if (loadableAs === "candidate_manifest") {
    return { text: "load as manifest", hint: "(unverified)" };
  }
  return { text: `load as ${loadableAs}`, hint: "" };
}

function clearBrowseSelectionFields() {
  $("browse-selected-path").value = "";
  $("browse-selected-kind").value = "";
  $("browse-selected-loadable").value = "";
}

function visibleBrowseEntries() {
  return state.search.active ? state.search.results : state.browse.entries;
}

function resetSearchState(resetInput = false) {
  state.search.active = false;
  state.search.query = "";
  state.search.results = [];
  state.search.rootPath = "";
  state.search.truncated = false;
  if (resetInput) {
    $("browse-search").value = "";
  }
}

function browseStatusMessage() {
  const currentPath = state.browse.currentPath || "/";
  return `Loaded ${state.browse.entries.length} entries from ${currentPath}.`;
}

function searchStatusMessage() {
  const rootPath = state.search.rootPath || state.browse.currentPath || "/";
  const count = state.search.results.length;
  const noun = count === 1 ? "result" : "results";
  let message = `Loaded ${count} search ${noun} for "${state.search.query}" under ${rootPath}.`;
  if (state.search.truncated) {
    message += " Showing the first 100 matches.";
  }
  return message;
}

function renderBrowseEntries() {
  const list = $("browse-entry-list");
  const entries = visibleBrowseEntries();
  list.innerHTML = "";

  if (!entries.length) {
    const empty = document.createElement("p");
    empty.className = "file-panel__footer-status";
    empty.textContent = state.search.active
      ? "No loadable scenes or manifests matched this query."
      : "No entries found in this directory.";
    list.appendChild(empty);
    return;
  }

  entries.forEach((entry) => {
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
    main.className = "file-panel__entry-main";

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
    if (isBrowseEntryLoadable(entry)) {
      const loadableBadge = describeLoadableBadge(entry.loadable_as);
      badges.appendChild(createBadge(loadableBadge.text, true, entry.kind, loadableBadge.hint));
    }
    main.appendChild(badges);

    const actions = document.createElement("div");
    actions.className = "file-panel__entry-actions";

    if (entry.kind === "directory") {
      const openButton = document.createElement("button");
      openButton.type = "button";
      openButton.className = "file-panel__header-btn";
      openButton.textContent = "Open";
      openButton.addEventListener("click", () => {
        if (state.search.active) {
          resetSearchState(true);
        }
        browsePath(entry.path, false).catch((error) => setBrowseStatus(error.message, true));
      });
      actions.appendChild(openButton);
    }

    const selectButton = document.createElement("button");
    selectButton.type = "button";
    selectButton.className = "file-panel__header-btn";
    selectButton.textContent = "Select";
    selectButton.addEventListener("click", () => setSelectedBrowseEntry(entry));
    actions.appendChild(selectButton);

    if (isBrowseEntryLoadable(entry)) {
      const loadButton = document.createElement("button");
      loadButton.type = "button";
      loadButton.className = "file-panel__header-btn";
      loadButton.textContent = "Load";
      loadButton.disabled = state.browse.pendingLoadSequence !== 0;
      loadButton.addEventListener("click", async () => {
        try {
          setSelectedBrowseEntry(entry);
          await loadSelectedContent();
        } catch (error) {
          setStatus(error.message, true);
        }
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
    clearBrowseSelectionFields();
  }
  updateLoadButtonAvailability();
  renderBrowseEntries();
  setBrowseStatus(browseStatusMessage());
}

async function browseSearch(query, path = state.browse.currentPath || "", preserveSelection = true) {
  const trimmedQuery = String(query || "").trim();
  $("browse-search").value = trimmedQuery;

  if (!trimmedQuery) {
    resetSearchState(true);
    if (!state.browse.currentPath) {
      await browseInitialPath();
      return;
    }
    renderBrowseEntries();
    updateLoadButtonAvailability();
    setBrowseStatus(browseStatusMessage());
    return;
  }

  const url = new URL("/api/fs/search", configuredHttpOrigin());
  url.searchParams.set("q", trimmedQuery);
  if (path) {
    url.searchParams.set("path", path);
  }

  const previousSelectedPath = preserveSelection && state.browse.selectedEntry ? state.browse.selectedEntry.path : "";
  const response = await fetch(url.toString(), { cache: "no-store" });
  if (!response.ok) {
    let errorMessage = `Search request failed: HTTP ${response.status}`;
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
    throw new Error("Search response did not contain entries.");
  }

  state.search.active = true;
  state.search.query = trimmedQuery;
  state.search.rootPath = typeof payload.current_path === "string" ? payload.current_path : state.browse.currentPath;
  state.search.results = payload.entries.map((entry) => ({
    name: String(entry.name || ""),
    path: String(entry.path || ""),
    kind: String(entry.kind || ""),
    loadable_as: String(entry.loadable_as || "none"),
  }));
  state.search.truncated = Boolean(payload.truncated);

  const selectionPath = previousSelectedPath || (state.browse.selectedEntry ? state.browse.selectedEntry.path : "");
  const selectedEntry = selectionPath
    ? state.search.results.find((entry) => entry.path === selectionPath) || null
    : null;
  state.browse.selectedEntry = selectedEntry;
  if (!selectedEntry) {
    clearBrowseSelectionFields();
  }
  updateLoadButtonAvailability();
  renderBrowseEntries();
  setBrowseStatus(searchStatusMessage());
}

async function browseRoot() {
  resetSearchState(true);
  await browsePath("__ROOT__", false);
}

async function browseInitialPath() {
  resetSearchState(true);
  await browsePath("", false);
}

async function browseUp() {
  resetSearchState(true);
  const nextPath = state.browse.parentPath || state.browse.currentPath;
  await browsePath(nextPath, false);
}

async function refreshBrowsePanel() {
  if (state.search.active) {
    await browseSearch(state.search.query, state.search.rootPath || state.browse.currentPath || "", true);
    return;
  }
  await browsePath(state.browse.currentPath || "", true);
}

function ensureSocketConnected() {

  if (!state.socket || state.socket.readyState !== WebSocket.OPEN) {
    throw new Error("WebSocket is not connected.");
  }
}

async function probeBrowseEntry(entry) {
  const url = new URL("/api/fs/probe", configuredHttpOrigin());
  url.searchParams.set("path", entry.path);

  const response = await fetch(url.toString(), { cache: "no-store" });
  let payload = null;
  try {
    payload = await response.json();
  } catch (error) {
    payload = null;
  }

  if (!response.ok || !payload || !payload.ok) {
    const errorMessage = payload && payload.error
      ? payload.error
      : `Probe failed: HTTP ${response.status}`;
    setStatus(errorMessage, true);
    return null;
  }

  return payload;
}

async function loadSelectedContent() {
  ensureSocketConnected();
  const entry = state.browse.selectedEntry;
  if (!isBrowseEntryLoadable(entry)) {
    throw new Error("Select a loadable model directory or manifest first.");
  }

  if (isCandidateLoadable(entry.loadable_as)) {
    const payload = await probeBrowseEntry(entry);
    if (!payload) {
      return;
    }

    entry.path = String(payload.canonical_path || entry.path);
    entry.loadable_as = String(payload.kind || entry.loadable_as);
    if (!isResolvedLoadable(entry.loadable_as)) {
      setStatus("Probe returned an unsupported load target.", true);
      return;
    }
    setSelectedBrowseEntry(entry);
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
    this.dirty = false;
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
      position = vec3Add(position, vec3Scale(moveRight, moveAmount));
      moved = true;
    }
    if (this.keysPressed.KeyD && vectorNorm(moveRight) > MIN_VECTOR_NORM) {
      position = vec3Sub(position, vec3Scale(moveRight, moveAmount));
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
      applyYaw(-rotateAmount);
    }
    if (this.keysPressed.KeyL) {
      applyYaw(rotateAmount);
    }
    if (this.keysPressed.KeyI) {
      applyPitch(-rotateAmount);
    }
    if (this.keysPressed.KeyK) {
      applyPitch(rotateAmount);
    }
    if (this.keysPressed.KeyU) {
      applyRoll(-rotateAmount);
    }
    if (this.keysPressed.KeyO) {
      applyRoll(rotateAmount);
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
    this.dirty = false;
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

async function toggleCameraControl() {
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

  await pollHealthz();

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
  resetSearchState(true);
  $("browse-current-path").value = "";
  $("browse-parent-path").value = "";
  $("browse-current-path-display").textContent = "/";
  clearBrowseSelectionFields();
  $("left-sidebar").classList.add("sidebar--open");
  $("sidebar-left-tab").hidden = true;
  $("file-panel").hidden = true;
  $("toggle-file-panel").classList.remove("is-open");
  $("toggle-file-panel").setAttribute("aria-expanded", "false");
  $("stream-preview").src = "";
  renderBrowseEntries();
  setBrowseStatus("Browse the server filesystem and search for a loadable model directory or manifest.");
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
      if (currentWsStatus() === "connected") {
        disconnectSocket();
        setStatus("WebSocket: disconnected");
      } else {
        connectSocket();
      }
    } catch (error) {
      setStatus(error.message, true);
    }
  });
  $("disconnect-ws").addEventListener("click", () => {
    disconnectSocket();
    setStatus("WebSocket: disconnected");
  });

  const submitBrowseSearch = () => {
    browseSearch($("browse-search").value, state.browse.currentPath || "", true).catch((error) => setBrowseStatus(error.message, true));
  };

  $("browse-search-submit").addEventListener("click", submitBrowseSearch);
  $("browse-search-clear").addEventListener("click", () => {
    resetSearchState(true);
    renderBrowseEntries();
    updateLoadButtonAvailability();
    setBrowseStatus(browseStatusMessage());
  });
  $("browse-search").addEventListener("keydown", (event) => {
    if (event.key === "Enter") {
      event.preventDefault();
      submitBrowseSearch();
    }
  });

  $("browse-root").addEventListener("click", () => {
    browseRoot().catch((error) => setBrowseStatus(error.message, true));
  });
  $("browse-up").addEventListener("click", () => {
    browseUp().catch((error) => setBrowseStatus(error.message, true));
  });
  $("browse-refresh").addEventListener("click", () => {
    refreshBrowsePanel().catch((error) => setBrowseStatus(error.message, true));
  });
  $("load-selected").addEventListener("click", async () => {
    try {
      await loadSelectedContent();
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
    toggleCameraControl().catch((error) => {
      setStatus(error.message, true);
    });
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

  $("toggle-stream").addEventListener("click", () => {
    const btn = $("toggle-stream"); const currentState = btn.dataset.state || "closed";
    if (currentState === "closed") {
      try { openStream(); btn.dataset.state = "open"; btn.textContent = "⏹ Stop Stream"; btn.classList.add("is-open"); }
      catch (error) { setStatus(error.message, true); }
    } else {
      $("stream-preview").src = ""; btn.dataset.state = "closed"; btn.textContent = "▶ Open Stream"; btn.classList.remove("is-open"); setStatus("Stream closed.");
    }
    updateStatusChip();
  });
  $("toggle-left").addEventListener("click", () => {
    const sidebar = $("left-sidebar"); const tab = $("sidebar-left-tab"); tab.hidden = sidebar.classList.toggle("sidebar--open");
  });
  $("sidebar-left-tab").addEventListener("click", () => { $("left-sidebar").classList.add("sidebar--open"); $("sidebar-left-tab").hidden = true; });

  $("toggle-file-panel").addEventListener("click", (event) => {
    const panel = $("file-panel");
    const willOpen = panel.hidden;
    panel.hidden = !panel.hidden;
    event.currentTarget.classList.toggle("is-open", !panel.hidden);
    event.currentTarget.setAttribute("aria-expanded", panel.hidden ? "false" : "true");
    if (willOpen) {
      setActivePanel("scenes");
      if (!state.browse.currentPath) {
        browseInitialPath().catch((error) => setBrowseStatus(error.message, true));
      }
    } else {
      setActivePanel("sidebar");
    }
  });

  // Theme toggle
  const themeBtn = $("theme-toggle");
  if (themeBtn) themeBtn.addEventListener("click", () => applyTheme(uiState.theme === "dark" ? "light" : "dark"));

  // InfoPanel tabs + collapse/expand
  document.querySelectorAll(".info-panel__tab").forEach((el) => {
    el.addEventListener("click", (event) => { event.stopPropagation(); setInfoTab(el.dataset.tab); });
  });
  const infoClose = $("info-panel-close");
  if (infoClose) infoClose.addEventListener("click", (event) => { event.stopPropagation(); setInfoPanelCollapsed(true); });
  const infoExpand = $("info-panel-expand");
  if (infoExpand) infoExpand.addEventListener("click", (event) => { event.stopPropagation(); setInfoPanelCollapsed(false); });

  // Scenes browser close (modal)
  const scenesClose = $("scenes-close");
  if (scenesClose) scenesClose.addEventListener("click", (event) => {
    event.stopPropagation();
    const panel = $("file-panel");
    panel.hidden = true;
    const fab = $("toggle-file-panel");
    fab.classList.remove("is-open");
    fab.setAttribute("aria-expanded", "false");
    setActivePanel("sidebar");
  });

  // Click delegation for activePanel selection.
  const sidebarEl = $("left-sidebar");
  if (sidebarEl) sidebarEl.addEventListener("mousedown", () => setActivePanel("sidebar"));
  const stage = $("stream-stage");
  if (stage) stage.addEventListener("mousedown", () => setActivePanel("viewer"));
  const filePanel = $("file-panel");
  if (filePanel) filePanel.addEventListener("mousedown", () => setActivePanel("scenes"));
}

bindActions();
applyTheme(uiState.theme);
setInfoTab(uiState.infoTab);
setActivePanel(uiState.activePanel);
applyDefaults();
