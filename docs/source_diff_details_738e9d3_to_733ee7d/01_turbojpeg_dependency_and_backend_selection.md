# TurboJPEG dependency / backend selection 상세

이 문서는 Linux에서 system package가 없어도 MJPEG 경로가 `TurboJPEG`를 사용할 수 있게 만든 dependency layer 변화를 정리한다.

## 파일: `cmake/linux/dependencies.cmake`

### 초기 코드

```cmake
include(sibr_library)

Win3rdPartyGlobalCacheAction()
...
sibr_gitlibrary(TARGET picojson
    GIT_REPOSITORY  "https://gitlab.inria.fr/sibr/libs/picojson.git"
    GIT_TAG         "7cf8feee93c8383dddbcb6b64cf40b04e007c49f"
)
```

### 현재 코드

```cmake
include(sibr_library)
include(ExternalProject)
```

```cmake
option(SIBR_FETCH_TURBOJPEG
    "Fetch and build libjpeg-turbo locally when a system TurboJPEG package is unavailable" ON)

set(SIBR_TURBOJPEG_ROOT_DIR "${CMAKE_SOURCE_DIR}/extlibs/libjpeg-turbo")
set(SIBR_TURBOJPEG_SOURCE_DIR "${SIBR_TURBOJPEG_ROOT_DIR}/libjpeg-turbo")
set(SIBR_TURBOJPEG_BINARY_DIR "${SIBR_TURBOJPEG_ROOT_DIR}/build")
set(SIBR_TURBOJPEG_INSTALL_DIR "${SIBR_TURBOJPEG_ROOT_DIR}/install")
set(SIBR_TURBOJPEG_LIBRARY_DIR "${SIBR_TURBOJPEG_BINARY_DIR}/lib")

find_path(SIBR_TURBOJPEG_INCLUDE_DIR NAMES turbojpeg.h
    HINTS "${SIBR_TURBOJPEG_INSTALL_DIR}/include")
find_library(SIBR_TURBOJPEG_LIBRARY NAMES turbojpeg libturbojpeg
    HINTS
        "${SIBR_TURBOJPEG_LIBRARY_DIR}"
        "${SIBR_TURBOJPEG_INSTALL_DIR}/lib"
        "${SIBR_TURBOJPEG_INSTALL_DIR}/lib64")
```

```cmake
if(SIBR_TURBOJPEG_INCLUDE_DIR AND SIBR_TURBOJPEG_LIBRARY)
    add_library(TurboJPEG::TurboJPEG UNKNOWN IMPORTED GLOBAL)
    set_target_properties(TurboJPEG::TurboJPEG PROPERTIES
        IMPORTED_LOCATION "${SIBR_TURBOJPEG_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SIBR_TURBOJPEG_INCLUDE_DIR}")
elseif(SIBR_FETCH_TURBOJPEG)
    ExternalProject_Add(sibr_turbojpeg_external
        GIT_REPOSITORY "https://github.com/libjpeg-turbo/libjpeg-turbo.git"
        GIT_TAG "2.1.5"
        SOURCE_DIR "${SIBR_TURBOJPEG_SOURCE_DIR}"
        BINARY_DIR "${SIBR_TURBOJPEG_BINARY_DIR}"
        INSTALL_DIR "${SIBR_TURBOJPEG_INSTALL_DIR}"
        CMAKE_ARGS
            -DENABLE_SHARED=ON
            -DENABLE_STATIC=OFF
            -DWITH_TURBOJPEG=ON
            -DWITH_SIMD=OFF
            -DCMAKE_INSTALL_LIBDIR=lib)

    add_library(TurboJPEG::TurboJPEG UNKNOWN IMPORTED GLOBAL)
    set_target_properties(TurboJPEG::TurboJPEG PROPERTIES
        IMPORTED_LOCATION "${SIBR_TURBOJPEG_LIBRARY_DIR}/libturbojpeg.so"
        INTERFACE_INCLUDE_DIRECTORIES "${SIBR_TURBOJPEG_INSTALL_DIR}/include")
    add_dependencies(TurboJPEG::TurboJPEG sibr_turbojpeg_external)
endif()
```

