# Remote Stream Reference Client

This directory contains the browser client assets used by the Ubuntu remote browser stream mode.
As of M6, these files are installed and served by the runtime HTTP server.

## Scope

The current assets are meant to document and exercise the shared contract, not to serve as the final product UI.

The current runtime exposes:

- `/stream.mjpg` as an actual multipart MJPEG stream in M5
- `/control` as an actual WebSocket endpoint in M6

## Files

- `index.html`: reference page for stream preview, WebSocket connection, and payload formatting
- `app.js`: default endpoint derivation plus the same baseline camera-pose validation used by the current C++ parser
- `styles.css`: presentation-only styling for the reference page

## Design Tokens & UI Model

The presentation layer follows the M-GStream design handoff (`design_handoff_mgstream_ui/`).

- The root `<div id="app">` carries either `theme-dark` (default) or `theme-light`. All colors, borders, and surfaces are CSS variables defined under those selectors; components never use literal hex values.
- The single accent color is a muted green (`--accent`). Status colors (`connected/disconnected/connecting/streaming`) are separate semantic tokens used only for the status dot.
- "Live" state buttons (`Stop Stream` when streaming, `Disconnect WS` when connected) use the accent green tint, not red. Red (`--danger-*`) is reserved for the InfoPanel `Disconnect` confirmation and the disconnected status indicator.
- `activePanel` is one of `sidebar | viewer | scenes`. Clicking any of them sets the active surface, which gets a `.panel-focus-ring` accent ring and surfaces a labelled pill in the top bar.
- Persisted via `localStorage`: `mgstream-tweaks` = `{ theme, sidebarWidth }`, `mgstream-state` = `{ activePanel }`.

## Current Client Behavior

- The reference page derives `/stream.mjpg` and `/control` from the current origin by default.
- `app.js` formats `set_camera_pose` payloads, performs browser-side validation, and manages WebSocket open/close/send actions.
- The browser camera controller uses the current camera basis for yaw/pitch, so it can pass straight up and straight down without a pitch clamp.
- `Q` and `E` remain world-Y translation shortcuts.
- The status line is diagnostic only. It is not a state machine for the final product UX.

## Current `/control` Contract

The current runtime accepts only text WebSocket messages on `/control`.
The reference client assumes same-port, same-origin deployment and derives the default WebSocket URL from the current page origin.

- On connect, the server sends a `ready` JSON message.
- Valid `set_camera_pose` payloads return an `ack` JSON message.
- Invalid payloads return an `error` JSON message.
- Binary WebSocket messages are rejected.

The current control queue policy is `latest_only`.
At most one pending control message is kept between the server thread and the viewer main/update loop.

## Current Validation Rules

The formatter mirrors the current parser contract:

- `0 < fovy < pi`
- `forward` must be non-zero
- `up` must be non-zero
- `forward` and `up` must not be parallel or near-parallel

## Explicit Non-Goals

This directory does not yet provide:

- production UI flows
- authentication
- reconnect/backoff policy
- guarantees about the final browser UX
- binary control payloads
- richer control verbs beyond `set_camera_pose`

## Change Guidance

These assets are shipped as part of the server install tree, but they still serve as a thin reference client rather than the final product UX.
For M6, prefer narrow changes that preserve the current MJPEG and `/control` text-JSON contract. Larger UX changes should land as separate browser/runtime work.
