# Ubuntu 24.04 Toolchain Status

작성일: 2026-04-09
대상 브랜치: `develop/ubuntu24-desktop-server`
마일스톤: `M1 build bootstrap`

## 1. 현재 결론

Ubuntu 24.04에서 기존 GUI viewer의 configure / build / install은 통과했다.

다만 이 SSH 세션에는 desktop X11 display가 없어서 installed viewer의 GUI smoke는 완료하지 못했다.

현재 검증 상태:

| 항목 | 결과 |
|---|---|
| Ninja configure | PASS |
| `extended_gaussianViewer_app` build | PASS |
| `install` target | PASS |
| `cmake --install build-ninja` | PASS |
| installed binary dynamic link | PASS, `not found` 0개 |
| GUI smoke on real desktop display | NOT RUN, 현재 세션에 `DISPLAY` 없음 |
| startup under Xvfb | FAIL, Mesa GL context 생성 후 segfault |

## 2. 환경 스냅샷

현재 shell 기준:

| 항목 | 값 |
|---|---|
| OS | Ubuntu 24.04.4 LTS |
| CMake | 3.28.3 |
| Ninja | 1.11.1 |
| GCC / G++ | 13.3.0 |
| Python | 3.12.3 |
| selected CUDA compiler | `/usr/local/cuda-12.8/bin/nvcc` |
| selected CUDA version | CUDA 12.8 / V12.8.93 |
| GPU | NVIDIA RTX PRO 4500 Blackwell |
| driver | 580.126.09 |

Ubuntu packages observed by CMake / pkg-config:

| dependency | observed value |
|---|---|
| GLEW | 2.2.0 / `/usr/lib/x86_64-linux-gnu/libGLEW.so` |
| GLFW | 3.3.10 / `/usr/lib/x86_64-linux-gnu/libglfw.so.3.3` |
| ASSIMP | 5.3.0 |
| OpenCV | 4.6.0 |
| FFmpeg libavcodec | 60.31.102 |
| FFmpeg libavformat | 60.16.100 |
| Embree | 4.3.0 / `libembree4.so.4` |

## 3. Configure 기록

현재 성공한 configure command:

```sh
cmake -S . -B build-ninja -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_CUDA_COMPILER=/usr/local/cuda-12.8/bin/nvcc \
  -DCUDAToolkit_ROOT=/usr/local/cuda-12.8
```

결과:

- `build-ninja/build.ninja` 생성됨.
- `build-ninja/CMakeCache.txt`의 CUDA compiler는 `/usr/local/cuda-12.8/bin/nvcc`로 정렬됨.
- Embree는 Ubuntu package config를 통해 `/usr/lib/x86_64-linux-gnu/cmake/embree-4.3.0` 계열로 발견됨.

## 4. 재현했던 configure 실패

명시적으로 CUDA 12.8을 선택하지 않았을 때 CMake가 `/usr/bin/nvcc` wrapper를 선택했다.

그때의 compiler-id 단계는 CUDA 12.0 경로로 들어갔고, Ubuntu 24.04의 system headers와 조합되어 실패했다.

대표 fatal line:

```text
/usr/include/stdlib.h:141:8: error: '_Float32' does not name a type; did you mean '_Float16'?
```

M1에서는 CUDA 12.0 / `/usr/bin/nvcc`를 고치지 않았다.

대신 이 checkout의 canonical Ninja tree는 CUDA 12.8 compiler를 명시해서 configure했다.

## 5. Build / Install 검증

통과한 command:

```sh
cmake --build build-ninja --target CudaRasterizer -j2
cmake --build build-ninja --target sibr_video -j2
cmake --build build-ninja --target extended_gaussian -j4
cmake --build build-ninja --target extended_gaussianViewer_app -j4
cmake --build build-ninja --target install -j4
cmake --install build-ninja
```

설치 결과:

| check | result |
|---|---|
| `install/bin/extended_gaussianViewer_app` | executable exists |
| `install/bin/libextended_gaussian_rwdi.so` | exists |
| `install/ibr_resources.ini` | exists |
| `install/shaders/core` | exists |
| `install/shaders/extended_gaussian` | exists |
| `ldd install/bin/extended_gaussianViewer_app | rg 'not found'` | no matches |

주의:

- `cmake --install build-ninja`는 build를 수행하지 않는다.
- 처음 raw install을 실행했을 때 아직 `mrf`가 빌드되어 있지 않아 `libmrf_rwdi.so` 설치에서 실패했다.
- `cmake --build build-ninja --target install -j4`로 install 대상까지 빌드한 뒤 `cmake --install build-ninja`는 exit 0으로 통과했다.

## 6. M1에서 적용한 portability fixes

| 영역 | 변경 요지 |
|---|---|
| Embree | `find_package(embree CONFIG)`로 Ubuntu Embree 4 config를 수용하고, Raycaster에 Embree 3/4 query wrapper를 추가했다. |
| CUDA rasterizer | upstream external target에 CUDA `--pre-include=cstdint`를 추가했다. CUDA 12.8에서 upstream header가 기대한 fixed-width integer transitive include가 없었다. |
| renderer public headers | `extended_gaussian` target의 renderer directory를 PUBLIC include path로 내보냈다. Public header들이 `"Config.hpp"`를 포함한다. |
| FFmpeg encoder | legacy `AVStream::codec` / `avcodec_encode_video2` 경로를 explicit `AVCodecContext` + send/receive API로 옮겼다. |
| VideoUtils | 1-channel histogram mode loop가 vector element를 pair처럼 structured-binding하던 템플릿 오류를 index loop로 고쳤다. |
| GaussianLoader | `std::ifstream` / `FLT_MAX` 사용 header를 직접 포함했다. |
| GUI phase input | MSVC 전용 `strcpy_s`를 portable bounded `std::snprintf`로 대체했다. |

