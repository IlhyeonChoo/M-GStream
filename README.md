# M_GStream

Languages: [English](README.md) | [한국어](README_ko.md)

M_GStream is a C++/CUDA Gaussian Splatting viewer/editor built on top of a pinned
SIBR fork. It does not train models. Instead, it loads pre-trained Gaussian Splatting
outputs from disk, keeps them as CPU assets, creates scene instances, uploads shared GPU
copies on demand, and renders composed scenes through an external CUDA rasterizer.

## Demo

[Watch the remote browser viewer demo](docs/demo.mp4)

The demo video is H.264 MP4, 1280x720, 30 FPS, and about 102 seconds long. It shows
the headless remote browser viewer workflow: starting the server, opening the browser,
loading models, moving the camera, replacing the current model, and unloading content.

## Quick Start

These commands assume the existing `build-ninja/` tree matches the current checkout path.
Run them from the repository root.

Build and install the viewer:

```sh
cmake --build build-ninja --target M_GStreamViewer_app --parallel
cmake --build build-ninja --target install --parallel
```

Start the headless remote browser viewer:

```sh
install/bin/M_GStreamViewer_app \
  --headless \
  --server \
  --listen-host 0.0.0.0 \
  --listen-port 8080 \
  --render-width 1280 \
  --render-height 720 \
  --stream-width 1280 \
  --stream-height 720 \
  --stream-fps 15
```

Open the browser:

```text
http://<host-lan-ip>:8080/
```

Use `http://127.0.0.1:8080/` when the browser runs on the same machine. Use
`--listen-host 127.0.0.1` for loopback-only access, and use `--listen-host 0.0.0.0`
only on a trusted LAN or VPN.

Browser workflow:

- Click `Open Stream`.
- Click `Connect WS`.
- Click `Browse Scenes`.
- Select a loadable model directory or manifest and click `Load Selected`.
- Turn `Camera Control ON` and move the camera with the browser controls.
- Select another model and click `Load Selected` to replace the current content.
- Click `Unload Current` to clear the loaded content.

To pre-load content at startup, append one of these options:

```text
--path <modelPath>
--manifest <manifest.json>
```

## What This Repository Does

- Load Gaussian Splatting model directories or manifest JSON files.
- Manage CPU assets, scene instances, and shared GPU caches.
- Render composed scenes in real time with CUDA.
- Provide a desktop viewer with ImGui-based controls.
- Provide headless snapshot rendering.
- Optionally provide an HTTP/MJPEG/WebSocket remote browser viewer that can browse
  host-visible model directories, load content, replace the current content, and unload it
  through the WebSocket control path.

## What This Repository Does Not Do

- Train Gaussian Splatting models.
- Replace the full dataset preprocessing pipeline.
- Contain the rasterizer kernel sources.

The rasterizer is fetched at configure time from
`graphdeco-inria/diff-gaussian-rasterization`.

## Input Data

### Model Directory

The viewer expects a trained Gaussian Splatting result directory with this structure:

```text
<modelPath>/
  cfg_args
  point_cloud/
    iteration_XXXX/
      point_cloud.ply
```

The loader reads `cfg_args`, selects the latest `iteration_*` directory, and imports
`point_cloud.ply`.

### Manifest JSON

The viewer can also load a manifest JSON file for phased or remote-controlled content loading:

```text
install/bin/M_GStreamViewer_app --manifest <manifest.json>
```

## Build

This repository often keeps out-of-source build trees such as `build/` and `build-ninja/`.
Reuse them if they already match the current checkout path.

If the repository directory is renamed or moved, recreate the build tree before rebuilding.
CMake caches absolute paths, and log macros such as `__FILE__` will continue to report the old
source path until the affected targets are reconfigured and rebuilt.

### Linux (Verified Path)

Configure:

```sh
cmake -S . -B build-ninja -G Ninja -DCMAKE_BUILD_TYPE=Release -DSIBR_BUILD_REMOTE_STREAM=ON
```

Build and install:

```sh
cmake --build build-ninja --target M_GStreamViewer_app --parallel
cmake --build build-ninja --target install --parallel
```

Main installed executable:

```text
install/bin/M_GStreamViewer_app
```

### Windows (Expected Path)

Configure:

```bat
cmake -S . -B build -G "Visual Studio 16 2019" -A x64
```

Build and install:

```bat
cmake --build build --config Release --target M_GStreamViewer_app
cmake --build build --config Release --target INSTALL
```

Main installed executable:

```text
install/bin/M_GStreamViewer_app.exe
```

### Existing Configured Build Trees

If the repository already contains a matching configured build tree, the canonical rebuild path is:

```sh
cmake --build build-ninja --target M_GStreamViewer_app --parallel
cmake --build build-ninja --target install --parallel
```

## Run

### Desktop Viewer

Load a model directory:

