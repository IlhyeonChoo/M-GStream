# server protocol / HTTP runtime 상세

이 문서는 M4에서 `renderer/server` 가 실제 HTTP skeleton runtime 을 갖게 된 변화와 option canonicalization 을 정리한다.

## 디렉터리: `src/projects/extended_gaussian/renderer/server`

### 파일: `src/projects/extended_gaussian/renderer/server/ServerProtocol.hpp`

#### 초기 코드

```cpp
struct ServerOptions {
    bool enabled = false;
    std::string listen_host = "127.0.0.1";
    int listen_port = 8080;
    int stream_width = 1280;
    int stream_height = 720;
    int stream_fps = 15;
};
```

#### 현재 코드

```cpp
struct ServerOptions {
    bool enabled = false;
    std::string listen_host = "127.0.0.1";
    int listen_port = 8080;
    int stream_width = 1280;
    int stream_height = 720;
    int stream_fps = 15;
    std::string www_root;
};
```

#### 바뀐 이유

- M4부터는 정적 자산 루트를 source tree, install tree, 사용자 override 중 어디로 잡을지 runtime 에서 결정해야 했다.
- 이를 위해 `ServerOptions` 자체에 `www_root` 를 포함시켜 app shell 이 override 를 전달할 수 있게 했다.

### 파일: `src/projects/extended_gaussian/renderer/server/ServerProtocol.cpp`

#### 초기 코드

```cpp
ServerOptions options;
options.enabled = args.get<bool>("server", false);

const std::string listenHost = args.get<std::string>("listen-host", options.listen_host);
if (!listenHost.empty()) {
    options.listen_host = listenHost;
}

const int listenPort = args.get<int>("listen-port", options.listen_port);
if (listenPort > 0 && listenPort <= 65535) {
    options.listen_port = listenPort;
}
```

#### 현재 코드

```cpp
ServerOptions options;
options.enabled = args.get<bool>("server", false);

const std::string bindHost = args.get<std::string>("bind", "");
if (!bindHost.empty()) {
    options.listen_host = bindHost;
}

const std::string listenHost = args.get<std::string>("listen-host", "");
if (!listenHost.empty()) {
    options.listen_host = listenHost;
}

const int bindPort = args.get<int>("port", 0);
if (bindPort != 0) {
    options.listen_port = bindPort;
}

const int listenPort = args.get<int>("listen-port", 0);
if (listenPort != 0) {
    options.listen_port = listenPort;
}

const std::string wwwRoot = args.get<std::string>("www-root", "");
if (!wwwRoot.empty()) {
    options.www_root = wwwRoot;
}
```

#### 바뀐 이유

- app help 에는 `--listen-host`, `--listen-port` 를 canonical 이름으로 노출하되, 기존 관성상 `--bind`, `--port` 같은 짧은 alias 도 허용할 필요가 있었다.
- 최종 우선순위는 canonical 옵션이 alias 보다 강하게 먹도록 구현했다.
- `--www-root` 는 smoke test 와 설치 전 개발 환경에서 정적 자산 루트를 강제로 바꿔야 할 때 필요했다.

### 파일: `src/projects/extended_gaussian/renderer/server/CMakeLists.txt`

#### 초기 코드

```cmake
set(SERVER_SOURCES
    CameraPoseAdapter.cpp
    CameraPoseAdapter.hpp
    Config.hpp
    ServerProtocol.cpp
    ServerProtocol.hpp
)
...
target_compile_definitions(${PROJECT_NAME} PUBLIC
    SIBR_EXTENDED_GAUSSIAN_SERVER_STATIC_DEFINE
)
```

#### 현재 코드

```cmake
set(SERVER_SOURCES
    CameraPoseAdapter.cpp
    CameraPoseAdapter.hpp
    Config.hpp
    RemoteStreamServer.cpp
    RemoteStreamServer.hpp
    ServerProtocol.cpp
    ServerProtocol.hpp
)
...
target_compile_definitions(${PROJECT_NAME} PUBLIC
    SIBR_EXTENDED_GAUSSIAN_SERVER_STATIC_DEFINE
)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    SIBR_EXTENDED_GAUSSIAN_REMOTE_WWW_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/www"
)
```

#### 바뀐 이유

- M3의 `extended_gaussian_server` 는 parser / camera adapter 만 가진 상태였다.
- M4에서 실제 HTTP runtime 을 추가하면서 `RemoteStreamServer` 소스를 target 에 편입했다.
- compile-time source `www/` 경로를 매크로로 넣어 둔 이유는 install tree 가 아직 없거나 `--www-root` 가 비어 있을 때도 최소한의 개발용 fallback 을 보장하기 위해서다.

