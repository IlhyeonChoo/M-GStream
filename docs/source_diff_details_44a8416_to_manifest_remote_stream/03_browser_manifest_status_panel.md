# browser manifest status panel 상세

이 문서는 `www` reference client 에 manifest status panel, phase apply, `healthz` polling 이 추가된 변화를 정리한다.

## 파일: `src/projects/extended_gaussian/renderer/server/www/index.html`

### 초기 코드

```html
<section class="panel">
  <h2>Camera Control</h2>
  ...
</section>

<section class="panel">
  <h2>Camera Pose</h2>
  ...
</section>
```

초기 browser client 는 connection / camera control / raw pose form 까지만 있었고, manifest 상태를 볼 수 있는 별도 UI 가 없었다.

### 현재 코드

```html
<section class="panel" id="manifest-panel" style="display:none;">
  <h2>Manifest Status</h2>
  <div class="grid two-col">
    <label>
      <span>Current Phase</span>
      <div class="phase-control">
        <select id="phase-select"><option value="">(none)</option></select>
        <input id="phase-custom" type="text" placeholder="Custom phase...">
      </div>
    </label>
    <label>
      <span>Total Assets</span>
      <input id="total-assets" type="text" value="--" readonly>
    </label>
  </div>
  <div class="actions">
    <button id="apply-phase" type="button">Apply Phase</button>
    <button id="toggle-health-poll" type="button">Start Health Poll</button>
  </div>
  <div class="grid three-col streaming-stats">
    ... stat-required-gpu / stat-warm-cpu / stat-pending-disk / ...
  </div>
</section>
```

## 파일: `src/projects/extended_gaussian/renderer/server/www/app.js`

### 초기 코드

```javascript
const state = {
  socket: null,
  cameraController: null,
};
```

```javascript
socket.addEventListener("message", (event) => {
  try {
    const message = JSON.parse(event.data);
    if (message.type === "ready") {
      ...
      setStatus("WebSocket ready. Camera pose synchronized.");
      return;
    }
    if (message.type === "ack") {
      return;
    }
  } catch (error) {
    ...
  }
});
```

초기 `app.js` 는 browser camera control 은 했지만, manifest 존재 여부를 모른 채 단순 `/control` client 로만 동작했다.

### 현재 코드

```javascript
const state = {
  socket: null,
  cameraController: null,
  healthPollTimer: null,
  manifestInfo: null,
};
```

```javascript
function updateManifestPanel(info) {
  ...
  if (!info || !info.has_manifest) {
    panel.style.display = "none";
    ...
    return;
  }
  panel.style.display = "";
  ...
}

async function pollHealthz() {
  const response = await fetch(new URL("/healthz", configuredHttpOrigin()).toString(), { cache: "no-store" });
  ...
  updateManifestPanel(payload.renderer);
  updateStreamingStats(payload.renderer);
}

function applyPhase() {
  const phase = $("phase-custom").value.trim() || $("phase-select").value;
  ...
  state.socket.send(JSON.stringify({ type: "set_phase", phase }));
}
```

```javascript
if (message.type === "ready") {
  ...
  updateManifestPanel({
    has_manifest: message.has_manifest,
    current_phase: message.current_phase,
    available_phases: message.available_phases,
    total_assets: message.total_assets,
  });
  ...
}
if (message.type === "ack") {
  if (message.request_type === "set_phase") {
    setStatus(`Phase request queued (seq=${message.sequence}).`);
  }
  return;
}
```

## 파일: `src/projects/extended_gaussian/renderer/server/www/styles.css`

### 현재 코드

```css
.phase-control {
  display: flex;
  gap: 8px;
}

.phase-control select {
  flex: 1;
  min-width: 0;
  padding: 12px 14px;
  border: 1px solid var(--line);
  border-radius: 14px;
  background: rgba(255, 255, 255, 0.92);
  color: var(--ink);
}

.streaming-stats input[readonly] {
  background: rgba(11, 122, 117, 0.06);
  text-align: center;
  font-family: "IBM Plex Mono", monospace;
}
```

## 바뀐 이유

- manifest 기반 remote stream 의 실제 gap 은 phase 를 바꿀 방법과 manifest/streaming 상태를 볼 방법이 browser 에 없었다는 점이다.
- `ready` 메시지는 connection 직후 최소 상태를 채우고, `healthz` polling 은 1 Hz 로 live streaming stats 를 갱신한다. 이렇게 나누면 WebSocket protocol 을 크게 늘리지 않고도 상태 UI 를 유지할 수 있다.
- `set_phase` 는 arbitrary string 을 허용하므로 dropdown 과 custom input 을 같이 두었다.
- `ack.request_type` 을 확인해 `set_camera_pose` 연속 ack 는 계속 무시하고, `set_phase` 단발 요청만 status line 에 반영한다.
