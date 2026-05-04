# Handoff: M-GStream Remote Viewer UI Redesign

## Overview
Redesign of the browser UI for M-GStream — a remote server that renders 3D Gaussian Splatting scenes and streams them to a browser, where users can navigate the scene with keyboard/mouse and browse/load scenes from the server filesystem.

The existing UI (see the original screenshot provided by the user) had four specific problems this redesign addresses:

1. **Limited color / theming** — only a single hard-coded dark palette.
2. **Overlapping / cramped text** — long scene paths, status strings, and the top bar all competed for space.
3. **Ambiguous connection-state colors** — "Stop Stream" and "Connect WS" buttons used muted/near-identical tones, so users couldn't tell whether the WS or stream was currently live.
4. **No foreground-panel indicator** — when the user had focus on the camera-control sidebar, the scenes browser, or the viewer (keyboard navigation), there was no visual cue telling them which surface would receive their input.

## About the Design Files
The files in this bundle are **design references created in HTML** — an interactive prototype showing the intended look, layout, interactions, and state transitions. They are **not production code to copy directly**.

Your task is to **recreate these designs in the existing M-GStream codebase**, using whatever framework, component library, and styling system that repo already uses. The HTML prototype uses React + inline styles for convenience; map the visual decisions (tokens, layout, states) onto the real code's conventions — do not port the inline-style soup verbatim.

If the repo has no existing UI framework, pick what fits best given the rest of the stack.

## Fidelity
**High-fidelity.** Colors, typography, spacing, component states, and interaction behavior are all specified and should be reproduced closely. Deviations are fine where the existing codebase has stronger conventions (e.g. a design token system), but the end result should feel visually identical to the prototype.

## Files in This Bundle
- `M-GStream.html` — the interactive prototype. Open in a browser to explore. Uses React via CDN + Babel standalone; all logic is in one file.
- `README.md` — this document.

---

## Global Structure

```
┌──────────────────────────────────────────────────────────────────────┐
│  Top Bar  [M_GStream] [active-panel pill] [scene]  ...  [☾] [status] │
│           [Stop/Start Stream] [Disconnect/Connect WS]                │
├─────────────┬────────────────────────────────────────────────────────┤
│             │                                     ┌───── InfoPanel ─┐│
│  Sidebar    │               Viewer                │ Connection|Cam.. ││
│  (Controls) │       (Gaussian Splat render)       │  (tabs: Conn /   ││
│             │                                     │   Camera / Scene)││
│             │       ┌─ ScenesBrowser (modal) ─┐   └──────────────────┘│
│             │       │  (floating, centered)   │                       │
│             │       └─────────────────────────┘                       │
│             │                                                          │
│             │               [⊞ Browse Scenes] ← FAB toggle            │
└─────────────┴────────────────────────────────────────────────────────┘
```

Three focusable surfaces: **Sidebar (Controls)**, **Viewer**, **ScenesBrowser**. Exactly one is active at a time. Clicking any of them makes it active. The active surface gets an accent-colored focus ring + a small "ACTIVE" badge, and the top bar shows a pill indicating which it is.

---

## Theming System

Replace the current hard-coded palette with a two-theme CSS-variable system. Only two themes: **dark** (default) and **light**. The accent is a single muted green in both themes; do not introduce multiple accent hues.

### Design tokens

Apply these as CSS variables on a root element carrying a `theme-dark` or `theme-light` class. Components consume tokens, never literal colors.

