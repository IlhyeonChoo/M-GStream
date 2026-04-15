# browser control panel UI 상세

이 문서는 M8에서 `index.html` 과 `styles.css` 가 **browser camera control panel 과 활성 상태 시각 표현**을 갖도록 바뀐 변화를 정리한다.

## 디렉터리: `src/projects/extended_gaussian/renderer/server/www`

### 파일: `src/projects/extended_gaussian/renderer/server/www/index.html`

#### 초기 코드

```html
<section class="panel">
  <h2>Connection</h2>
  ...
  <img id="stream-preview" alt="MJPEG stream preview">
</section>

<section class="panel">
  <h2>Camera Pose</h2>
  ...
</section>
```

초기 HTML 은 connection panel 과 camera pose form 만 있었다. 사용자는 payload 를 숫자로 수동 편집한 뒤 `Send Payload` 를 눌러야 했다.

#### 현재 코드

```html
<section class="panel">
  <h2>Camera Control</h2>
  <div class="actions">
    <button id="toggle-camera" type="button">Enable Camera Control</button>
  </div>
  <div class="grid two-col">
    <label>
      <span>Move speed</span>
      <input id="move-speed" type="range" min="0.05" max="5.0" step="0.05" value="0.6">
    </label>
    <label>
      <span>Rotate speed (deg/s)</span>
      <input id="rotate-speed" type="range" min="5" max="120" step="1" value="30">
    </label>
  </div>
  <div class="key-legend">
    <span><kbd>W</kbd><kbd>A</kbd><kbd>S</kbd><kbd>D</kbd> Move</span>
    <span><kbd>Q</kbd><kbd>E</kbd> Up / Down</span>
    <span><kbd>Up</kbd><kbd>Down</kbd><kbd>Left</kbd><kbd>Right</kbd> Rotate</span>
    <span>Drag to look / Scroll to zoom</span>
  </div>
</section>
```

### 파일: `src/projects/extended_gaussian/renderer/server/www/styles.css`

#### 초기 코드

```css
button:hover {
  background: var(--accent-strong);
  transform: translateY(-1px);
}

#stream-preview {
  width: 100%;
  min-height: 240px;
  ...
}
```

기존 스타일은 정적 reference client 수준만 가정하고 있었고, camera control active/inactive 상태를 드러내는 규칙이 없었다.

#### 현재 코드

```css
button.active {
  background: var(--danger);
}

button.active:hover {
  background: #8a3327;
}

#stream-preview.camera-active {
  cursor: crosshair;
  border-color: var(--accent);
  box-shadow: 0 0 0 1px rgba(11, 122, 117, 0.25);
}
```

```css
.key-legend {
  display: flex;
  flex-wrap: wrap;
  gap: 12px 18px;
  margin-top: 18px;
  align-items: center;
}

kbd {
  display: inline-block;
  min-width: 2.4em;
  padding: 4px 8px;
  border: 1px solid var(--line);
  border-radius: 8px;
  background: rgba(255, 255, 255, 0.94);
  ...
}

input[type="range"]::-webkit-slider-thumb {
  ...
  background: var(--accent);
}
```

#### 바뀐 이유

- `app.js` 쪽에 실시간 camera control 이 들어가면, 브라우저 UI도 사용자가 **지금 control mode 가 켜져 있는지**, **어떤 키를 쓰는지**, **속도를 어디서 조절하는지** 즉시 알 수 있어야 한다.
- 그래서 `index.html` 에 별도 `Camera Control` 패널을 추가해 enable/disable 버튼, move/rotate slider, key legend 를 노출했다.
- `styles.css` 는 기존 reference client 톤은 유지하되, active button 과 `#stream-preview.camera-active` 로 control 상태를 명확히 드러내고, `kbd` / range slider 스타일로 조작 요소를 읽기 쉽게 만들었다.
- 이 변경은 purely browser UI scope 이고, 서버 protocol / build / install 경로에는 영향을 주지 않는다.

## 요약

- M8의 HTML/CSS 변경은 새 로직을 추가하는 것이 아니라, **새 input runtime 이 실제로 usable 하도록 UI affordance 를 보강한 작업**이다.
- 사용자는 더 이상 raw payload 편집만 보지 않고, panel 에서 바로 control mode 를 켜고 속도를 조절할 수 있다.
