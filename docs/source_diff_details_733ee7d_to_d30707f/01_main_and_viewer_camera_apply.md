# main / viewer camera apply 상세

이 문서는 M6에서 remote control payload 가 `main.cpp` 를 거쳐 `"Gaussian View"` 카메라에 적용되도록 연결된 변화를 정리한다.

## 디렉터리: `src/projects/extended_gaussian/apps/extended_gaussianViewer`

### 파일: `src/projects/extended_gaussian/apps/extended_gaussianViewer/main.cpp`

#### 초기 코드

```cpp
#if defined(SIBR_EXTENDED_GAUSSIAN_REMOTE_STREAM_BUILD)
#include "projects/extended_gaussian/renderer/server/RemoteStreamServer.hpp"
#include "projects/extended_gaussian/renderer/server/ServerProtocol.hpp"
#endif
```

```cpp
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
```

```cpp
while (!g_shutdown_requested && window.isOpened())
{
    Input::poll();
    if (Input::global().key().isPressed(sibr::Key::Escape)) {
        window.close();
    }

    viewer.onUpdate(Input::global());
    viewer.onRender(window);
    if (server) {
        updateServerHealth(server, viewer);
        ...
    }
    viewer.onGUI();
    viewer.onSwapBuffer(window);
}
```

#### 현재 코드

```cpp
#if defined(SIBR_EXTENDED_GAUSSIAN_REMOTE_STREAM_BUILD)
#include "projects/extended_gaussian/renderer/server/CameraPoseAdapter.hpp"
#include "projects/extended_gaussian/renderer/server/RemoteStreamServer.hpp"
#include "projects/extended_gaussian/renderer/server/ServerProtocol.hpp"
#endif
```

```cpp
RendererHealthSnapshot makeRendererHealthSnapshot(const ExtendedGaussianViewer& viewer)
{
    RendererHealthSnapshot snapshot;
    const RenderingSystem* rendering_system = viewer.getRenderingSystem();
    snapshot.initialized = rendering_system != nullptr;
    snapshot.has_manifest = rendering_system != nullptr && rendering_system->hasManifest();
    snapshot.frame_index = viewer.getFrameIndex();
    snapshot.app_time_sec = viewer.getAppTimeSeconds();

    sibr::InputCamera gaussian_view_camera;
    std::string camera_error;
    if (viewer.tryGetGaussianViewCamera(gaussian_view_camera, camera_error)) {
        snapshot.has_camera_pose = true;
        snapshot.camera_pose = ExportRemoteCameraPose(gaussian_view_camera);
    }
    return snapshot;
}
```

```cpp
void pumpRemoteControl(RemoteStreamServer* server, ExtendedGaussianViewer& viewer)
{
    if (!server) {
        return;
    }

    ControlMessage message;
    uint64_t sequence = 0;
    if (!server->consumePendingControlMessage(message, sequence)) {
        return;
    }

    bool applied = false;
    std::string apply_error;
    switch (message.type) {
    case ControlMessageType::SetCameraPose: {
        sibr::InputCamera camera;
        if (viewer.tryGetGaussianViewCamera(camera, apply_error) &&
            TryBuildInputCamera(message.camera_pose, camera, apply_error)) {
            applied = viewer.applyGaussianViewCamera(camera, apply_error);
        }
        break;
    }
    }

    if (!applied) {
        SIBR_WRG << "Failed to apply remote control message seq=" << sequence << ": " << apply_error << std::endl;
    }
    server->recordControlMessageApplied(sequence, applied);
}
```

```cpp
viewer.onUpdate(Input::global());
pumpRemoteControl(server, viewer);
viewer.onRender(window);
```

#### 바뀐 이유

- M5까지는 `main.cpp` 가 stream capture 와 `/healthz` publish 만 담당했고, remote control 입력을 viewer 에 주입하는 경로가 없었다.
- M6에서는 기존 parser/adapter (`ParseControlMessageJson(...)`, `TryBuildInputCamera(...)`) 를 재사용하되, apply 는 main thread 에서만 수행하도록 `pumpRemoteControl(...)` 을 `viewer.onUpdate(...)` 뒤에 배치했다.
- `makeRendererHealthSnapshot(...)` 가 current camera pose 를 함께 export 하도록 만든 이유는, browser/client 와 `/healthz` 가 같은 camera state 를 관찰할 수 있게 하기 위해서다.
- `recordControlMessageApplied(...)` 는 queue thread 가 아니라 apply 결과를 아는 main thread 에서 호출해야 하므로, `main.cpp` 가 최종 결과 기록 지점을 계속 소유한다.

