# `RemoteStreamServer` verification metrics 상세

이 문서는 M7에서 `RemoteStreamServer` 가 M6의 WebSocket/MJPEG runtime 위에 **health summary, control latency aggregation, MJPEG tracing header** 를 추가한 변화를 정리한다.

## 디렉터리: `src/projects/extended_gaussian/renderer/server`

### 파일: `src/projects/extended_gaussian/renderer/server/RemoteStreamServer.hpp`

#### 초기 코드

```cpp
struct RendererHealthSnapshot {
    bool initialized = false;
    bool has_manifest = false;
    uint64_t frame_index = 0;
    double app_time_sec = 0.0;
    bool has_camera_pose = false;
    RemoteCameraPose camera_pose;
};

struct ServerStats {
    ...
    int stream_fps = 0;
    uint64_t active_control_clients = 0;
    uint64_t control_messages_received = 0;
    ...
    uint64_t control_last_applied_sequence = 0;
    bool control_message_pending = false;
};
```

```cpp
void submitRenderedFrame(const IRenderTarget& render_target, uint64_t source_frame_index);
bool consumePendingControlMessage(ControlMessage& message, uint64_t& sequence);
void recordControlMessageApplied(uint64_t sequence, bool applied);
```

#### 현재 코드

```cpp
struct TimingStatsSummary {
    uint64_t samples = 0;
    double average_ms = 0.0;
    double p50_ms = 0.0;
    double p95_ms = 0.0;
    double max_ms = 0.0;
};
```

```cpp
struct ServerStats {
    ...
    int stream_fps = 0;
    uint64_t stream_encoded_bytes_total = 0;
    uint64_t stream_encoded_bytes_last = 0;
    double stream_encoded_bytes_average = 0.0;
    TimingStatsSummary stream_capture_to_raw_ready_ms;
    TimingStatsSummary stream_encode_ms;
    TimingStatsSummary stream_capture_to_encoded_ms;
    ...
    uint64_t control_last_applied_sequence = 0;
    bool control_message_pending = false;
    TimingStatsSummary control_receive_to_apply_ms;
};
```

```cpp
void submitRenderedFrame(const IRenderTarget& render_target, uint64_t source_frame_index, uint64_t control_sequence);
bool consumePendingControlMessage(
    ControlMessage& message,
    uint64_t& sequence,
    std::chrono::steady_clock::time_point& received_at);
void recordControlMessageApplied(uint64_t sequence, bool applied, double receive_to_apply_ms);
```

#### 바뀐 이유

- M6의 `ServerStats` 는 연결 수와 queue/apply counter 까지만 담고 있어서, verification report 에 필요한 latency/byte 계열 수치를 health surface 로 내보낼 수 없었다.
- M7에서는 stream 계열과 control 계열 모두 공통으로 쓸 수 있는 `TimingStatsSummary` 를 만들고, `ServerStats` 에 timing/byte 요약을 붙였다.
- public API 도 함께 넓혀서 `submitRenderedFrame(...)` 는 visible control sequence 를 받고, `consumePendingControlMessage(...)` 는 receive timestamp 를 넘기며, `recordControlMessageApplied(...)` 는 latency sample 을 받도록 바뀌었다.

### 파일: `src/projects/extended_gaussian/renderer/server/RemoteStreamServer.cpp`

#### 초기 코드

```cpp
constexpr const char* kVersionName = "m6-websocket-control";
```

```cpp
std::string healthJson(const ServerStats& stats, const RendererHealthSnapshot& renderer, double uptime_sec)
{
    stream
        << "..."
        << "stream.frames_published=" << stats.stream_frames_published
        << "stream.latest_sequence=" << stats.stream_latest_sequence
        << "stream.width=" << stats.stream_width
        << "stream.height=" << stats.stream_height
        << "stream.fps=" << stats.stream_fps
        << "control.last_applied_sequence=" << stats.control_last_applied_sequence
        << "control.message_pending=" << stats.control_message_pending;
}
```

```cpp
std::string controlAckJson(uint64_t sequence, bool superseded_previous)
{
    stream
        << "{"
        << "\"ok\":true,"
        << "\"type\":\"ack\","
        << "\"status\":\"queued\","
        << "\"queue_mode\":\"latest_only\","
        << "\"sequence\":" << sequence << ","
        << "\"superseded_previous\":" << (superseded_previous ? "true" : "false")
        << "}";
}
```

```cpp
std::string mjpegPartHeader(const sibr::MjpegStreamer::EncodedFrame& frame)
{
    stream
        << "X-Sequence: " << frame.sequence << "\r\n"
        << "X-Source-Frame-Index: " << frame.source_frame_index << "\r\n"
        << "X-Width: " << frame.width << "\r\n"
        << "X-Height: " << frame.height << "\r\n\r\n";
}
```

