# protocol / viewer phase control 상세

이 문서는 manifest remote stream 단계에서 `set_phase` control message 와 viewer phase API 가 어떻게 추가됐는지 정리한다.

## 파일: `src/projects/extended_gaussian/renderer/server/ServerProtocol.hpp`

### 초기 코드

```cpp
enum class ControlMessageType {
    SetCameraPose
};

struct ControlMessage {
    ControlMessageType type = ControlMessageType::SetCameraPose;
    RemoteCameraPose camera_pose;
};
```

초기 protocol 은 WebSocket control payload 를 `set_camera_pose` 하나만 받는 구조였다.

### 현재 코드

```cpp
enum class ControlMessageType {
    SetCameraPose,
    SetPhase
};

struct ControlMessage {
    ControlMessageType type = ControlMessageType::SetCameraPose;
    RemoteCameraPose camera_pose;
    std::string phase;
};
```

## 파일: `src/projects/extended_gaussian/renderer/server/ServerProtocol.cpp`

### 초기 코드

```cpp
if (type != "set_camera_pose") {
    result.error = "Unsupported control message type '" + type + "'.";
    return result;
}

ControlMessage message;
message.type = ControlMessageType::SetCameraPose;
...
if (!ValidateRemoteCameraPose(message.camera_pose, result.error)) {
    return result;
}
```

### 현재 코드

```cpp
ControlMessage message;
if (type == "set_camera_pose") {
    message.type = ControlMessageType::SetCameraPose;
    ...
    if (!ValidateRemoteCameraPose(message.camera_pose, result.error)) {
        return result;
    }
} else if (type == "set_phase") {
    message.type = ControlMessageType::SetPhase;
    if (!parseStringField(object, "phase", message.phase, result.error)) {
        return result;
    }
} else {
    result.error = "Unsupported control message type '" + type + "'.";
    return result;
}
```

핵심은 `set_phase` 가 camera pose validation 경로를 타지 않게 분기를 완전히 분리한 점이다.

## 파일: `src/projects/extended_gaussian/renderer/ExtendedGaussianViewer.{hpp,cpp}`

### 초기 코드

```cpp
const std::string& getCurrentPhase() const;
```

viewer 내부에는 `_currentPhase` 와 `_manifestStore` 가 있었지만, 외부에서 phase 목록과 asset 수를 읽거나 phase 를 바꾸는 public API 는 없었다.

### 현재 코드

```cpp
const std::string& getCurrentPhase() const;
void setCurrentPhase(const std::string& phase);
std::vector<std::string> getAvailablePhases() const;
size_t getManifestAssetCount() const;
```

```cpp
void ExtendedGaussianViewer::setCurrentPhase(const std::string& phase)
{
    _currentPhase = phase;
}

std::vector<std::string> ExtendedGaussianViewer::getAvailablePhases() const
{
    return _manifestStore.phases();
}

size_t ExtendedGaussianViewer::getManifestAssetCount() const
{
    return _manifestStore.assets().size();
}
```

## 바뀐 이유

- server mode 에서는 ImGui phase panel 이 없으므로, browser control path 에 phase 변경 엔트리포인트가 필요했다.
- validation 은 일부러 넣지 않았다. desktop ImGui 와 동일하게 arbitrary string 을 허용하고, 매칭되는 rule 이 없으면 no-op 이 되게 유지했다.
- `getAvailablePhases()` / `getManifestAssetCount()` 는 browser status panel 과 `/healthz` 의 기반 데이터를 주기 위한 최소 공개 API 다.
