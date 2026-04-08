# Ubuntu 24.04 Toolchain Status for Remote Browser Stream Follow-up

작성일: 2026-04-08  
대상 브랜치: `develop/ubuntu24-remote-browser-stream`

## 1. 목적

이 문서는 현재 저장소에 남아 있는 `build-ninja/` partial configure 산출물을 기준으로,
Ubuntu 24.04 계열 환경에서 remote browser stream 후속 브랜치가 먼저 확인해야 할
툴체인 상태를 고정하기 위한 기록이다.

핵심 목적은 다음 두 가지다.

- 현재 브랜치의 신규 `renderer/server/` 코드가 아니라 기존 CUDA toolchain bring-up이
  선행 블로커라는 점을 분리한다.
- 이후 headless EGL / HTTP / MJPEG / WebSocket 구현 브랜치가
  "새 코드 문제"와 "기존 configure 문제"를 빠르게 구분하게 한다.

## 2. 현재 워크스페이스에서 확인한 근거

### 2.1 `build-ninja/` 상태

현재 `build-ninja/`에는 다음이 남아 있다.

- `CMakeCache.txt`
- `CMakeFiles/CMakeConfigureLog.yaml`
- `CMakeFiles/3.28.3/CompilerIdCUDA/`

반대로 다음 파일은 없다.

- `build-ninja/build.ninja`

즉, configure는 시작되었지만 generator 출력이 완성되기 전에 중단된 상태로 본다.

### 2.2 cache / configure log 기준 환경 스냅샷

현재 남아 있는 산출물에서 직접 확인되는 값은 다음과 같다.

- generator
  - `Ninja`
- C++ compiler
  - `/usr/bin/c++`
- CUDA compiler
  - `/usr/bin/nvcc`
- CMake compiler-id 로그에 기록된 `nvcc`
  - `Cuda compilation tools, release 12.0, V12.0.140`
- CMake compiler / OpenMP 로그에 기록된 host compiler
  - GCC `13.3.0 (Ubuntu 13.3.0-6ubuntu2~24.04.1)`
- configure log 상 system string
  - `Linux - 10.0.15063.0 - x86_64`

## 3. 현재 로그가 보여주는 마지막 확인 지점

현재 저장소에 남아 있는 증거만 기준으로 하면,
configure는 `src/projects/extended_gaussian/renderer/CMakeLists.txt:2 (project)`에 들어간 뒤
`CMakeDetermineCUDACompiler.cmake:75`의 CUDA compiler vendor / identification 단계까지는
도달한 흔적이 남아 있다.

`CMakeConfigureLog.yaml`의 마지막 CUDA 관련 메시지는 아래 의미를 가진다.

- CMake가 `/usr/bin/nvcc`를 실제로 호출했다.
- `nvcc` 자체는 실행되어 NVIDIA CUDA compiler driver로 인식되었다.
- 그러나 configure log에는 그 이후 단계까지 진행되었다고 확정할 보존 증거가 남아 있지 않다.
- 최종 generator 파일인 `build.ninja`도 생성되지 않았다.

이 상태를 현재 기준으로 가장 보수적으로 정리하면 다음과 같다.

- **persisted evidence가 보여주는 마지막 확실한 CUDA 관련 지점은 compiler identification 단계다.**
- **top-level configure는 그 다음 단계로 넘어가 완결되지 못했다.**

## 4. 무엇이 확실하고, 무엇은 아직 확실하지 않은가

### 4.1 확실한 사실

- 현재 브랜치의 신규 `renderer/server/` 문서/계약 파일은 configure 이전 단계 산출물과는 분리되어 있다.
- partial configure tree는 project source compile 이전에 이미 CUDA toolchain bring-up 구간에 걸려 있다.
- `CompilerIdCUDA/tmp/` 산출물이 남아 있으므로
  CMake는 적어도 CUDA compiler-id용 임시 소스 생성과 일부 `nvcc` 호출까지 수행했다.

### 4.2 아직 로그만으로 확정할 수 없는 점

- 이번 워크스페이스에 남은 파일만으로는 당시 terminal stderr 전체가 보존되어 있지 않다.
- 따라서 "정확히 어떤 한 줄의 fatal error가 마지막으로 출력되었는가"는
  현재 보존된 build tree만으로 재현 없이 단정할 수 없다.

이 문서에서는 위 한계를 명시한 상태로,
후속 브랜치가 **실제 기능 코드보다 먼저 CUDA configure 문제를 확인해야 한다는 사실**만 고정한다.

## 5. 현재 브랜치 코드와 분리해서 봐야 하는 이유

`renderer/server/` 선행 작업은 다음 범위에만 머물러 있다.

- server option parser
- control message parser
- remote camera pose adapter
- browser reference client static assets

이 파일들은 아직 다음과 연결되지 않았다.

- `main.cpp`
- `ExtendedGaussianViewer`
- `RenderingSystem`
- `GaussianView`
- actual HTTP / MJPEG / WebSocket runtime

따라서 configure가 CUDA bring-up에서 막힌 상태라면,
그것은 현재 브랜치의 server prep 코드가 아니라
기존 renderer / CUDA toolchain 경로를 먼저 분리해서 봐야 한다는 뜻이다.

## 6. 후속 브랜치에서 가장 먼저 다시 확인할 항목

다음 브랜치가 실제 headless / server runtime 작업을 시작하기 전에,
최소한 아래 조합을 함께 기록하는 편이 좋다.

- `cmake --version`
- `nvcc --version`
- `c++ --version`
- `ldd --version`
- `echo $CUDAHOSTCXX`
- `echo $CUDACXX`
- `echo $CC`
- `echo $CXX`

추가로 아래 조건도 같이 확인한다.

- `/usr/bin/nvcc`가 어떤 CUDA toolkit 패키지에 연결되어 있는지
- host GCC가 CUDA 12.0과 호환 범위 안에 있는지
- configure 후 `build-ninja/build.ninja`가 실제로 생성되는지
- `CMakeConfigureLog.yaml`가 CUDA compiler-id 이후 단계까지 이어지는지

## 7. exact failure line이 필요할 때의 권장 캡처

현재 partial build tree만으로는 exact stderr가 남아 있지 않으므로,
후속 브랜치에서 정말 한 줄 단위 실패 메시지를 남겨야 할 때는
configure를 다시 실행할 때 terminal 출력까지 함께 캡처해야 한다.

예시:

```sh
cmake -S . -B build-ninja -G Ninja
```

그리고 아래 두 종류를 함께 보관한다.

- terminal stderr / stdout transcript
- `build-ninja/CMakeFiles/CMakeConfigureLog.yaml`

## 8. 지금 이 문서가 고정하는 결론

- 현재 브랜치의 후속 작업을 막는 첫 번째 외부 블로커는
  remote browser stream runtime 구현이 아니라 **CUDA configure bring-up**이다.
- 현재 저장소에 남아 있는 증거만 보면,
  configure는 `renderer` 서브프로젝트 진입 후
  **CUDA compiler identification 단계까지는 진행된 흔적이 있고**,
  최종 generator 파일인 `build.ninja`는 생성되지 않았다.
- 따라서 다음 기능 브랜치는 네트워크 서버 코드보다 먼저
  Ubuntu 24.04 + `/usr/bin/nvcc` + GCC 13 계열 조합이
  이 저장소의 configure를 끝까지 통과시키는지 재확인해야 한다.