## 디렉터리: `src/projects/extended_gaussian/renderer`

### 파일 묶음

- `src/projects/extended_gaussian/renderer/ExtendedGaussianViewer.hpp`
- `src/projects/extended_gaussian/renderer/ExtendedGaussianViewer.cpp`

#### 초기 코드

```cpp
bool loadModelDirectoryAsInstance(const std::string& modelPath);
bool captureGaussianViewSnapshot(const std::string& snapshotPath);
const RenderTargetRGB* getGaussianViewRenderTarget() const;
bool isStreamingIdle() const;
double getAppTimeSeconds() const;
uint64_t getFrameIndex() const;
const std::string& getCurrentPhase() const;
```

```cpp
const RenderTargetRGB* ExtendedGaussianViewer::getGaussianViewRenderTarget() const
{
    const auto viewIt = _ibrSubViews.find("Gaussian View");
    if (viewIt == _ibrSubViews.end()) {
        return nullptr;
    }
    return dynamic_cast<const RenderTargetRGB*>(viewIt->second.view.get());
}
```

#### 현재 코드

```cpp
bool loadModelDirectoryAsInstance(const std::string& modelPath);
bool captureGaussianViewSnapshot(const std::string& snapshotPath);
bool tryGetGaussianViewCamera(sibr::InputCamera& camera, std::string& error) const;
bool applyGaussianViewCamera(const sibr::InputCamera& camera, std::string& error);
const RenderTargetRGB* getGaussianViewRenderTarget() const;
bool isStreamingIdle() const;
double getAppTimeSeconds() const;
uint64_t getFrameIndex() const;
const std::string& getCurrentPhase() const;
```

```cpp
bool ExtendedGaussianViewer::tryGetGaussianViewCamera(sibr::InputCamera& camera, std::string& error) const
{
    const auto viewIt = _ibrSubViews.find("Gaussian View");
    if (viewIt == _ibrSubViews.end()) {
        error = "Gaussian View camera is not available.";
        return false;
    }

    if (const auto handler = std::dynamic_pointer_cast<InteractiveCameraHandler>(viewIt->second.handler)) {
        camera = handler->getCamera();
    } else {
        camera = viewIt->second.cam;
    }
    error.clear();
    return true;
}
```

```cpp
bool ExtendedGaussianViewer::applyGaussianViewCamera(const sibr::InputCamera& camera, std::string& error)
{
    auto viewIt = _ibrSubViews.find("Gaussian View");
    if (viewIt == _ibrSubViews.end()) {
        error = "Gaussian View camera is not available.";
        return false;
    }

    if (auto handler = std::dynamic_pointer_cast<InteractiveCameraHandler>(viewIt->second.handler)) {
        handler->fromCamera(camera, false, true);
        viewIt->second.cam = handler->getCamera();
    } else {
        viewIt->second.cam = camera;
    }
    error.clear();
    return true;
}
```

#### 바뀐 이유

- viewer 는 M4/M5에서 read-only health surface 와 render target getter 까지만 노출했다.
- M6에서는 `"Gaussian View"` 라는 단일 subview 가 실제 control 대상이므로, 그 camera state 를 읽고 쓰는 최소 helper 를 viewer 경계에 추가했다.
- interactive camera handler 가 존재할 때 handler 와 `viewIt->second.cam` 을 함께 갱신하는 이유는, 입력 시스템과 실제 렌더 카메라가 분리되지 않게 유지하기 위해서다.
- viewer 가 `RemoteStreamServer` 타입을 직접 알 필요는 없으므로, camera getter/apply helper 는 여전히 generic `InputCamera` 기반 API 로만 노출한다.

## 요약

- M6의 `main` / viewer 변화는 **server thread 가 직접 카메라를 건드리지 못하게 하고, main thread 만 apply 하도록 경계를 세운 것**이 핵심이다.
- 이 구조 덕분에 queue, validation, apply, health reporting 이 서로 다른 책임으로 남고, 카메라 소유권이 viewer 밖으로 새지 않는다.
