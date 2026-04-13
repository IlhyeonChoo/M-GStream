# viewer 내부 headless runtime 상세

이 문서는 M2에서 `ExtendedGaussianViewer` 내부에 추가된 headless helper와 snapshot/runtime 보조 로직을 정리한다.

## 디렉터리: `src/projects/extended_gaussian/renderer`

### 파일 묶음

- `src/projects/extended_gaussian/renderer/ExtendedGaussianViewer.hpp`
- `src/projects/extended_gaussian/renderer/ExtendedGaussianViewer.cpp`

#### 초기 코드

```cpp
const GaussianScene* getScene() const;
GaussianScene* getScene();
ResourceManager* getResourceManager();
const ResourceManager* getResourceManager() const;
RenderingSystem* getRenderingSystem();
const RenderingSystem* getRenderingSystem() const;

private:
    bool loadManifestFile(const std::string& path);
    size_t createManifestInstances(bool onlyMissing = true);
    void focusCameraOnManifest();
```

```cpp
void ExtendedGaussianViewer::onRender(Window& win)
{
    ...
    onGui(win);
    ...
}
```

#### 현재 코드

```cpp
bool loadModelDirectoryAsInstance(const std::string& modelPath);
bool captureGaussianViewSnapshot(const std::string& snapshotPath);
bool isStreamingIdle() const;
...
void focusCameraOnBounds(const Vector3f& minBounds, const Vector3f& maxBounds);
```

```cpp
void ExtendedGaussianViewer::onRender(Window& win)
{
    ...
    if (_enableGUI) {
        onGui(win);
    }
    ...
}

void ExtendedGaussianViewer::onGui(Window& win)
{
    if (!_enableGUI) {
        return;
    }
    MultiViewBase::onGui(win);
    ...
}
```

```cpp
bool ExtendedGaussianViewer::loadModelDirectoryAsInstance(const std::string& modelPath)
{
    auto field = GaussianLoader::load(modelPath);
    ...
    GaussianInstance* instance = _scene->createInstance(instanceName, assetId);
    ...
    focusCameraOnBounds(minBounds, maxBounds);
    return true;
}
```

```cpp
bool ExtendedGaussianViewer::captureGaussianViewSnapshot(const std::string& snapshotPath)
{
    const auto viewIt = _ibrSubViews.find("Gaussian View");
    ...
    captureView("Gaussian View", directory, fileName);
    return boost::filesystem::exists(boost::filesystem::path(directory) / fileName);
}
```

```cpp
bool ExtendedGaussianViewer::isStreamingIdle() const
{
    const SwapManager::Stats* stats = renderingSystem->getSwapStats();
    return stats->pending_disk_loads == 0
        && stats->pending_gpu_uploads == 0
        && stats->pending_gpu_evictions == 0
        && stats->skipped_instances_last_frame == 0;
}
```

#### 바뀐 이유

- headless app entry만 추가하면 부족하다. viewer 내부에도 GUI 비활성 상태에서 안전하게 돌고, raw model directory 또는 manifest를 받아 스스로 장면을 만들고, 마지막에 캡처까지 끝낼 수 있는 helper가 필요했다.
- `loadModelDirectoryAsInstance`는 manifest가 아닌 단일 model directory로도 M2 smoke를 할 수 있게 만든 경로다.
- `captureGaussianViewSnapshot`는 M5 이후 MJPEG 캡처 경계와 같은 `"Gaussian View"` subview를 단발 PNG 저장에 먼저 재사용한 것이다.
- `isStreamingIdle`과 `focusCameraOnBounds`는 manifest 기반 headless 캡처가 안정적인 시점에 종료되도록 만들기 위한 보조 API다.

### 파일: `src/projects/extended_gaussian/renderer/subsystem/rendering_system/GaussianView.cpp`

#### 초기 코드

```cpp
if (!useInterop)
{
    fallback_bytes.resize(render_w * render_h * 3 * sizeof(float));
    cudaMalloc(&fallbackBufferCuda, fallback_bytes.size());
    _interop_failed = true;
}
```

#### 현재 코드

```cpp
if (!useInterop)
{
    fallback_bytes.resize(render_w * render_h * 3 * sizeof(float));
    cudaMalloc(&fallbackBufferCuda, fallback_bytes.size());
    _interop_failed = true;
    SIBR_WRG << "GaussianView CUDA/GL interop disabled, using CUDA->CPU->GL fallback copy." << std::endl;
}
else
{
    SIBR_LOG << "GaussianView CUDA/GL interop enabled." << std::endl;
}
```

#### 바뀐 이유

- M2 bring-up에서는 interop 성공/실패 여부를 로그에서 즉시 확인할 수 있어야 했다.
- 실제 rendering path를 바꾸려는 수정이 아니라, headless 환경에서 어떤 copy 경로가 선택됐는지 진단하기 쉽게 만든 보조 수정이다.

### 파일: `src/projects/extended_gaussian/renderer/subsystem/Subsystem.hpp`

#### 초기 코드

```text
기준 커밋 `0dd4e74` 시점과 비교해 실질 의미 변화가 없었다.
```

#### 현재 코드

```text
현재 diff 는 개행/포맷 수준이며 enum 값이나 callback 계약 변화는 없다.
```

#### 바뀐 이유

- 이 파일은 M2 기능의 실질 변경 대상이 아니었다.
- 비교 구간에 포함되긴 했지만, subsystem 계약 자체를 바꾼 것은 아니라는 점을 명시해 두는 편이 맞다.

## 요약

- M2 viewer 수정의 핵심은 headless 실행을 `main.cpp`에만 억지로 넣지 않고, viewer API로 끌어올려 재사용 가능하게 만든 것이다.
- `GaussianView` 로그 보강은 headless acceptance를 닫기 위한 관측성 강화에 해당한다.