#### Dark theme
| Token | Value | Purpose |
|---|---|---|
| `--bg`            | `#0a0a0a` | Page background |
| `--surface`       | `#111`    | Top bar, sidebar, scene cards |
| `--surface-2`     | `#161616` | Loaded-scene card, field backgrounds |
| `--surface-3`     | `#1a1a1a` | Tweaks panel, search input |
| `--surface-4`     | `#1e1e1e` | — |
| `--border`        | `#1e1e1e` | Default dividers |
| `--border-2`      | `#252525` | Overlay panels |
| `--border-3`      | `#2a2a2a` | Button borders, subtle dividers |
| `--border-strong` | `#333`    | Key badges, Tweaks panel outline |
| `--text`          | `#ccc`    | Primary body text |
| `--text-strong`   | `#ddd`    | Emphasized text |
| `--text-dim`      | `#888`    | Secondary labels |
| `--text-faint`    | `#666`    | Tertiary |
| `--text-muted`    | `#555`    | Hint / placeholder |
| `--text-ghost`    | `#444`    | Lowest-contrast |
| `--key-bg`        | `#1e1e1e` | Keyboard key badge background |
| `--key-text`      | `#bbb`    | Keyboard key badge text |
| `--btn-bg`        | `#1a1a1a` | Inactive/secondary button bg |
| `--btn-border`    | `#2a2a2a` | Inactive/secondary button border |
| `--btn-text`      | `#777`    | Inactive/secondary button text |
| `--overlay-bg`    | `rgba(14,14,14,0.95)` | Floating panels (with backdrop-blur) |
| `--viewer-bg`     | radial-gradient dark-green | Viewer fallback when no stream |
| `--accent`        | `#4d9e6e` | Primary accent (muted green) |
| `--accent-dim`    | `rgba(77,158,110,0.10)` | Accent tint fills |
| `--accent-border` | `rgba(77,158,110,0.28)` | Accent tint borders |
| `--accent-glow`   | `0 0 10px rgba(77,158,110,0.25)` | Focus ring glow |
| `--accent-text`   | `#4d9e6e` | Accent text (same as --accent) |
| `--danger`        | `#a05050` | Destructive action text |
| `--danger-dim`    | `rgba(160,80,80,0.10)` | Destructive fill |
| `--danger-border` | `rgba(160,80,80,0.28)` | Destructive border |

#### Light theme
| Token | Value |
|---|---|
| `--bg`            | `#f6f7f8` |
| `--surface`       | `#ffffff` |
| `--surface-2`     | `#f9fafb` |
| `--surface-3`     | `#f1f3f5` |
| `--surface-4`     | `#e9ecef` |
| `--border`        | `#e5e7eb` |
| `--border-2`      | `#dde1e6` |
| `--border-3`      | `#d0d5dc` |
| `--border-strong` | `#c4cad2` |
| `--text`          | `#222` |
| `--text-strong`   | `#111` |
| `--text-dim`      | `#555` |
| `--text-faint`    | `#777` |
| `--text-muted`    | `#8a8f96` |
| `--text-ghost`    | `#a8adb4` |
| `--key-bg`        | `#eef0f3` |
| `--key-text`      | `#333` |
| `--btn-bg`        | `#f1f3f5` |
| `--btn-border`    | `#dde1e6` |
| `--btn-text`      | `#555` |
| `--overlay-bg`    | `rgba(255,255,255,0.97)` |
| `--viewer-bg`     | radial-gradient light-green | 
| `--accent`        | `#2f7a4f` |
| `--accent-dim`    | `rgba(47,122,79,0.10)` |
| `--accent-border` | `rgba(47,122,79,0.32)` |
| `--accent-glow`   | `0 0 10px rgba(47,122,79,0.20)` |
| `--accent-text`   | `#2f7a4f` |
| `--danger`        | `#b84242` |
| `--danger-dim`    | `rgba(184,66,66,0.08)` |
| `--danger-border` | `rgba(184,66,66,0.30)` |

### Status colors (for connection/stream status dot only)
Separate from accent tokens because they encode semantic state, not brand:

| Status | Dark hex | Light hex |
|---|---|---|
| connected | `#4d9e6e` | `#2f7a4f` |
| disconnected | `#a05050` | `#b84242` |
| connecting | `#a08a3a` | `#a07a1e` |
| streaming | `#4d7fc4` | `#2a6bc0` |

Persist the chosen theme to `localStorage` (key `mgstream-tweaks`, shape `{ theme, sidebarWidth }`).

---

## Typography

- **Font family**: `'Consolas', 'Monaco', 'Courier New', monospace` throughout. The technical/telemetry feel is intentional.
- **Sizes (px)**: 9 (micro labels, all-caps section headers), 10 (tertiary), 11 (body, buttons), 12 (primary labels, scene names), 13 (logo wordmark).
- **Weights**: 400 default, 600 emphasized, 700 for labels/logo/loaded-scene name.
- **Letter-spacing**: `0.03em`–`0.12em` for uppercase labels; default otherwise.

---

## Screens / Surfaces

### 1. Top Bar (height 40px)