#### 현재 코드

```cpp
constexpr const char* kVersionName = "m7-integration-verification";
```

```cpp
std::string healthJson(const ServerStats& stats, const RendererHealthSnapshot& renderer, double uptime_sec)
{
    stream
        << "..."
        << "stream.frames_published=" << stats.stream_frames_published
        << "stream.latest_sequence=" << stats.stream_latest_sequence
        << "stream.encoded_bytes_total=" << stats.stream_encoded_bytes_total
        << "stream.encoded_bytes_last=" << stats.stream_encoded_bytes_last
        << "stream.encoded_bytes_average=" << stats.stream_encoded_bytes_average
        << "stream.timing.capture_to_raw_ready=" << timingSummaryJson(stats.stream_capture_to_raw_ready_ms)
        << "stream.timing.encode=" << timingSummaryJson(stats.stream_encode_ms)
        << "stream.timing.capture_to_encoded=" << timingSummaryJson(stats.stream_capture_to_encoded_ms)
        << "control.last_applied_sequence=" << stats.control_last_applied_sequence
        << "control.message_pending=" << stats.control_message_pending
        << "control.timing.receive_to_apply=" << timingSummaryJson(stats.control_receive_to_apply_ms);
}
```

```cpp
std::string controlAckJson(uint64_t sequence, bool superseded_previous)
{
    const uint64_t server_ack_unix_ms = unixTimeMillisecondsNow();
    stream
        << "{"
        << "\"ok\":true,"
        << "\"type\":\"ack\","
        << "\"status\":\"queued\","
        << "\"queue_mode\":\"latest_only\","
        << "\"sequence\":" << sequence << ","
        << "\"superseded_previous\":" << (superseded_previous ? "true" : "false") << ","
        << "\"server_ack_unix_ms\":" << server_ack_unix_ms
        << "}";
}
```

```cpp
std::string mjpegPartHeader(const sibr::MjpegStreamer::EncodedFrame& frame)
{
    stream
        << "X-Sequence: " << frame.sequence << "\r\n"
        << "X-Source-Frame-Index: " << frame.source_frame_index << "\r\n"
        << "X-Control-Sequence: " << frame.control_sequence << "\r\n"
        << "X-Width: " << frame.width << "\r\n"
        << "X-Height: " << frame.height << "\r\n"
        << "X-Capture-To-Raw-Ready-Ms: " << frame.capture_to_raw_ready_ms << "\r\n"
        << "X-Encode-Ms: " << frame.encode_ms << "\r\n"
        << "X-Capture-To-Encoded-Ms: " << frame.capture_to_encoded_ms << "\r\n"
        << "X-Encoded-Unix-Ms: " << frame.encoded_unix_time_ms << "\r\n\r\n";
}
```

```cpp
void RemoteStreamServer::recordControlMessageApplied(uint64_t sequence, bool applied, double receive_to_apply_ms)
{
    if (applied) {
        pushControlLatencySample(receive_to_apply_ms);
    }

    TimingStatsSummary latency_summary = summarizeControlLatencySamples();

    std::lock_guard<std::mutex> lock(stats_mutex_);
    if (applied) {
        ++stats_.control_messages_applied;
        stats_.control_last_applied_sequence = sequence;
    } else {
        ++stats_.control_apply_failures;
    }
    stats_.control_receive_to_apply_ms = latency_summary;
}
```

#### 바뀐 이유

- M7의 목표는 “동작한다”를 넘어서 **무슨 시점에, 어느 정도 지연으로, 어떤 frame metadata 와 함께 보였는지**를 증거로 남기는 것이었다.
- `healthJson(...)` 은 verification script 가 바로 읽을 수 있도록 stream/control timing summary 와 byte stats 를 JSON 으로 노출하도록 확장됐다.
- `controlAckJson(...)` 의 `server_ack_unix_ms` 는 WebSocket ack 시점과 MJPEG encoded 시점을 다른 채널에서 맞춰 보기 위한 최소 anchor 다.
- MJPEG part header 는 외부 브라우저/측정 스크립트가 별도 API 없이도 frame-level timing 을 읽을 수 있게 만들기 위해 확장됐다.
- `recordControlMessageApplied(...)` 와 `summarizeControlLatencySamples()` 는 M6의 카운터 위에 실제 latency distribution 을 얹어, report 에 `p50/p95/max` 를 넣을 수 있게 한다.

## 요약

- M7의 `RemoteStreamServer` 변화는 protocol 기능을 더 늘린 것이 아니라, **기존 runtime 을 검증 가능한 표면으로 바꾼 것**이다.
- 결과적으로 `/healthz`, WebSocket `ack`, MJPEG part header 세 채널이 같은 sequence/timing 계열 정보를 공유하게 됐다.
