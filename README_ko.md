# M_GStream

언어: [English](README.md) | [한국어](README_ko.md)

M_GStream은 pinned SIBR fork 위에 만든 C++/CUDA 기반 Gaussian Splatting
뷰어/에디터입니다. 모델을 학습하지 않습니다. 대신 사전 학습된 Gaussian Splatting
결과를 디스크에서 읽어 CPU 에셋으로 보관하고, 씬 인스턴스를 만들고, 필요할 때
공유 GPU 복사본을 업로드한 뒤 외부 CUDA 래스터라이저로 합성된 씬을 렌더링합니다.

## 데모

[원격 브라우저 뷰어 데모 보기](docs/demo.mp4)
https://github.com/user-attachments/assets/eef6f69f-a3d4-4069-9907-861f35ba1793

데모 영상은 H.264 MP4, 1280x720, 30 FPS, 약 102초 길이입니다. 영상은 headless
원격 브라우저 뷰어의 기본 흐름을 보여줍니다: 서버 실행, 브라우저 접속, 모델 로드,
카메라 조작, 현재 모델 교체, content 언로드.

## 빠른 시작

아래 명령은 기존 `build-ninja/` 빌드 트리가 현재 checkout 경로와 맞는다고 가정합니다.
명령은 저장소 루트에서 실행합니다.

뷰어를 빌드하고 설치합니다.

```sh
cmake --build build-ninja --target M_GStreamViewer_app --parallel
cmake --build build-ninja --target install --parallel
```

headless 원격 브라우저 뷰어를 실행합니다.

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

브라우저에서 접속합니다.

```text
http://<host-lan-ip>:8080/
```

브라우저가 같은 머신에서 실행 중이면 `http://127.0.0.1:8080/`를 사용합니다.
loopback 전용으로 열려면 `--listen-host 127.0.0.1`을 사용하고,
`--listen-host 0.0.0.0`은 신뢰할 수 있는 LAN 또는 VPN에서만 사용하세요.

브라우저 사용 흐름:

- `Open Stream`을 클릭합니다.
- `Connect WS`를 클릭합니다.
- `Browse Scenes`를 클릭합니다.
- 로드 가능한 모델 디렉터리 또는 manifest를 선택하고 `Load Selected`를 클릭합니다.
- `Camera Control ON`으로 전환하고 브라우저 컨트롤로 카메라를 움직입니다.
- 다른 모델을 선택하고 `Load Selected`를 클릭하면 현재 content가 교체됩니다.
- `Unload Current`를 클릭하면 현재 content가 언로드됩니다.

시작 시점에 content를 미리 로드하려면 아래 옵션 중 하나를 추가합니다.

```text
--path <modelPath>
--manifest <manifest.json>
```

## 이 저장소가 하는 일

- Gaussian Splatting 모델 디렉터리 또는 manifest JSON 파일을 로드합니다.
- CPU 에셋, 씬 인스턴스, 공유 GPU 캐시를 관리합니다.
- CUDA로 합성된 씬을 실시간 렌더링합니다.
- ImGui 기반 컨트롤을 가진 desktop viewer를 제공합니다.
- headless snapshot 렌더링을 제공합니다.
- 선택적으로 HTTP/MJPEG/WebSocket 기반 원격 브라우저 뷰어를 제공합니다. 이 뷰어는
  호스트에서 보이는 모델 디렉터리를 탐색하고, content를 로드/교체/언로드할 수 있으며,
  이 제어는 WebSocket control path를 통해 전달됩니다.

## 이 저장소가 하지 않는 일

- Gaussian Splatting 모델을 학습하지 않습니다.
- 전체 dataset preprocessing pipeline을 대체하지 않습니다.
- 래스터라이저 커널 소스를 저장소에 포함하지 않습니다.

래스터라이저는 configure 시점에 `graphdeco-inria/diff-gaussian-rasterization`에서
fetch됩니다.

## 입력 데이터

### 모델 디렉터리

뷰어는 학습된 Gaussian Splatting 결과 디렉터리가 아래 구조를 가진다고 가정합니다.

```text
<modelPath>/
  cfg_args
  point_cloud/
    iteration_XXXX/
      point_cloud.ply
```

로더는 `cfg_args`를 읽고, 최신 `iteration_*` 디렉터리를 선택한 뒤
`point_cloud.ply`를 import합니다.

### Manifest JSON

단계별 또는 원격 제어 기반 content loading을 위해 manifest JSON 파일도 로드할 수 있습니다.

```text
install/bin/M_GStreamViewer_app --manifest <manifest.json>
```

## 빌드

이 저장소에는 `build/`, `build-ninja/` 같은 out-of-source 빌드 트리가 존재할 수
있습니다. 기존 빌드 트리가 현재 checkout 경로와 맞으면 재사용하세요.

저장소 디렉터리를 이동하거나 이름을 바꿨다면 빌드 트리를 다시 configure해야 합니다.
CMake cache에는 절대 경로가 남기 때문에, 재configure/rebuild 전에는 `__FILE__` 같은
로그 매크로가 이전 source path를 계속 보고할 수 있습니다.

### Linux (검증된 경로)

Configure:

```sh
cmake -S . -B build-ninja -G Ninja -DCMAKE_BUILD_TYPE=Release -DSIBR_BUILD_REMOTE_STREAM=ON
```

Build and install:

```sh
cmake --build build-ninja --target M_GStreamViewer_app --parallel
cmake --build build-ninja --target install --parallel
```

설치된 기본 실행 파일:

```text
install/bin/M_GStreamViewer_app
```

### Windows (예상 경로)

Configure:

```bat
cmake -S . -B build -G "Visual Studio 16 2019" -A x64
```

Build and install:

```bat
cmake --build build --config Release --target M_GStreamViewer_app
cmake --build build --config Release --target INSTALL
```

설치된 기본 실행 파일:

```text
install/bin/M_GStreamViewer_app.exe
```

### 기존 configure 빌드 트리

현재 checkout과 맞는 configure된 빌드 트리가 이미 있다면 표준 rebuild 경로는 아래와 같습니다.

```sh
cmake --build build-ninja --target M_GStreamViewer_app --parallel
cmake --build build-ninja --target install --parallel
```

## 실행

### Desktop Viewer

모델 디렉터리 로드:

```text
install/bin/M_GStreamViewer_app --path <modelPath>
```

Manifest 로드:

```text
install/bin/M_GStreamViewer_app --manifest <manifest.json>
```

Windows에서는 `install/bin/M_GStreamViewer_app.exe`를 사용합니다.

### Headless Snapshot

보이는 창 없이 단일 snapshot을 렌더링합니다.

```sh
install/bin/M_GStreamViewer_app \
  --headless \
  --render-width 1280 \
  --render-height 720 \
  --snapshot output.png \
  --path <modelPath>
```

### Headless Remote Browser Stream

headless mode에서 HTTP/MJPEG/WebSocket 서버를 실행합니다. 이 명령은 content를 미리
로드하지 않고 서버를 시작합니다. 브라우저 content panel에서 호스트 파일시스템의
모델 디렉터리 또는 manifest를 선택해 로드합니다.

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

같은 머신에서 브라우저를 열 때:

```text
http://127.0.0.1:8080/
```

다른 신뢰할 수 있는 LAN/VPN 머신에서 브라우저를 열 때:

```text
http://<host-lan-ip>:8080/
```

주요 endpoint:

- `/` - reference client page
- `/stream.mjpg` - MJPEG stream
- `/control` - WebSocket control channel
- `/healthz` - runtime health/status JSON
- `/api/fs/list` - host filesystem browser listing
- `/api/fs/search` - loadable content 검색
- `/api/fs/probe` - 모델 디렉터리 또는 manifest probe

## Remote Streaming 보안 주의

remote stream server는 인증이나 TLS를 제공하지 않습니다.
public internet에 직접 노출하지 마세요.
loopback, 신뢰할 수 있는 LAN, 신뢰할 수 있는 VPN에서만 사용하세요.

## SIBR와의 관계

SIBR는 프로젝트 아키텍처의 일부이지만, 이 README는 M_GStream 중심으로 작성되어 있습니다.
빌드와 런타임이 pinned SIBR fork에 의존하므로 이 섹션은 짧게 유지하고, upstream SIBR
manual 전체를 중복하지 않습니다.

이 저장소에서의 역할:

- SIBR는 공유 window, view, camera, render-target, application framework를 제공합니다.
- `M_GStream`은 Gaussian loader, scene/resource management, CUDA world-buffer assembly,
  swap/manifest logic, viewer UI, remote streaming code를 제공합니다.

최상위 CMake 설정은 configure 시점에 pinned custom SIBR fork를 fetch합니다.

Pinned SIBR dependency:

- Repository: `git@github.com:IlhyeonChoo/sibr_core.git`
- Commit: `29b3cfcb186148fe6037a1d0204e9a1bfb0c3eaf`

## 검증된 환경

현재 저장소 상태는 아래 환경에서 검증되었습니다.

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

이 환경은 현재 checkout의 checked build cache와 installed binary에 반영된 상태입니다.

## 예상 호환성

코드베이스에는 검증된 환경 외의 호환성을 시사하는 platform-specific 경로도 있습니다.
하지만 직접 재검증하기 전까지는 confirmed가 아니라 expected로 취급하세요.

- Windows 10/11 build script와 packaging path가 존재합니다.
- Visual Studio 2019는 아직 project tooling과 install script에서 참조됩니다.
- Linux headless EGL rendering과 remote browser streaming은 현재 CMake 설정에서
  first-class path입니다.
- 최상위 프로젝트는 CMake 3.24 이상을 요구합니다.
- build-side script와 utility에는 Python 3.8 이상이 예상됩니다.
- 렌더링에는 CUDA 지원 NVIDIA GPU가 필요합니다.

Remote stream support 기본값:

- Linux: `SIBR_BUILD_REMOTE_STREAM=ON`
- Windows: `SIBR_BUILD_REMOTE_STREAM=OFF`

## 주요 런타임 참고 사항

- 뷰어는 `"Gaussian View"`라는 이름의 primary IBR subview를 가정합니다.
- `archive_system`과 `UI_system`은 placeholder이며, 실제 구현된 subsystem은
  `rendering_system`입니다.
- 로더는 SH degree 0부터 3까지 parse하지만, downstream world buffer는 현재 degree 3
  layout으로 구성되어 있습니다.
- 프로젝트는 asset별 공유 GPU buffer와 frame 간 재사용되는 persistent world buffer를
  모두 유지합니다.

## 검증

이 저장소에는 자동 테스트 스위트, `ctest` target, lint target이 없습니다.
변경한 mode에서 `M_GStreamViewer_app`을 직접 실행하고 관련 workflow를 직접 확인하세요.

## 관련 문서

- [`AGENTS.md`](../AGENTS.md)
- [`M_GStream_code_flow_phase0_ko.md`](M_GStream_code_flow_phase0_ko.md)
- [`M_GStream_vs_sibr_viewer_ko.md`](M_GStream_vs_sibr_viewer_ko.md)
- [`sibr_gaussian_swap_detailed_design.md`](sibr_gaussian_swap_detailed_design.md)
- [`M_GStream_ubuntu24_remote_browser_stream_user_guide_ko.md`](M_GStream_ubuntu24_remote_browser_stream_user_guide_ko.md)