### 파일 묶음

- `src/projects/extended_gaussian/renderer/server/RemoteStreamServer.hpp`
- `src/projects/extended_gaussian/renderer/server/RemoteStreamServer.cpp`

#### 초기 코드

```text
기준 커밋 `c6b82fd` 에는 파일이 없었다.
```

#### 현재 코드

```cpp
struct RendererHealthSnapshot {
    bool initialized = false;
    bool has_manifest = false;
    uint64_t frame_index = 0;
    double app_time_sec = 0.0;
};

struct ServerStats {
    bool running = false;
    std::string listen_host;
    int listen_port = 0;
    std::string www_root;
    uint64_t total_http_requests = 0;
};
```

```cpp
bool RemoteStreamServer::start(std::string& error)
{
    www_root_ = resolveWwwRoot(error);
    ...
    acceptor->non_blocking(true, ec);
    ...
    server_thread_ = std::thread(&RemoteStreamServer::serverThreadMain, this);
}
```

```cpp
std::string RemoteStreamServer::resolveWwwRoot(std::string& error) const
{
    if (!options_.www_root.empty()) {
        ...
    }
    if (const char* env_root = std::getenv("EXTENDED_GAUSSIAN_WWW_ROOT")) {
        ...
    }
    const fs::path install_directory = fs::path(getInstallDirectory());
    candidates.emplace_back(install_directory / "resources" / "extended_gaussian" / "server" / "www");
    candidates.emplace_back(install_directory / "share" / "extended_gaussian" / "www");
#ifdef SIBR_EXTENDED_GAUSSIAN_REMOTE_WWW_SOURCE_DIR
    candidates.emplace_back(fs::path(SIBR_EXTENDED_GAUSSIAN_REMOTE_WWW_SOURCE_DIR));
#endif
}
```

```cpp
if (target == "/healthz") {
    write_string_response(http::status::ok, "application/json; charset=utf-8", healthJson(...));
} else if (target == "/stream.mjpg") {
    write_string_response(http::status::not_implemented, ...);
} else if (target == "/control") {
    ... response.set(http::field::upgrade, "websocket");
} else {
    if (target == "/" || target == "/index.html") {
        relative_path = "index.html";
    } else if (target == "/app.js") {
        relative_path = "app.js";
    } else if (target == "/styles.css") {
        relative_path = "styles.css";
    } else if (target.rfind("/static/", 0) == 0) {
        relative_path = target.substr(std::string("/static/").size());
    }
}
```

#### 바뀐 이유

- M4의 실제 목표는 MJPEG 나 WebSocket session 을 끝까지 구현하는 것이 아니라, viewer process 위에 올릴 최소 HTTP skeleton 을 닫는 것이었다.
- 그래서 `RemoteStreamServer` 는 다음 경계에 집중한다.
  - viewer thread 와 분리된 dedicated server thread
  - 단일 connection / 단일 response 기반의 단순한 HTTP 처리
  - `healthz` 와 reference web asset serving
  - 이후 M5/M6에서 확장할 placeholder route (`/stream.mjpg`, `/control`)
- `resolveWwwRoot()` 는 개발 환경과 설치 환경이 모두 동작하도록 우선순위를 분명히 했다.
  1. `--www-root`
  2. `EXTENDED_GAUSSIAN_WWW_ROOT`
  3. install tree `resources/extended_gaussian/server/www`
  4. legacy `share/extended_gaussian/www`
  5. compile-time source `www/`
- path decode 와 traversal 방어를 M4에서 먼저 넣어 둔 이유는, 이후 브라우저 자산이 늘어나더라도 정적 자산 서빙이 가장 기초적인 안전성을 갖게 하기 위해서다.
- `/stream.mjpg` 를 501, `/control` 을 426 으로 두고 명시적인 JSON 을 반환한 이유는, M4 단계에서 “아직 안 된 기능”과 “라우트 자체는 존재함”을 분리해서 드러내기 위해서다.

## 요약

- M4에서 `renderer/server` 는 더 이상 parser-only 준비 코드가 아니라, 실제 HTTP skeleton runtime 을 가진 모듈이 되었다.
- 다만 이 시점의 server 는 health / static assets / placeholder routes 까지만 책임진다.
- MJPEG multipart streaming, JPEG encoding, WebSocket session, 인증/TLS 는 의도적으로 다음 단계로 미뤘다.
