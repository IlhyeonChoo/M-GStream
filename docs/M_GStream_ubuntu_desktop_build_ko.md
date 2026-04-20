# Extended Gaussian Ubuntu Desktop Build Guide

대상: Ubuntu desktop에서 Ninja로 `M_GStreamViewer_app`을 빌드하고 실행하려는 작업자.

최종 확인일: 2026-04-10

## 확인된 기준 환경

이번 desktop smoke는 아래 조합에서 통과했다.

| 항목 | 확인된 값 |
|---|---|
| OS | Ubuntu 24.04 계열 desktop |
| CMake | 3.28.3 |
| Ninja | 1.11.1 |
| GCC / G++ | 13.3 계열 |
| CUDA compiler | `/usr/local/cuda/bin/nvcc` |
| CUDA compiler version | 12.9.86 |
| NVIDIA driver | 590.48.01 |
| GPU smoke | NVIDIA GeForce RTX 4060 Ti |

주의: NVIDIA driver가 보고하는 "CUDA Version"과 `nvcc --version`의 compiler version은 다를 수 있다. CMake에는 실제로 사용할 `nvcc` 경로를 명시한다.

## 1. 필요한 패키지

새 Ubuntu desktop checkout에서 다음 패키지가 필요했다.

```sh
sudo apt update
sudo apt install -y \
  ninja-build \
  build-essential \
  cmake \
  git \
  pkg-config \
  libglew-dev \
  libglfw3-dev \
  libassimp-dev \
  libopencv-dev \
  libavcodec-dev \
  libavformat-dev \
  libavutil-dev \
  libswscale-dev \
  libavdevice-dev \
  libgtk-3-dev \
  libxxf86vm-dev \
  libembree-dev
```

CUDA toolkit은 apt 패키지와 별도로 설치되어 있을 수 있다. 이 checkout에서는 `/usr/local/cuda/bin/nvcc`를 사용했다.

```sh
/usr/local/cuda/bin/nvcc --version
ninja --version
cmake --version
```

## 2. Configure

repo root에서 실행한다.

```sh
cd /home/ilhyeonchu/Documents/GitHub/M_GStream
cmake -S . -B build-ninja -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CUDA_COMPILER=/usr/local/cuda/bin/nvcc
```

성공 기준:

```text
-- Configuring done
-- Generating done
-- Build files have been written to: .../build-ninja
```

## 3. Build

우선 사용자가 실행할 viewer target만 빌드한다.

```sh
cmake --build build-ninja --target M_GStreamViewer_app
```

에러를 처음 조사할 때는 병렬 출력을 줄인다.

```sh
cmake --build build-ninja --target M_GStreamViewer_app -- -j1
```

많은 compiler warning은 현재 upstream SIBR / vendored dependency에서도 나온다. 실패 여부는 `FAILED:`, `error:`, `/usr/bin/ld: cannot find ...`, `ninja: build stopped` 주변의 첫 fatal 진단으로 판단한다.

## 4. Install

`cmake --install build-ninja`는 install만 수행한다. install 대상에 필요한 target까지 빌드하려면 Ninja install target을 사용한다.

```sh
cmake --build build-ninja --target install
```

설치 결과 확인:

```sh
ls -la install/bin/M_GStreamViewer_app
ldd install/bin/M_GStreamViewer_app | grep 'not found'
```

두 번째 명령은 아무 줄도 출력하지 않는 것이 정상이다.

## 5. Run

Ubuntu desktop 세션에서 실행한다.

```sh
./install/bin/M_GStreamViewer_app
```

Manifest 기반 smoke를 바로 재현하려면 `--manifest`를 넘긴다.

```sh
./install/bin/M_GStreamViewer_app \
  --manifest manifests/mc_small_aerial_c36_neighbors_3x3_cpu36_gpu9.json
```

성공 기준:

- viewer window가 열린다.
- Scene / Resource 계열 GUI가 표시된다.
- 앱이 dynamic loader 오류 없이 시작한다.
- manifest smoke에서는 manifest의 Gaussian fields / scene instances가 GUI에서 확인되고 `"Gaussian View"`에 렌더된다.

현재 viewer는 모델을 학습하지 않는다. GUI의 `Import PLY`는 raw `.ply` 파일이 아니라 아래 구조를 가진 모델 디렉터리를 고르는 흐름이다.

```text
<modelPath>/
  cfg_args
  point_cloud/
    iteration_XXXX/
      point_cloud.ply
```

## 6. 이번 desktop build에서 만난 blocker

| 증상 | 원인 | 조치 |
|---|---|---|
| `ASSIMP wasn't found correctly` | ASSIMP 개발 패키지 없음 | `sudo apt install -y libassimp-dev` |
| `Package 'gtk+-3.0' ... not found` | nativefiledialog의 Linux GTK3 dependency 없음 | `sudo apt install -y libgtk-3-dev` |
| `FFMPEG_LIBAVDEVICE_LIBRARIES ... NOTFOUND` | FFmpeg avdevice 개발 패키지 없음 | `sudo apt install -y libavdevice-dev` |
| `/usr/bin/ld: cannot find -lXxf86vm` | Xxf86vm 개발 라이브러리 없음 | `sudo apt install -y libxxf86vm-dev` |
| `Embree headers not found` | Embree 개발 패키지 없음 | `sudo apt install -y libembree-dev` 후 configure 재실행 |
| `The CUDA compiler identification is unknown` | CMake cache에 잘못된 CUDA compiler path가 들어감 | 아래 cache 복구 명령 사용 |
| `build.ninja: loading 'CMakeFiles/rules.ninja'` | 실패한 configure 뒤에 build를 실행함 | configure를 성공시킨 뒤 build 실행 |

CUDA compiler cache가 깨졌을 때는 repo root에서 다시 configure한다.

```sh
cmake -S . -B build-ninja -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -UCMAKE_CUDA_COMPILER \
  -UCMAKE_CUDA_ARCHITECTURES \
  -DCMAKE_CUDA_COMPILER=/usr/local/cuda/bin/nvcc
```

## 7. 완료된 검증

2026-04-10 desktop session에서 다음을 확인했다.

```sh
cmake --build build-ninja --target M_GStreamViewer_app -- -j1
cmake --build build-ninja --target install
ldd install/bin/M_GStreamViewer_app
./install/bin/M_GStreamViewer_app
./install/bin/M_GStreamViewer_app \
  --manifest manifests/mc_small_aerial_c36_neighbors_3x3_cpu36_gpu9.json
```

결과:

| 항목 | 결과 |
|---|---|
| `M_GStreamViewer_app` target | PASS |
| Ninja `install` target | PASS |
| installed executable | `install/bin/M_GStreamViewer_app` 존재 |
| installed executable link check | PASS, 주요 SIBR / CUDA / Embree library resolved |
| GUI desktop startup | PASS |
| manifest runtime smoke | PASS, user-confirmed with `manifests/mc_small_aerial_c36_neighbors_3x3_cpu36_gpu9.json` |