Layout, left → right:
1. **Logo** — `M_GStream`, 13px, weight 700, `--accent` color.
2. **Active-panel pill** — when an activePanel is set, render a small pill with `--accent-dim` bg, `--accent-border`, `--accent` text. Shows icon + label:
   - `camera`: `⌨ Camera Control · Keyboard active`
   - `scenes`: `⊞ Browse Scenes · Panel active`
   - `viewer`: `▶ Viewer · Keyboard navigation`
3. **Scene chip** — middle-dot separator + truncated scene name, 11px `--text-dim`, max-width ~160px, ellipsis, `title` attr with full name for hover.
4. `flex: 1` spacer.
5. **Theme toggle** — 1x button, 28px tall, shows `☾` in dark mode, `☀` in light, flips the theme.
6. **Status dot + label** — `<StatusDot>` component: 8px dot + text label in the status color. The dot pulses (opacity 1 ↔ 0.4, 1.2s) when status is `connecting` or `streaming`. Has a matching-color `box-shadow: 0 0 6px`.
7. **Vertical divider** — 1px × 18px, `--border-3`.
8. **Stream button** — 
   - If streaming: accent green bg (`--accent-dim`) + accent border + accent text, label `Stop Stream`, with a 7×7 filled square icon in `--accent`.
   - If not streaming: neutral button (`--btn-bg`/`--btn-border`/`--btn-text`), label `▶ Start Stream`.
9. **WS button** —
   - If connected: accent green (`--accent-dim`/border/text), label `Disconnect WS`.
   - If disconnected: neutral, label `Connect WS`.

**Critical color semantics:** when a connection is live (stream running or WS connected), the button to turn it off is **green** (matches "currently healthy"). Red is reserved for destructive confirmation in the InfoPanel's Disconnect action, or for error/disconnected status indicators. This was a deliberate reversal of the common "destructive = red" pattern — the user wanted the button color to reflect current state, not action outcome.

### 2. Left Sidebar (width 220px default, range 160–300)

Header row: `CONTROLS` uppercase 9px, `--text-ghost`, bordered bottom.

Four stacked sections with gap 16px, padded 10px/12px:

#### § LOADED SCENE
Card with `--surface-2` bg, `--border-2` border, 6px radius, padding 8/10.
- Scene name, 12px weight 600, `--accent` color, truncated with `title` attr.
- Scene path, 10px `--text-muted`, truncated with `title` attr.
- Badge row: `loaded` pill (accent tint) + plain `model_dir` text in `--text-muted`.

#### § CAMERA CONTROL
Full-width button. States:
- **ON**: `--accent-dim` bg + accent border + `--accent` text, 6px dot in `--accent`. Label: `Camera Control ON`.
- **OFF**: `--btn-bg`/border/text, dot in `--text-ghost`. Label: `Camera Control OFF`.

### § SPEED
Two `<Slider>` components. Each has a row with label (11px `--text-dim`) left, value pill (11px weight 600, `--accent` text on `--accent-dim` bg, 3px radius, min-width 36px right-aligned) right, then an `<input type="range">` below styled with `accent-color: var(--accent)`.
- Move: min 0.01, max 5, step 0.01, default 0.44.
- Rotate (deg/s): min 1, max 180, step 1, default 20.

#### § KEYBOARD SHORTCUTS
List of key-group ↔ label rows:
- `W A S D` → Move
- `Q E` → World Y ±
- `↑ ↓ ← →` → Free look
- `Z X` → Roll
- Footer line (bordered top, `--text-ghost`, 10px): `Drag in viewer → Look sensitivity`.

Each key is a `<KeyBadge>`: `--key-bg` bg, `--border-strong` border, 3px radius, padding 1/5, 10px weight 600 `--key-text`.

### 3. Viewer (flex: 1)

Full flex-1 region. Background is `--viewer-bg` (a radial gradient using dark-green tones in dark theme, light-green in light). Overlay two subtle radial gradients on top at ~60% opacity to suggest rendered Gaussian splat haze — placeholder until real WebGL stream is wired in.

When `activePanel === 'viewer'`:
- Focus ring applied (accent box-shadow).
- Top-left: "ACTIVE" indicator pill with pulsing dot + text `VIEWER ACTIVE · Keyboard navigation enabled`.

