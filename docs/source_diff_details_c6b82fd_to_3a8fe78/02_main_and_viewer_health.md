# app entry / viewer health 상세

이 문서는 M4에서 `main.cpp` 가 remote stream server lifecycle 을 직접 관리하게 된 변화와, viewer 쪽에 추가된 read-only health surface 를 정리한다.

## 디렉터리: `src/projects/extended_gaussian/apps/extended_gaussianViewer`

### 파일: `src/projects/extended_gaussian/apps/extended_gaussianViewer/main.cpp`

#### 초기 코드

```cpp
struct ExtendedGaussianViewerAppArgs : virtual BasicIBRAppArgs {
    Arg<std::string> manifest = { "manifest", "", "path to a manifest json file" };
    Arg<bool> headless = { "headless", "run a finite offscreen render loop and exit" };
    Arg<int> render_width = { "render-width", 1280, ... };
    Arg<int> render_height = { "render-height", 720, ... };
    Arg<std::string> snapshot = { "snapshot", "", ... };
    Arg<bool> wait_for_streaming_idle = { "wait-for-streaming-idle", ... };
    Arg<int> max_headless_frames = { "max-headless-frames", 600, ... };
};

int runInteractive(ExtendedGaussianViewer& viewer, Window& window)
{
    while (window.isOpened())
    {
        ...
        viewer.onUpdate(Input::global());
        viewer.onRender(window);
        viewer.onSwapBuffer(window);
    }
}
```

```cpp
int main(int ac, char** av)
{
    CommandLineArgs::parseMainArgs(ac, av);
    ExtendedGaussianViewerAppArgs myArgs;
    ...
    ExtendedGaussianViewer viewer(*window, false);

    if (myArgs.headless.get()) {
        return runHeadless(viewer, *window, myArgs);
    }

    return runInteractive(viewer, *window);
}
```

#### 현재 코드

```cpp
#if defined(SIBR_EXTENDED_GAUSSIAN_REMOTE_STREAM_BUILD)
#include "projects/extended_gaussian/renderer/server/RemoteStreamServer.hpp"
#include "projects/extended_gaussian/renderer/server/ServerProtocol.hpp"
#endif

volatile std::sig_atomic_t g_shutdown_requested = 0;

void handleProcessSignal(int /*signal_number*/)
{
    g_shutdown_requested = 1;
}
```

```cpp
struct ExtendedGaussianViewerAppArgs : virtual BasicIBRAppArgs {
    ...
#if defined(SIBR_EXTENDED_GAUSSIAN_REMOTE_STREAM_BUILD)
    Arg<bool> server = { "server", "start the remote browser stream HTTP server" };
    Arg<std::string> listen_host = { "listen-host", "127.0.0.1", ... };
    Arg<int> listen_port = { "listen-port", 8080, ... };
    Arg<std::string> bind = { "bind", "", "alias for --listen-host" };
    Arg<int> port = { "port", 0, "alias for --listen-port" };
    Arg<int> stream_width = { "stream-width", 1280, ... };
    Arg<int> stream_height = { "stream-height", 720, ... };
    Arg<int> stream_fps = { "stream-fps", 15, ... };
    Arg<std::string> www_root = { "www-root", "", ... };
#endif
};
```

```cpp
#if defined(SIBR_EXTENDED_GAUSSIAN_REMOTE_STREAM_BUILD)
RendererHealthSnapshot makeRendererHealthSnapshot(const ExtendedGaussianViewer& viewer)
{
    RendererHealthSnapshot snapshot;
    const RenderingSystem* rendering_system = viewer.getRenderingSystem();
    snapshot.initialized = rendering_system != nullptr;
    snapshot.has_manifest = rendering_system != nullptr && rendering_system->hasManifest();
    snapshot.frame_index = viewer.getFrameIndex();
    snapshot.app_time_sec = viewer.getAppTimeSeconds();
    return snapshot;
}
#endif
```

