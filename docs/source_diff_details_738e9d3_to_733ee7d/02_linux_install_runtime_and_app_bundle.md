# Linux install runtime / app bundle 상세

이 문서는 installed viewer가 실제로 `libturbojpeg.so.0`와 `www/`를 포함하도록 Linux install 경로를 바로잡은 변화를 정리한다.

## 파일: `cmake/linux/install_runtime.cmake`

### 1. `installPDB` macro

#### 초기 코드

```cmake
macro(installPDB targetName configType)
    cmake_parse_arguments(instpdb "" "COMPONENT" "ARCHIVE_DEST;LIBRARY_DEST;RUNTIME_DEST" ${ARGN})

    if(NOT MSVC)
        return()
    endif()
    ...
endmacro()
```

#### 현재 코드

```cmake
macro(installPDB targetName configType)
    cmake_parse_arguments(instpdb "" "COMPONENT" "ARCHIVE_DEST;LIBRARY_DEST;RUNTIME_DEST" ${ARGN})

    if(MSVC)
        ...
    endif()
endmacro()
```

#### 바뀐 이유

- 이 파일의 `installPDB`는 `macro()`, 즉 함수가 아니라 호출 지점 스코프를 그대로 공유한다.
- Linux에서 `INSTALL_PDB` 옵션이 켜진 상태로 `ibr_install_target()`를 호출하면, `return()`이 `installPDB`만 끝내는 것이 아니라 현재 `CMakeLists.txt` 실행 전체를 끊어 버렸다.
- 그 결과 `ibr_install_target()` 뒤에 배치한 `install(DIRECTORY ... www)`와 `install(FILES ... libturbojpeg.so*)`가 생성된 `cmake_install.cmake`에 아예 들어가지 않았다.
- `return()`을 없애고 `if(MSVC)` block으로 감싸면, Windows에서는 기존 동작을 유지하면서 Linux에서도 이후 install rule이 정상 생성된다.

### 2. `install_runtime()` executable path 계산

#### 초기 코드

```cmake
string(TOUPPER ${inst_run_CONFIG_TYPE} inst_run_CONFIG_TYPE_UC)
get_target_property(postfix ${inst_run_TARGET} "${inst_run_CONFIG_TYPE_UC}_POSTFIX")
install(CODE "... app path uses ${EXEC_PATH}/${EXEC_NAME}${postfix}${CMAKE_EXECUTABLE_SUFFIX} ..."
    COMPONENT ${inst_run_COMPONENT} CONFIGURATIONS ${CONFIG_TYPE})
```

```cmake
if(_u_deps)
    message(WARNING "There were unresolved dependencies for executable ${EXEC_FILE}: ...")
endif()
```

#### 현재 코드

```cmake
string(TOUPPER ${inst_run_CONFIG_TYPE} inst_run_CONFIG_TYPE_UC)
get_target_property(postfix ${inst_run_TARGET} "${inst_run_CONFIG_TYPE_UC}_POSTFIX")
if(NOT postfix OR postfix STREQUAL "postfix-NOTFOUND")
    set(postfix "")
endif()

install(CODE "... app path uses ${EXEC_PATH}/${EXEC_NAME}${postfix}${CMAKE_EXECUTABLE_SUFFIX} ..."
    COMPONENT ${inst_run_COMPONENT} CONFIGURATIONS ${inst_run_CONFIG_TYPE})
```

```cmake
if(_u_deps)
    message(WARNING "There were unresolved dependencies for executable ${app}: ...")
endif()
```

#### 바뀐 이유

- postfix property가 정의되지 않은 target에서는 문자열이 빈 값이 아니라 `postfix-NOTFOUND`로 흘러 들어갔다.
- 그래서 generated install script가 `extended_gaussianViewer_apppostfix-NOTFOUND` 같은 잘못된 경로를 executable로 가정했고, runtime dependency copy가 비정상 동작했다.
- `CONFIGURATIONS ${CONFIG_TYPE}`도 잘못된 변수 이름이라, install code block이 기대한 config와 다르게 생성될 수 있었다.
- warning message가 `${EXEC_FILE}`를 쓰고 있었는데 이 변수는 실제로 세팅되지 않으므로, 로그도 `${app}` 기준으로 맞췄다.

## 파일: `src/projects/extended_gaussian/apps/extended_gaussianViewer/CMakeLists.txt`

### 초기 코드

```cmake
if(TARGET extended_gaussian_server)
    target_link_libraries(${PROJECT_NAME} extended_gaussian_server)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SIBR_EXTENDED_GAUSSIAN_REMOTE_STREAM_BUILD=1)
endif()
```