Always:
- Bottom-left: small hint `Click to activate viewer keyboard navigation` in `--text-ghost`.
- Bottom-right: `~60 fps` counter in `--text-faint`.

Cursor: `crosshair` over the viewer area.

### 4. InfoPanel (top-right overlay)

Floating panel, 300px wide, `--overlay-bg` background with `backdrop-filter: blur(8px)`. 8px radius.

**Collapsed state**: renders as a 28×28 `ⓘ` button in the same position. Clicking expands it back.

**Expanded**: Three-tab header (`Connection`, `Camera`, `Scene`) + `×` close button (which collapses, not hides permanently).
Active tab: `--accent-dim` bg, `--accent` text, 2px bottom border in `--accent`.
Inactive: transparent bg, `--text-faint` text.

Tab contents:

#### Connection
- `HTTP Origin` field (read-only display, truncated in a `--surface-2` box, `title` shows full value).
- `WebSocket URL` field (same).
- Two buttons row: `Use Current Origin` (accent filled) + `Disconnect` (danger filled — this is one of the only places red is used, since it's a confirmation action).

#### Camera
- Header row: `CONTROL STATE` label + `ENABLED`/`DISABLED` pill.
- `POSITION (x, y, z)` — 3-col grid, monospace, 11px.
- `ROTATION (yaw, pitch, roll)` — same shape.
- Stats row: `Move`, `Rotate`, `FPS` columns (FPS in `--accent`).
- `Reset Camera` button at bottom (outline button, `--border-3`, `--text-dim`).
- When `cameraEnabled`, animate the position values subtly (tiny random drift every 400ms) to signal liveness. When disabled, freeze them.

#### Scene
- `Name` / `Path` fields.
- Three-column row: `Type` / `Gaussians` / `VRAM`.

### 5. ScenesBrowser (centered modal)

Width 520, max-height 360, `--overlay-bg`, `backdrop-filter: blur(10px)`, 10px radius, border `--border-2`. Positioned center-center via `transform: translate(-50%, -50%)`.

When `activePanel === 'scenes'`: focus ring + `ACTIVE` badge top-right.

Sections top→bottom:
1. **Search bar row** — text input with placeholder `Search scenes…` + 4 action buttons (`Search`, `Clear`, `Run`, `Up`) + `×` close.
2. **Title row** — `Content Browser` 12px weight 600 + `↺ Refresh` button (text-only).
3. **Scrollable list** of scene rows. Each row:
   - Loaded state: name in `--text`, weight 700, + small `LOADED` accent pill.
   - Selected state: row bg becomes `--accent-dim`, border `--accent-border`, name text `--accent`.
   - 3-button action cluster right-aligned: `Open` / `Select` / `Load` (neutral buttons).
   - Path on second line, `--text-muted` 9px, truncated.
   - Badge row: `directory` tag + model-dir tag.
4. **Selected-path footer** — shows 3 metadata columns (`SELECTED PATH`, `KIND`, `LOAD AS`) + big `Load Selected` accent button on the right.
5. **Results count** — 9px `--text-muted`: `Loaded N search results for "…" under /home/…`.

Clicking anywhere inside sets `activePanel = 'scenes'`. Buttons use `stopPropagation` to avoid bubbling when clicked.

### 6. Browse Scenes FAB

Bottom-center floating button, 7/18 padding, 8px radius, with a `⊞` icon. Toggles the ScenesBrowser modal. When the modal is open, the FAB is styled active (accent tint + "· active" suffix).

### 7. Tweaks Panel (bottom-right, only visible when host activates edit mode)

220px wide, `--surface-3` bg, `--border-strong` border, 10px radius, padding 16. Contents:
- `THEME` section: two buttons (`☾ Dark`, `☀ Light`). Selected one uses accent tint.
- `SIDEBAR WIDTH` section: range slider (160–300, step 10) + px label.

This is only needed if the host app has a Tweaks/edit-mode concept; otherwise skip it. It's present in the prototype for live-editing defaults.

---

## State Management

```
wsStatus:     'connected' | 'disconnected' | 'connecting' | 'streaming'
streamStatus: 'connected' | 'streaming'  (streaming implies wsStatus !== disconnected)
activePanel:  'camera' | 'viewer' | 'scenes'
cameraEnabled: boolean
moveSpeed:    number (0.01–5)
rotateSpeed:  number (1–180, integer)
showScenes:   boolean
theme:        'dark' | 'light'
sidebarWidth: number (160–300)

// Derived UI state (local to InfoPanel)
infoPanelOpen: boolean
infoPanelTab:  'connection' | 'camera' | 'scene'
```

**Persisted via localStorage:**
- `mgstream-state` → `{ activePanel }`
- `mgstream-tweaks` → `{ theme, sidebarWidth }`

**Interactions:**
- Clicking the Sidebar, Viewer, or ScenesBrowser sets `activePanel` to the clicked one. This drives the focus ring + top-bar pill + per-surface ACTIVE badge.
- Clicking `Camera Control ON/OFF` toggles `cameraEnabled`. Use `stopPropagation` so it doesn't also change `activePanel`.
- `Stop Stream` → flips `streamStatus`. (In the prototype, this just toggles; in real app, it should call the stream-stop endpoint and update on response.)
- `Connect WS` → sets `wsStatus='connecting'`, after delay → `'connected'`. (Prototype fakes with `setTimeout`; real app should use actual WS open/close events.)
- `Disconnect WS` → `wsStatus='disconnected'`.
- Theme toggle (both in top bar and in Tweaks) → swaps `theme` class on root container.
- Browse Scenes FAB toggles `showScenes` and sets `activePanel` to `scenes` on open / `camera` on close.
- When `cameraEnabled`, the Camera tab animates position values subtly (jitter every 400ms) to show liveness. In the real app, hook this to actual camera-pose messages from the server.

---

## Animations & Transitions

| Element | Property | Duration | Easing |
|---|---|---|---|
| Focus ring (`panel-focus-ring`) | box-shadow | 0.2s | ease |
| Active-panel pill, ACTIVE badge | opacity | 0.2s | ease (`fade-in`) |
| Status dot pulse (connecting/streaming) | opacity 1↔0.4 | 1.2s | ease-in-out infinite |
| InfoPanel, ConnectionPanel entrance | transform + opacity | 0.2s | ease (`slide-in-right`) |
| ScenesBrowser entrance | opacity | 0.15s | ease (`fade-in`) |
| Viewer ACTIVE dot pulse | opacity | 1.5s | infinite |
| Button state transitions | all | 0.12–0.2s | ease |

Do not add motion that wasn't in the prototype. The UI is dense; motion is purely for state-change feedback, not decoration.

---

## Component Mapping Cheatsheet

The prototype uses these top-level React components. The real codebase can merge or split as its conventions require:

- `<App>` — owns all state
- `<TopBar>`
- `<StatusDot>`
- `<Sidebar>` — left rail with Controls sections
- `<Viewer>` — the splat render area
- `<InfoPanel>` — the tabbed overlay (top-right)
- `<ScenesBrowser>` — the centered modal
- `<TweaksPanel>` — bottom-right config panel (optional)
- `<Slider>`, `<SectionHeader>`, `<KeyBadge>`, `<Field>` — small presentational primitives

---

## What Changed vs. Old UI (Summary of User Intent)

1. **Multiple colors → light/dark toggle only.** Keep a single muted green as the accent. No other hue options.
2. **Text overlaps fixed.** All long text fields use ellipsis + `title`-attr tooltips. Top-bar layout uses `flex:1` spacer + `flexShrink:0` on right-side pills. Scene paths truncate gracefully. Sections use explicit `gap` rather than margin collisions.
3. **Live-state buttons are green, not red.** When the WS is connected, `Disconnect WS` is green-tinted. When streaming, `Stop Stream` is green-tinted. Red (`--danger-*`) is reserved for the InfoPanel's explicit Disconnect button (a confirmation action) and for the `disconnected` status color.
4. **Foreground-panel indicator.** Three signals, all tied to the same `activePanel` state:
   - Accent-colored focus ring (+ glow) on the surface itself.
   - `ACTIVE` badge in the corner of the surface.
   - Pill in the top bar showing which surface is active and what that means for input.

## Assets
None. The prototype uses only text + CSS-generated shapes. No images, icons, or fonts beyond system monospace.

## Files
- `M-GStream.html` — full self-contained prototype.
- `README.md` — this document.