```text
install/bin/M_GStreamViewer_app --path <modelPath>
```

Load a manifest:

```text
install/bin/M_GStreamViewer_app --manifest <manifest.json>
```

On Windows, use `install/bin/M_GStreamViewer_app.exe` instead.

### Headless Snapshot

Render a single snapshot without opening a visible window:

```sh
install/bin/M_GStreamViewer_app \
  --headless \
  --render-width 1280 \
  --render-height 720 \
  --snapshot output.png \
  --path <modelPath>
```

### Headless Remote Browser Stream

Run the HTTP/MJPEG/WebSocket server in headless mode. This starts the server without
pre-loading content; use the browser content panel to choose a model directory or manifest
from the host filesystem.

```sh
install/bin/M_GStreamViewer_app \
  --headless \
  --server \
  --listen-host 0.0.0.0 \
  --listen-port 8080 \
  --render-width 1280 \
  --render-height 720 \
  --stream-width 1280 \
  --stream-height 720 \
  --stream-fps 15
```

Open the browser from the same machine at:

```text
http://127.0.0.1:8080/
```

Open the browser from another trusted LAN/VPN machine at:

```text
http://<host-lan-ip>:8080/
```

Useful endpoints:

- `/` - reference client page
- `/stream.mjpg` - MJPEG stream
- `/control` - WebSocket control channel
- `/healthz` - runtime health/status JSON
- `/api/fs/list` - host filesystem browser listing
- `/api/fs/search` - host filesystem search for loadable content
- `/api/fs/probe` - model directory or manifest probe

## Security Note for Remote Streaming

The remote stream server does not provide authentication or TLS.
Do not expose it directly to the public internet.
Use it only on loopback, a trusted LAN, or a trusted VPN.

## Relationship to SIBR

SIBR is part of the project architecture, but this README is intentionally project-centric.
Keep this section short because the build and runtime depend on a pinned SIBR fork, while the
full upstream SIBR manual is outside this project's scope.

In this repository:

- SIBR provides the shared window, view, camera, render-target, and application framework.
- `M_GStream` provides the Gaussian loader, scene/resource management, CUDA world-buffer assembly,
  swap/manifest logic, viewer UI, and remote streaming code.

The top-level CMake configuration fetches a pinned custom SIBR fork during configure.

Pinned SIBR dependency:

- Repository: `git@github.com:IlhyeonChoo/sibr_core.git`
- Commit: `29b3cfcb186148fe6037a1d0204e9a1bfb0c3eaf`

## Verified Environment

The current repository state has been verified in this environment:

- OS: Ubuntu 24.04
- Generator: Ninja
- Build type: Release
- CMake: 3.28.3
- C compiler: `/usr/bin/gcc-12`
- C++ compiler: `/usr/bin/g++-12`
- CUDA host compiler: `/usr/bin/g++-12`
- CUDA compiler: `/usr/local/cuda-12.8/bin/nvcc`
- CUDA toolkit: 12.8 (`nvcc 12.8.93`)
- Remote stream build option: `SIBR_BUILD_REMOTE_STREAM=ON`

This is the environment currently reflected by the checked build cache and installed binary in this
checkout.

## Expected Compatibility

The codebase also contains platform-specific paths that suggest likely compatibility beyond the
verified environment, but those should be treated as expected rather than confirmed unless you
re-test them.

- Windows 10/11 build scripts and packaging paths are present.
- Visual Studio 2019 is still referenced in project tooling and install scripts.
- Linux headless EGL rendering and remote browser streaming are first-class paths in the current
  CMake configuration.
- CMake 3.24 or newer is required by the top-level project.
- Python 3.8 or newer is expected for build-side scripts and utilities.
- NVIDIA GPU with CUDA support is required for rendering.

Remote stream support defaults:

- Linux: `SIBR_BUILD_REMOTE_STREAM=ON`
- Windows: `SIBR_BUILD_REMOTE_STREAM=OFF`

## Key Runtime Notes

- The viewer assumes a primary IBR subview named `"Gaussian View"`.
- `archive_system` and `UI_system` are placeholders; the implemented subsystem is `rendering_system`.
- The loader parses SH degree 0 through 3, but downstream world buffers are currently laid out for degree 3.
- The project keeps both per-asset shared GPU buffers and persistent world buffers reused across frames.

## Validation

There is no automated test suite, `ctest` target, or lint target in this repository.
Validate changes by launching `M_GStreamViewer_app` in the mode you changed and exercising the
relevant workflow directly.

## Related Documents

- `AGENTS.md`
- `docs/M_GStream_code_flow_phase0_ko.md`
- `docs/M_GStream_vs_sibr_viewer_ko.md`
- `docs/sibr_gaussian_swap_detailed_design.md`
- `docs/M_GStream_ubuntu24_remote_browser_stream_user_guide_ko.md`