```cmake
ibr_install_target(${PROJECT_NAME}
    INSTALL_PDB
    RESOURCES ${RESOURCES}
    SCRIPTS ${WINDOWS_PORTABLE_SCRIPT} ${WINDOWS_PORTABLE_CHECK_SCRIPT}
    RSC_FOLDER "extended_gaussian"
    STANDALONE ${INSTALL_STANDALONE}
    COMPONENT ${PROJECT_NAME}_install
)
```

```cmake
if(TARGET extended_gaussian_server)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/projects/extended_gaussian/renderer/server/www/
        DESTINATION resources/extended_gaussian/server/www
        COMPONENT ${PROJECT_NAME}_install
    )
endif()
```

### 현재 코드

```cmake
set(LINUX_REMOTE_STREAM_RUNTIME_DIRS "")
...
if(TARGET extended_gaussian_server)
    target_link_libraries(${PROJECT_NAME} extended_gaussian_server)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SIBR_EXTENDED_GAUSSIAN_REMOTE_STREAM_BUILD=1)
    if(SIBR_TURBOJPEG_LIBRARY_DIR)
        list(APPEND LINUX_REMOTE_STREAM_RUNTIME_DIRS ${SIBR_TURBOJPEG_LIBRARY_DIR})
    endif()
endif()
```

```cmake
ibr_install_target(${PROJECT_NAME}
    INSTALL_PDB
    RESOURCES ${RESOURCES}
    SCRIPTS ${WINDOWS_PORTABLE_SCRIPT} ${WINDOWS_PORTABLE_CHECK_SCRIPT}
    RSC_FOLDER "extended_gaussian"
    DIRS ${LINUX_REMOTE_STREAM_RUNTIME_DIRS}
    STANDALONE ${INSTALL_STANDALONE}
    COMPONENT ${PROJECT_NAME}_install
)
```

```cmake
if(TARGET extended_gaussian_server)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/projects/extended_gaussian/renderer/server/www/
        DESTINATION resources/extended_gaussian/server/www
        COMPONENT ${PROJECT_NAME}_install
    )

    install(FILES
        ${SIBR_TURBOJPEG_LIBRARY_DIR}/libturbojpeg.so
        ${SIBR_TURBOJPEG_LIBRARY_DIR}/libturbojpeg.so.0
        ${SIBR_TURBOJPEG_LIBRARY_DIR}/libturbojpeg.so.0.2.0
        DESTINATION bin
        COMPONENT ${PROJECT_NAME}_install
        OPTIONAL
    )
    install(FILES
        ${SIBR_TURBOJPEG_LIBRARY_DIR}/libturbojpeg.so
        ${SIBR_TURBOJPEG_LIBRARY_DIR}/libturbojpeg.so.0
        ${SIBR_TURBOJPEG_LIBRARY_DIR}/libturbojpeg.so.0.2.0
        DESTINATION lib
        COMPONENT ${PROJECT_NAME}_install
        OPTIONAL
    )
endif()
```

### 바뀐 이유

- M5 시점에는 build-tree binary는 동작했지만, installed binary는 `libturbojpeg.so.0`를 못 찾아 실행이 깨졌다.
- app install 단계에서 vendored `libjpeg-turbo`의 library directory를 `install_runtime()` 탐색 경로에 넣어 두면, `GET_RUNTIME_DEPENDENCIES`가 해당 `.so` 체인을 찾아 `install/bin`으로 복사할 수 있다.
- 여기에 더해 `libturbojpeg.so`, `.so.0`, `.so.0.2.0`를 명시적으로도 install해서, `extended_gaussianViewer_app_install`과 root `install` 양쪽에서 runtime bundle이 비어 있지 않게 했다.
- `www/` install은 M4 때 이미 추가했지만, 앞단 macro `return()` 때문에 Linux generated install script에 빠지고 있었다. 이번에는 그 원인까지 함께 해결돼 실제 install tree에 `resources/extended_gaussian/server/www`가 생긴다.

## 요약

- 이번 수정은 “build-tree에서는 되지만 installed binary는 깨지는” 상태를 Linux install-runtime 계층에서 정리한 것이다.
- 핵심은 두 가지다.
  1. generated install script가 끝까지 생성되도록 macro control flow를 바로잡는다.
  2. installed executable이 찾을 shared library와 web assets를 install bundle 안에 실제로 넣는다.
- 그 결과 `./install/bin/extended_gaussianViewer_app --help`와 installed binary 기준 `--headless --server` smoke가 모두 다시 통과한다.
