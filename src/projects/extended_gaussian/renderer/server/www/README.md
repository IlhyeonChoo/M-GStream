# Remote Stream Reference Client

This directory contains a protocol-reference browser client for the future Ubuntu remote browser stream mode.
It is intentionally not wired into the runtime yet.

## Scope

The current assets are meant to document and exercise the shared contract, not to serve as the final product UI.

They assume a future server will expose:

- `/stream.mjpg`
- `/control`

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
- runtime packaging/install integration
- guarantees about the final browser UX

## Change Guidance

While this branch is still in the contract-first stage, prefer adding notes or separate documentation over heavily rewriting the HTML/CSS/JS.
Once a later branch promotes these files into actual shipped assets, that branch can revise the UI with the runtime context in hand.