### 바뀐 이유

- M5의 JPEG path는 `TurboJPEG`가 있으면 사용하고 없으면 `OpenCV` fallback으로 내려가게 되어 있었다.
- 그런데 Ubuntu 24.04 작업 환경에는 `libturbojpeg-dev`가 없었고, 이 프로젝트는 package manager 전제를 강하게 둘 수 없었다.
- 그래서 `renderer/server` 안에서 ad hoc `find_path` / `find_library`를 반복하는 대신, Linux dependency layer에서 `TurboJPEG::TurboJPEG` imported target을 표준화했다.
- `ExternalProject_Add()`를 쓴 이유는 이 저장소가 이미 configure 단계에서 외부 dependency를 가져오는 패턴을 갖고 있고, M5 이후 단계에서도 같은 target 이름으로 재사용할 수 있어야 했기 때문이다.
- `IMPORTED_LOCATION`을 build tree의 `lib/libturbojpeg.so`로 둔 이유는 vendored build 직후 link가 바로 가능해야 하기 때문이다. install tree만 바라보면 external project build 후 첫 app link가 깨진다.

## 파일: `src/projects/extended_gaussian/renderer/server/CMakeLists.txt`

### 초기 코드

```cmake
find_path(TURBOJPEG_INCLUDE_DIR NAMES turbojpeg.h)
find_library(TURBOJPEG_LIBRARY NAMES turbojpeg libturbojpeg)
if(TURBOJPEG_INCLUDE_DIR AND TURBOJPEG_LIBRARY)
    target_include_directories(${PROJECT_NAME} PRIVATE ${TURBOJPEG_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${TURBOJPEG_LIBRARY})
    target_compile_definitions(${PROJECT_NAME} PRIVATE SIBR_EXTENDED_GAUSSIAN_TURBOJPEG_AVAILABLE=1)
    message(STATUS "extended_gaussian_server JPEG backend: TurboJPEG found (${TURBOJPEG_LIBRARY})")
else()
    message(WARNING "extended_gaussian_server JPEG backend: TurboJPEG not found. Falling back to OpenCV JPEG encoding for M5.")
endif()
```

### 현재 코드

```cmake
if(TARGET TurboJPEG::TurboJPEG)
    target_link_libraries(${PROJECT_NAME} PRIVATE TurboJPEG::TurboJPEG)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SIBR_EXTENDED_GAUSSIAN_TURBOJPEG_AVAILABLE=1)
    message(STATUS "extended_gaussian_server JPEG backend: TurboJPEG enabled")
else()
    message(WARNING "extended_gaussian_server JPEG backend: TurboJPEG not found. Falling back to OpenCV JPEG encoding for M5.")
endif()
```

### 바뀐 이유

- `extended_gaussian_server`가 dependency 조달을 직접 책임지면, configure summary와 실제 링크 입력이 서로 어긋나기 쉽다.
- `TurboJPEG::TurboJPEG` target으로 올리면 `renderer/server`는 backend 선택만 신경 쓰고, 조달 방식은 Linux dependency layer에 일원화된다.
- 이 구조로 바꾸면 이후 M6/M7에서 `jpeg_backend=TurboJPEG` 여부를 healthz나 log로 검증할 때도 기준이 분명해진다.

## 요약

- 이번 수정으로 Linux에서는 system package가 없어도 configure/build 과정에서 vendored `libjpeg-turbo`를 준비할 수 있게 됐다.
- `extended_gaussian_server`는 이제 ad hoc 탐색이 아니라 공용 imported target을 소비한다.
- 따라서 M5의 JPEG backend는 환경 의존 fallback이 아니라, repo 안에서 재현 가능한 `TurboJPEG` 우선 경로로 정리됐다.
