# `extended_gaussian` M1 build/bootstrap 상세

이 문서는 M1에서 `extended_gaussian` 프로젝트 자체를 Ubuntu 24에서 빌드하고 실행하기 위해 들어간 project-local 수정들을 정리한다.

## 디렉터리: `src/projects/extended_gaussian/renderer`

### 파일: `src/projects/extended_gaussian/renderer/CMakeLists.txt`

#### 초기 코드

```cmake
set(CNU_PROJECT "extended_gaussian")
project(${CNU_PROJECT} LANGUAGES CXX CUDA)
...
sibr_gitlibrary(TARGET CudaRasterizer ...)
find_package(CUDAToolkit REQUIRED)
...
add_library(${PROJECT_NAME} SHARED ${ALL_FILES})
...
set_target_properties(${PROJECT_NAME} PROPERTIES
    CUDA_ARCHITECTURES "${EXTENDED_GAUSSIAN_EFFECTIVE_CUDA_ARCHITECTURES}"
    CUDA_SEPARABLE_COMPILATION ON
    CUDA_STANDARD 17
)
```

#### 현재 코드

```cmake
set(CNU_PROJECT "extended_gaussian")
set(CMAKE_CUDA_RUNTIME_LIBRARY Shared)
set(EXTENDED_GAUSSIAN_CUDA_ARCHITECTURES ...)
set(CMAKE_CUDA_ARCHITECTURES "${EXTENDED_GAUSSIAN_EFFECTIVE_CUDA_ARCHITECTURES}")
message(STATUS "extended_gaussian CUDA architectures: ${EXTENDED_GAUSSIAN_EFFECTIVE_CUDA_ARCHITECTURES}")

project(${CNU_PROJECT} LANGUAGES CXX CUDA)
...
if(TARGET CudaRasterizer)
    target_compile_options(CudaRasterizer PRIVATE
        $<$<COMPILE_LANGUAGE:CUDA>:--pre-include=cstdint>
    )
endif()
...
add_library(${PROJECT_NAME} SHARED ${ALL_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
```

#### 바뀐 이유

- `project(... LANGUAGES CXX CUDA)`보다 먼저 CUDA arch와 runtime policy를 고정해 stale cache나 generator별 기본값에 덜 의존하도록 만들었다.
- upstream `CudaRasterizer`는 CUDA 12.8에서 정수 타입 include chain이 불안정할 수 있어, `--pre-include=cstdint`를 강제로 넣어 Ubuntu 빌드를 안정화했다.
- `target_include_directories(... PUBLIC ...)`를 명시해 renderer 내부 헤더들이 Linux 빌드에서도 일관되게 보이도록 했다.

### 파일: `src/projects/extended_gaussian/renderer/ExtendedGaussianViewer.cpp`

#### 초기 코드

```cpp
char phaseBuffer[128] = {};
strcpy_s(phaseBuffer, sizeof(phaseBuffer), _currentPhase.c_str());
if (ImGui::InputText("Current Phase", phaseBuffer, IM_ARRAYSIZE(phaseBuffer))) {
    _currentPhase = phaseBuffer;
}
```

#### 현재 코드

```cpp
#include <cstdio>
...
char phaseBuffer[128] = {};
std::snprintf(phaseBuffer, sizeof(phaseBuffer), "%s", _currentPhase.c_str());
if (ImGui::InputText("Current Phase", phaseBuffer, IM_ARRAYSIZE(phaseBuffer))) {
    _currentPhase = phaseBuffer;
}
```

#### 바뀐 이유

- `strcpy_s`는 Windows/MSVC 계열에서는 자연스럽지만, Ubuntu/GCC 경로에서는 바로 쓸 수 없다.
- M1에서는 UI 로직 자체를 바꾸지 않고, 같은 의미를 갖는 portable API인 `std::snprintf`로 교체했다.
- 이 파일의 대규모 diff는 줄바꿈/포맷 영향도 포함돼 있지만, 실제 portability 의미가 있는 기능 차이는 이 부분이 핵심이다.

## 디렉터리: `src/projects/extended_gaussian/renderer/resource`

### 파일: `src/projects/extended_gaussian/renderer/resource/GaussianLoader.hpp`

#### 초기 코드

```cpp
#include <boost/filesystem.hpp>
```

#### 현재 코드

```cpp
#include <boost/filesystem.hpp>

#include <cfloat>
#include <fstream>
```

#### 바뀐 이유

- `GaussianLoader` 구현이 의존하는 표준 헤더를 명시적으로 포함하지 않으면, Linux toolchain에서는 transitive include 운에 기대는 빌드가 된다.
- M1에서는 이런 숨은 의존도 같이 정리해 Ubuntu에서 헤더 해석이 깨지지 않게 했다.

## 요약

- M1의 project-local 수정은 `extended_gaussian` 자체를 Ubuntu 24에서 configure/build/install 가능한 상태로 만드는 데 초점이 있다.
- 핵심은 CUDA/CMake 초기화 순서, 외부 rasterizer 호환, Windows 전용 API 제거, 숨은 include 의존 정리다.
- 런타임 기능 추가보다는 “기존 viewer가 다른 toolchain에서도 그대로 돌도록 만들기”가 목적이었다.
