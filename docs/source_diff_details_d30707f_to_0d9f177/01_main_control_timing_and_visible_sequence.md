# main control timing / visible sequence 상세

이 문서는 M7에서 `main.cpp` 가 M6의 camera apply 경로 위에 **timing 측정과 visible control sequence 연결**을 추가한 변화를 정리한다.

## 디렉터리: `src/projects/extended_gaussian/apps/extended_gaussianViewer`

### 파일: `src/projects/extended_gaussian/apps/extended_gaussianViewer/main.cpp`

#### 초기 코드

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
    ...
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
if (server) {
    if (const RenderTargetRGB* stream_target = viewer.getGaussianViewRenderTarget()) {
        server->submitRenderedFrame(*stream_target, viewer.getFrameIndex());
    }
    updateServerHealth(server, viewer);
}
```

#### 현재 코드

```cpp
void pumpRemoteControl(RemoteStreamServer* server, ExtendedGaussianViewer& viewer, uint64_t* visible_control_sequence)
{
    if (!server) {
        return;
    }

    ControlMessage message;
    uint64_t sequence = 0;
    std::chrono::steady_clock::time_point received_at = std::chrono::steady_clock::time_point::min();
    if (!server->consumePendingControlMessage(message, sequence, received_at)) {
        return;
    }
    ...
    const auto apply_end = std::chrono::steady_clock::now();
    double receive_to_apply_ms = 0.0;
    if (received_at != std::chrono::steady_clock::time_point::min()) {
        receive_to_apply_ms = std::chrono::duration<double, std::milli>(apply_end - received_at).count();
    }

    if (!applied) {
        SIBR_WRG << "Failed to apply remote control message seq=" << sequence << ": " << apply_error << std::endl;
    } else if (visible_control_sequence != nullptr) {
        *visible_control_sequence = sequence;
    }
    server->recordControlMessageApplied(sequence, applied, receive_to_apply_ms);
}
```

```cpp
uint64_t visible_control_sequence = 0;
...
viewer.onUpdate(Input::global());
pumpRemoteControl(server, viewer, &visible_control_sequence);
viewer.onRender(window);
if (server) {
    if (const RenderTargetRGB* stream_target = viewer.getGaussianViewRenderTarget()) {
        server->submitRenderedFrame(*stream_target, viewer.getFrameIndex(), visible_control_sequence);
    }
    updateServerHealth(server, viewer);
}
```

#### 바뀐 이유

- M6까지는 `ack` 와 `last_applied_sequence` 까지는 기록됐지만, **그 control 이 어떤 MJPEG frame 에 반영되었는지**는 서버 쪽에서 바로 추적할 수 없었다.
- M7에서는 `consumePendingControlMessage(...)` 가 `received_at` 을 함께 넘기도록 바뀌었고, `main.cpp` 가 apply 완료 시점에서 `receive_to_apply_ms` 를 계산해 `recordControlMessageApplied(...)` 로 밀어 넣는다.
- 성공적으로 apply 된 sequence 는 `visible_control_sequence` 로 유지한 뒤 `submitRenderedFrame(...)` 에 함께 넘긴다. 이 값이 이후 MJPEG part header 의 `X-Control-Sequence` 로 연결된다.
- 구조상 timing 계산과 visible sequence 확정은 **camera apply 결과를 아는 main thread** 에서만 할 수 있으므로, 이 책임은 계속 `main.cpp` 가 가진다.

## 요약

- M7에서 `main.cpp` 의 역할은 단순 control apply 에서 끝나지 않고, **control input -> apply -> visible frame** 사이의 correlation anchor 를 만드는 쪽으로 확장됐다.
- 이 연결이 있어야 verification tooling 이 `ack -> encoded frame` 지연을 계산할 수 있다.