## 7. Runtime smoke 결과

현재 세션:

```text
DISPLAY=
WAYLAND_DISPLAY=
XDG_SESSION_TYPE=
```

No-display startup:

```sh
timeout 8s install/bin/extended_gaussianViewer_app --offscreen --nogui --width 320 --height 240 --vsync 0
```

결과:

```text
exit 134
X11: The DISPLAY environment variable is missing
```

`DISPLAY=:0` / `DISPLAY=:1` probe:

```text
exit 134
X11: Failed to open display :0
X11: Failed to open display :1
```

Xvfb startup:

```sh
xvfb-run -a timeout 8s install/bin/extended_gaussianViewer_app --nogui --width 320 --height 240 --vsync 0
```

결과:

```text
exit 139
OpenGL Version: 4.5 (Compatibility Profile) Mesa 25.2.8-0ubuntu0.24.04.1
Interactive camera using (0.09,1100) near/far planes.
Segmentation fault
```

해석:

- installed binary는 dynamic loader / resource lookup / GLFW init까지 진행했다.
- 현재 세션에는 real desktop NVIDIA OpenGL context가 없다.
- Xvfb는 Mesa OpenGL context로 시작되며, 이 viewer의 CUDA / OpenGL 전제와 동일한 smoke 환경으로 보지 않는다.
- M1 GUI smoke는 Ubuntu Desktop 또는 X11-forwarded NVIDIA GL 세션에서 재실행해야 한다.

## 8. Smoke model / manifest

앱에서 확인된 preload 인자는 `--manifest <json>`이다.

Raw Gaussian model directory를 받는 `--model` / `--data` / `--ply` 인자는 확인하지 못했다.

GUI import는 `Import PLY` 버튼이 directory picker를 열고, 선택된 model directory를 `GaussianLoader::load(path)`로 넘기는 방식이다.

현재 머신에서 찾은 작은 complete model 후보:

```text
/home/ilhyeonchu/ReCompose3D/3DGS/gaussian-grouping/output/smoke_teatime_iter1
```

구조:

```text
cfg_args
point_cloud/iteration_1/point_cloud.ply
```

모델 import / `"Gaussian View"` 렌더 smoke는 GUI display 접근 실패 때문에 아직 실행하지 않았다.

## 9. 다음 사람이 실행할 desktop smoke

real desktop session 또는 GPU-backed X11 forwarding에서:

```sh
cd /home/ilhyeonchu/ReCompose3D/3DGS/extended_gaussian-desktop-server
install/bin/extended_gaussianViewer_app --width 1280 --height 720 --vsync 0
```

확인할 것:

- window가 열린다.
- Scene Outliner / Resource Browser 메뉴가 열린다.
- `Import PLY`로 아래 디렉터리를 선택할 수 있다.

```text
/home/ilhyeonchu/ReCompose3D/3DGS/gaussian-grouping/output/smoke_teatime_iter1
```

- `Create New Instance` 후 `"Gaussian View"`에서 1 frame 이상 렌더한다.
- Escape 또는 window close로 종료한다.

## 10. M2 / M3으로 넘길 항목

- `--offscreen`이 현재 no-display GLFW init을 통과하지 못한다. M2 headless EGL에서 별도로 다룬다.
- Xvfb / Mesa GL에서의 segfault는 desktop-viewer 완료 조건으로 보지 않는다. 다만 headless work에서는 CUDA / GL interop를 명시적으로 분리해야 한다.
- `/usr/bin/nvcc` CUDA 12.0 wrapper가 남아 있다. 이 workspace에서는 CUDA 12.8 compiler path를 configure에 명시해야 한다.
- remote server, MJPEG, WebSocket, snapshot, OOM fix는 M1에서 시작하지 않았다.


## 11. 2026-04-13 remote-browser-stream M7 follow-up

주의: 이 문서는 원래 `develop/ubuntu24-desktop-server` 의 M1 build bootstrap 상태를 기록하기 위해 시작했다.
아래는 그 이후 `develop/ubuntu24-remote-browser-stream` 브랜치에서 같은 Ubuntu 24.04 호스트로 수행한 M7 loopback verification follow-up 이다.

이번 follow-up 에서 확인한 것:

- `cmake --build build-ninja --target extended_gaussianViewer_app --parallel` 통과
- `cmake --build build-ninja --target install --parallel` 통과
- installed binary direct headless EGL startup 통과
- installed binary `--headless --server --path ../gaussian-splatting/eval/bonsai` 로 remote stream bring-up 통과
- `/healthz`, `/`, `/app.js`, `/styles.css`, `GET /control` loopback smoke 통과
- single-client / two-client MJPEG 측정 통과
- WebSocket `ready/error/ack` 및 pose apply correlation 통과
- 120초 local soak 에서 RSS drift +28 KB (+0.005%)

이번 follow-up 에서 닫지 못한 것:

- browser executable 이 없는 현재 shell 환경에서는 실제 browser page open 과 browser-visible control-to-visible 을 증명하지 못했다.
- 1-hour soak 는 아직 수행하지 않았다.
- clean SHA 기준 rerun 도 아직 수행하지 않았다.

최신 상세 결과는 아래 문서를 기준으로 본다.

```text
docs/extended_gaussian_remote_browser_stream_verification_report_ko.md
```
