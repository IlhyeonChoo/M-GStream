# `src/projects/extended_gaussian` M3 build surface 상세

이 문서는 M3에서 `renderer/server` 코드를 renderer 본체에서 분리하고, 명시적 server target으로 옮긴 빌드 구성을 정리한다.

## 디렉터리: `src/projects/extended_gaussian`

### 파일: `src/projects/extended_gaussian/CMakeLists.txt`

#### 초기 코드

```cmake
project(extended_gaussian_all)

add_subdirectory(apps)
add_subdirectory(renderer)

include(install_runtime)
subdirectory_target(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR} "projects/extended_gaussian")
```

#### 현재 코드

```cmake
project(extended_gaussian_all)

if(WIN32)
    option(SIBR_BUILD_REMOTE_STREAM "Build remote stream support targets for extended_gaussian" OFF)
else()
    option(SIBR_BUILD_REMOTE_STREAM "Build remote stream support targets for extended_gaussian" ON)
endif()
message(STATUS "extended_gaussian remote stream modules: ${SIBR_BUILD_REMOTE_STREAM}")

add_subdirectory(apps)
add_subdirectory(renderer)
```

#### 바뀐 이유

- M3부터는 remote stream 관련 코드를 항상 빌드하지 않고, top-level option으로 켜고 끌 수 있어야 했다.
- Linux에서는 이후 M4~M6을 바로 이어서 진행해야 하므로 기본값을 `ON`으로 두고, Windows는 아직 서버 경로를 검증하지 않았으므로 기본값을 `OFF`로 뒀다.
- configure 시점에 상태를 출력해 빌드 로그만 봐도 server 모듈이 활성화됐는지 바로 알 수 있게 했다.

## 디렉터리: `src/projects/extended_gaussian/renderer`

### 파일: `src/projects/extended_gaussian/renderer/CMakeLists.txt`

#### 초기 코드

```cmake
file(GLOB_RECURSE ALL_FILES
    "*.cpp" "*.h" "*.hpp"  "cuda/*.cu" "cuda/*.cuh"
    "shaders/*.frag" "shaders/*.vert" "shaders/*.geom"
)

add_library(${PROJECT_NAME} SHARED ${ALL_FILES})
...
set_target_properties(${PROJECT_NAME} PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
    CUDA_STANDARD 17
)
```

#### 현재 코드

```cmake
file(GLOB_RECURSE ALL_FILES
    "*.cpp" "*.h" "*.hpp"  "cuda/*.cu" "cuda/*.cuh"
    "shaders/*.frag" "shaders/*.vert" "shaders/*.geom"
)
file(GLOB_RECURSE SERVER_FILES "server/*.cpp" "server/*.h" "server/*.hpp")
list(REMOVE_ITEM ALL_FILES ${SERVER_FILES})

add_library(${PROJECT_NAME} SHARED ${ALL_FILES})
...
set_target_properties(${PROJECT_NAME} PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
    CUDA_STANDARD 17
)

if(SIBR_BUILD_REMOTE_STREAM)
    add_subdirectory(server)
endif()
```

#### 바뀐 이유

- 초기 상태에서는 `renderer/server/*.cpp`도 `ALL_FILES`에 암묵 포함돼서 `extended_gaussian` shared library 안으로 들어갔다.
- M3의 핵심은 server parser/adapter 코드의 target ownership을 renderer 본체에서 떼어내는 것이므로, 먼저 `SERVER_FILES`를 빼고 별도 하위 타깃으로 이동시켰다.
- 이 변경으로 viewer/renderer hot path와 server 준비 코드를 느슨하게 분리할 수 있게 됐다.

## 디렉터리: `src/projects/extended_gaussian/renderer/server`

### 파일: `src/projects/extended_gaussian/renderer/server/CMakeLists.txt`

#### 초기 코드

```text
기준 커밋 `0b509d4` 에는 파일이 없었다.
```

#### 현재 코드

```cmake
project(extended_gaussian_server)

set(SERVER_SOURCES
    CameraPoseAdapter.cpp
    CameraPoseAdapter.hpp
    Config.hpp
    ServerProtocol.cpp
    ServerProtocol.hpp
)

find_path(
    BOOST_BEAST_INCLUDE_DIR
    NAMES boost/beast/http.hpp
    HINTS ${Boost_INCLUDE_DIR} ${Boost_INCLUDE_DIRS}
)
if(NOT BOOST_BEAST_INCLUDE_DIR)
    message(FATAL_ERROR "SIBR_BUILD_REMOTE_STREAM requires Boost.Beast headers.")
endif()

add_library(${PROJECT_NAME} STATIC ${SERVER_SOURCES})
target_link_libraries(${PROJECT_NAME} PUBLIC
    sibr_assets
    sibr_system
)
...
find_path(TURBOJPEG_INCLUDE_DIR NAMES turbojpeg.h)
find_library(TURBOJPEG_LIBRARY NAMES turbojpeg libturbojpeg)
```

#### 바뀐 이유

- M3에서는 아직 HTTP server나 MJPEG encoder를 런타임에 붙이지 않는다. 대신 server 코드가 어디 소속인지 빌드 그래프를 먼저 고정해야 했다.
- 그래서 `extended_gaussian_server`를 **정적 라이브러리**로 만들고, 실제로 필요한 `ServerProtocol`/`CameraPoseAdapter`만 먼저 넣었다.
- HTTP/WebSocket backend는 `Boost.Beast`로 방향을 고정하되, JPEG 인코더는 M5에서 실제로 필요해지므로 M3에서는 probe만 하고 configure failure로 만들지 않았다.
- install/runtime 규칙을 붙이지 않은 이유도 같다. 이 단계의 목표는 shipped runtime이 아니라 build ownership 확정이다.

## 요약

- M3는 remote stream 기능을 붙인 단계가 아니라, 그 기능이 들어갈 **빌드 경계**를 먼저 만든 단계다.
- `extended_gaussian` 본체는 계속 viewer/renderer 역할만 유지하고, `renderer/server`는 `extended_gaussian_server` 정적 라이브러리로 분리됐다.
- 이후 M4부터는 이 server target 위에 lifecycle, HTTP endpoint, 브라우저 연결을 얹는 방식으로 확장하면 된다.
