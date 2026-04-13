# Remote Stream Reference Client

This directory contains the browser client assets used by the Ubuntu remote browser stream mode.
As of M4/M5, these files are installed and served by the runtime HTTP server.

## Scope

The current assets are meant to document and exercise the shared contract, not to serve as the final product UI.

The current runtime exposes:

- `/stream.mjpg` as an actual multipart MJPEG stream in M5
- `/control` as a placeholder HTTP 426 response until M6

## Files

- `index.html`: reference page for stream preview, WebSocket connection, and payload formatting
- `app.js`: default endpoint derivation plus the same baseline camera-pose validation used by the current C++ parser
- `styles.css`: presentation-only styling for the reference page

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
- completed WebSocket control wiring before M6

## Change Guidance

These assets are now shipped as part of the server install tree, but they still serve as a thin reference client rather than the final product UX.
For M5, prefer narrow changes that preserve the current MJPEG/control contract. Larger UX changes should land with the later runtime control work.
