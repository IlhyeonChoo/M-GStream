# project / app wiring 상세

이 문서는 M4에서 `extended_gaussian_server` 가 viewer app 런타임까지 실제로 연결되도록 바뀐 top-level CMake 와 app install wiring 을 정리한다.

## 디렉터리: `src/projects/extended_gaussian`

### 파일: `src/projects/extended_gaussian/CMakeLists.txt`

#### 초기 코드

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

#### 현재 코드

```cmake
project(extended_gaussian_all)

if(WIN32)
    option(SIBR_BUILD_REMOTE_STREAM "Build remote stream support targets for extended_gaussian" OFF)
else()
    option(SIBR_BUILD_REMOTE_STREAM "Build remote stream support targets for extended_gaussian" ON)
endif()
message(STATUS "extended_gaussian remote stream modules: ${SIBR_BUILD_REMOTE_STREAM}")

add_subdirectory(renderer)
add_subdirectory(apps)
```

#### 바뀐 이유

- M3까지는 `extended_gaussian_server` target 이 생겨도 app 쪽에서는 아직 그 target 을 보지 않았다.
- M4에서는 `extended_gaussianViewer_app` 가 `extended_gaussian_server` 를 link 하고 compile definition 도 받아야 했기 때문에, `apps` 보다 먼저 `renderer` 가 configure 되어야 했다.
- 이 순서를 바꾸지 않으면 app CMake 내부의 `if(TARGET extended_gaussian_server)` 블록이 false 로 평가돼, `--server` 지원이 빌드에 실제로 들어가지 않는다.

## 디렉터리: `src/projects/extended_gaussian/apps/extended_gaussianViewer`

### 파일: `src/projects/extended_gaussian/apps/extended_gaussianViewer/CMakeLists.txt`

#### 초기 코드

```cmake
add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME}
    ...
    extended_gaussian
    sibr_view
    sibr_assets
    sibr_renderer
)
...
ibr_install_target(${PROJECT_NAME}
    INSTALL_PDB
    RESOURCES ${RESOURCES}
    SCRIPTS ${WINDOWS_PORTABLE_SCRIPT} ${WINDOWS_PORTABLE_CHECK_SCRIPT}
    RSC_FOLDER "extended_gaussian"
    STANDALONE ${INSTALL_STANDALONE}
    COMPONENT ${PROJECT_NAME}_install
)
```

#### 현재 코드

```cmake
add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME}
    ...
    extended_gaussian
    sibr_view
    sibr_assets
    sibr_renderer
)
if(TARGET extended_gaussian_server)
    target_link_libraries(${PROJECT_NAME} extended_gaussian_server)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SIBR_EXTENDED_GAUSSIAN_REMOTE_STREAM_BUILD=1)
endif()
...
ibr_install_target(${PROJECT_NAME}
    INSTALL_PDB
    RESOURCES ${RESOURCES}
    SCRIPTS ${WINDOWS_PORTABLE_SCRIPT} ${WINDOWS_PORTABLE_CHECK_SCRIPT}
    RSC_FOLDER "extended_gaussian"
    STANDALONE ${INSTALL_STANDALONE}
    COMPONENT ${PROJECT_NAME}_install
)

if(TARGET extended_gaussian_server)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/projects/extended_gaussian/renderer/server/www/
        DESTINATION resources/extended_gaussian/server/www
        COMPONENT ${PROJECT_NAME}_install
    )
endif()
```

#### 바뀐 이유

- M4부터는 viewer app 이 실제로 `RemoteStreamServer` 코드를 호출하므로, link 단계에서 `extended_gaussian_server` 정적 라이브러리가 필요해졌다.
- `SIBR_EXTENDED_GAUSSIAN_REMOTE_STREAM_BUILD=1` compile definition 을 app 에만 주어, `main.cpp` 가 server 코드 경로를 조건부로 활성화하게 했다.
- 정적 자산은 처음에는 `share/extended_gaussian/www` 후보를 상정했지만, 현재 install tree 구조와 맞지 않았다.
- 최종적으로 `resources/extended_gaussian/server/www` 를 install destination 으로 맞춰, app binary 가 설치 후에도 reference client 자산을 직접 찾을 수 있게 했다.

## 요약

- M4의 첫 번째 변화는 새 server 코드를 단순히 빌드하는 수준을 넘어서, viewer app 런타임과 install tree 에 연결한 것이다.
- top-level CMake 순서와 app install 경로가 맞아야 `--server` 가 실제 실행 기능이 된다.