```cpp
const ServerOptions server_options = ParseServerOptions(getCommandLineArgs());
if (server_options.enabled && myArgs.headless.get()) {
    SIBR_WRG << "--server is not compatible with finite --headless mode. Use --offscreen --nogui --server for a long-running no-display server." << std::endl;
    return EXIT_FAILURE;
}
...
std::unique_ptr<RemoteStreamServer> server;
if (server_options.enabled) {
    server = std::make_unique<RemoteStreamServer>(server_options);
    updateServerHealth(server.get(), viewer);
    if (!server->start(server_error)) {
        ...
    }
}
const int result = runInteractive(viewer, *window, server.get());
if (server) {
    server->stop();
}
```

#### 바뀐 이유

- 초기 상태의 app entry 는 M2 headless snapshot 과 interactive viewer loop 만 갖고 있었고, 장기 실행 server lifecycle 이 없었다.
- M4에서는 `--server` 관련 CLI 를 app shell 에 노출하되, 실제 옵션 정규화는 `ParseServerOptions(...)` 에 맡겨 app-level UX 와 server contract parsing 을 분리했다.
- `main.cpp` 가 `RemoteStreamServer` 를 직접 소유하게 한 이유는 server 를 viewer 내부 객체로 만들지 않기 위해서다. viewer 는 render/UI state 소유자이고, server 는 process-level peripheral 이다.
- `--server --headless` 를 막은 이유는 M2의 finite one-shot path 와 M4의 long-running HTTP server 를 같은 실행 모드로 혼합하지 않기 위해서다.
- signal handling 을 `main` 에 둔 이유도 같다. 종료 정책은 프로세스 최상위 계층이 책임지는 편이 맞다.

## 디렉터리: `src/projects/extended_gaussian/renderer`

### 파일 묶음

- `src/projects/extended_gaussian/renderer/ExtendedGaussianViewer.hpp`
- `src/projects/extended_gaussian/renderer/ExtendedGaussianViewer.cpp`

#### 초기 코드

```cpp
ResourceManager* getResourceManager();
const ResourceManager* getResourceManager() const;
RenderingSystem* getRenderingSystem();
const RenderingSystem* getRenderingSystem() const;
bool loadModelDirectoryAsInstance(const std::string& modelPath);
bool captureGaussianViewSnapshot(const std::string& snapshotPath);
bool isStreamingIdle() const;
```

#### 현재 코드

```cpp
ResourceManager* getResourceManager();
const ResourceManager* getResourceManager() const;
RenderingSystem* getRenderingSystem();
const RenderingSystem* getRenderingSystem() const;
bool loadModelDirectoryAsInstance(const std::string& modelPath);
bool captureGaussianViewSnapshot(const std::string& snapshotPath);
bool isStreamingIdle() const;
double getAppTimeSeconds() const;
uint64_t getFrameIndex() const;
const std::string& getCurrentPhase() const;
```

```cpp
double ExtendedGaussianViewer::getAppTimeSeconds() const
{
    return _appTimeSec;
}

uint64_t ExtendedGaussianViewer::getFrameIndex() const
{
    return _frameIndex;
}

const std::string& ExtendedGaussianViewer::getCurrentPhase() const
{
    return _currentPhase;
}
```

#### 바뀐 이유

- M4의 `/healthz` 는 viewer 내부 상태를 읽어야 하지만, viewer 쪽이 server header 를 include 하거나 server 타입을 알게 만들 필요는 없었다.
- 그래서 viewer 에는 health snapshot 에 필요한 최소한의 read-only getter 만 추가했다.
- `main.cpp` 가 이 getter 들을 조합해 `RendererHealthSnapshot` 을 구성하면, viewer 와 server 가 직접 얽히지 않는다.
- `getCurrentPhase()` 는 M4 시점 health JSON 에 아직 직렬화하지 않았더라도, 이후 M5/M6에서 phase 를 그대로 노출할 수 있게 미리 surface 를 맞춰 둔 의미가 있다.

## 요약

- M4의 app entry 변화는 server 를 “viewer 안에 숨기는 것”이 아니라, `main` 이 lifecycle 과 signal 을 관리하고 viewer 는 read-only 상태 공급자만 되도록 경계를 유지한 것이다.
- 이 구조 덕분에 이후 M5/M6에서 스트림/컨트롤이 추가돼도 viewer 내부 책임이 급격히 커지지 않는다.
