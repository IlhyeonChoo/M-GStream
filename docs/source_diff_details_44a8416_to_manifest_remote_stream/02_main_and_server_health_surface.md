# main / server health surface 상세

이 문서는 viewer phase API 를 runtime 과 server telemetry 에 연결한 변경을 정리한다.

## 파일: `src/projects/extended_gaussian/apps/extended_gaussianViewer/main.cpp`

### 초기 코드

```cpp
RendererHealthSnapshot makeRendererHealthSnapshot(const ExtendedGaussianViewer& viewer)
{
    RendererHealthSnapshot snapshot;
    const RenderingSystem* rendering_system = viewer.getRenderingSystem();
    snapshot.initialized = rendering_system != nullptr;
    snapshot.has_manifest = rendering_system != nullptr && rendering_system->hasManifest();
    snapshot.frame_index = viewer.getFrameIndex();
    snapshot.app_time_sec = viewer.getAppTimeSeconds();
    ...
    return snapshot;
}

switch (message.type) {
case ControlMessageType::SetCameraPose: {
    ...
    break;
}
}
```

초기 `main.cpp` 는 renderer health 를 camera pose 수준까지만 노출했고, remote control 적용도 `SetCameraPose` 하나만 처리했다.

### 현재 코드

```cpp
#include "projects/extended_gaussian/renderer/subsystem/rendering_system/SwapManager.hpp"
```

```cpp
snapshot.current_phase = viewer.getCurrentPhase();
snapshot.available_phases = viewer.getAvailablePhases();
snapshot.total_asset_count = viewer.getManifestAssetCount();

if (rendering_system != nullptr) {
    if (const SwapManager::Stats* swap = rendering_system->getSwapStats()) {
        snapshot.required_gpu_count = swap->required_gpu_count;
        snapshot.warm_cpu_count = swap->warm_cpu_count;
        snapshot.pending_disk_loads = swap->pending_disk_loads;
        snapshot.pending_gpu_uploads = swap->pending_gpu_uploads;
        snapshot.pending_gpu_evictions = swap->pending_gpu_evictions;
        snapshot.cpu_resident_bytes = swap->cpu_resident_bytes;
        snapshot.gpu_resident_bytes = swap->gpu_resident_bytes;
        snapshot.skipped_instances_last_frame = swap->skipped_instances_last_frame;
        snapshot.swap_hits = swap->swap_hits;
        snapshot.swap_misses = swap->swap_misses;
    }
}
```

```cpp
case ControlMessageType::SetPhase: {
    viewer.setCurrentPhase(message.phase);
    applied = true;
    break;
}
```

## 파일: `src/projects/extended_gaussian/renderer/server/RemoteStreamServer.{hpp,cpp}`

### 초기 코드

```cpp
struct RendererHealthSnapshot {
    bool initialized = false;
    bool has_manifest = false;
    uint64_t frame_index = 0;
    double app_time_sec = 0.0;
    bool has_camera_pose = false;
    RemoteCameraPose camera_pose;
};
```

```cpp
"renderer": {
  "initialized": ...,
  "has_manifest": ...,
  "frame_index": ...,
  "app_time_sec": ...,
  "camera_pose_available": ...
}
```

```cpp
"type":"ready", ... ,"frame_index":...,"app_time_sec":...
```

```cpp
"type":"ack","request_type":"set_camera_pose",...
```

### 현재 코드

```cpp
struct RendererHealthSnapshot {
    ...
    std::string current_phase;
    std::vector<std::string> available_phases;
    size_t total_asset_count = 0;
    size_t required_gpu_count = 0;
    size_t warm_cpu_count = 0;
    size_t pending_disk_loads = 0;
    size_t pending_gpu_uploads = 0;
    size_t pending_gpu_evictions = 0;
    size_t cpu_resident_bytes = 0;
    size_t gpu_resident_bytes = 0;
    size_t skipped_instances_last_frame = 0;
    size_t swap_hits = 0;
    size_t swap_misses = 0;
};
```

```cpp
// Append manifest summary to the renderer object in /healthz.
// Added fields:
// - current_phase
// - available_phases[]
// - total_assets
// - streaming.{required_gpu,warm_cpu,pending_disk_loads,pending_gpu_uploads,
//   pending_gpu_evictions,cpu_resident_bytes,gpu_resident_bytes,
//   skipped_instances,swap_hits,swap_misses}
```

```cpp
"type":"ready",...,"has_manifest":true,"current_phase":"alpha","available_phases":["alpha","beta"],"total_assets":1
```

```cpp
std::string controlAckJson(uint64_t sequence, bool superseded_previous, const char* request_type)
```

```cpp
const char* request_type =
    parsed.message.type == ControlMessageType::SetPhase ? "set_phase" : "set_camera_pose";
response_payload = controlAckJson(sequence, superseded_previous, request_type);
```

## 파일: `src/projects/extended_gaussian/renderer/server/examples/control_messages/valid_set_phase_default.json`

```json
{"type":"set_phase","phase":"default_phase"}
```

이 예제는 `ws_control_smoke.mjs` 로 `set_phase` 회귀를 바로 찍기 위한 최소 payload 다.

## 바뀐 이유

- browser client 가 manifest 상태를 관찰하려면 `/healthz` 와 `ready` 가 현재 phase / phase 목록 / asset 수 / streaming stats 를 함께 제공해야 했다.
- `request_type` 을 ack 에 넣은 이유는 `set_camera_pose` 30 Hz 스트림과 `set_phase` 단발 요청을 browser 에서 구분하기 위해서다.
- `main.cpp` 에서는 viewer 를 단일 source of truth 로 두고, `SetPhase` 는 viewer phase 문자열만 바꾸도록 제한했다. rule evaluation 과 streaming 결과는 기존 render/update loop 가 그대로 처리한다.
