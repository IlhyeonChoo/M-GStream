# `RemoteStreamServer` WebSocket control 상세

이 문서는 M6에서 `RemoteStreamServer` 가 MJPEG-only HTTP server 에서 WebSocket control endpoint 를 함께 가지는 runtime 으로 확장된 변화를 정리한다.

## 디렉터리: `src/projects/extended_gaussian/renderer/server`

### 파일: `src/projects/extended_gaussian/renderer/server/RemoteStreamServer.hpp`

#### 초기 코드

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
    uint64_t active_stream_clients = 0;
    uint64_t stream_frames_captured = 0;
    ...
    int stream_fps = 0;
};

class RemoteStreamServer {
public:
    void setRendererHealthSnapshot(const RendererHealthSnapshot& snapshot);
    void submitRenderedFrame(const IRenderTarget& render_target, uint64_t source_frame_index);
    void releaseRenderThreadResources();
};
```

#### 현재 코드

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
    uint64_t control_messages_queued = 0;
    uint64_t control_messages_rejected = 0;
    uint64_t control_messages_superseded = 0;
    uint64_t control_messages_applied = 0;
    uint64_t control_apply_failures = 0;
    uint64_t control_latest_received_sequence = 0;
    uint64_t control_last_applied_sequence = 0;
    bool control_message_pending = false;
};
```

```cpp
class RemoteStreamServer {
public:
    ...
    bool consumePendingControlMessage(ControlMessage& message, uint64_t& sequence);
    void recordControlMessageApplied(uint64_t sequence, bool applied);

private:
    struct PendingControlMessage {
        uint64_t sequence = 0;
        ControlMessage message;
    };

    bool enqueueLatestControlMessage(
        const ControlMessage& message,
        uint64_t& sequence,
        bool& superseded_previous,
        std::string& error);

    mutable std::mutex control_message_mutex_;
    std::optional<PendingControlMessage> pending_control_message_;
    uint64_t next_control_sequence_ = 1;
};
```

#### 바뀐 이유

- M5의 `RemoteStreamServer` 는 outbound MJPEG 전달만 담당했기 때문에, inbound control queue 와 apply bookkeeping 이 전혀 없었다.
- M6에서는 server thread 와 viewer main thread 사이에 **단일 latest-only mailbox** 가 필요했다.
- `RendererHealthSnapshot` 에 camera pose 를 추가한 것은 `ready` frame 과 `/healthz` 둘 다 현재 시점을 노출하기 위해서다.
- `ServerStats` control 필드는 단순 연결 수뿐 아니라 rejected / superseded / applied / pending 상태를 한 번에 관찰할 수 있게 하기 위한 것이다.

### 파일: `src/projects/extended_gaussian/renderer/server/RemoteStreamServer.cpp`

#### 초기 코드

```cpp
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

namespace http = beast::http;
constexpr const char* kVersionName = "m5-mjpeg-stream";
```

```cpp
std::string healthJson(const ServerStats& stats, const RendererHealthSnapshot& renderer, double uptime_sec)
{
    std::ostringstream stream;
    stream
        << "{\n"
        << "  \"ok\": true,\n"
        << "  \"service\": \"...\",\n"
        << "  \"version\": \"m5-mjpeg-stream\",\n"
        << "  \"stream\": { ... },\n"
        << "  \"renderer\": {\n"
        << "    \"initialized\": ...,\n"
        << "    \"has_manifest\": ...,\n"
        << "    \"frame_index\": ...,\n"
        << "    \"app_time_sec\": ...\n"
        << "  }\n"
        << "}";
}
```

```cpp
if (target == "/control") {
    auto response = makeTextResponse(
        http::status::upgrade_required,
        "application/json; charset=utf-8",
        "{\"ok\":false,\"error\":\"WebSocket control is not implemented in M5.\"}\n");
    response.set(http::field::upgrade, "websocket");
    http::write(socket, response, write_error);
    closeSocket(socket);
    continue;
}
```

#### 현재 코드

```cpp
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>

namespace http = beast::http;
namespace websocket = beast::websocket;
constexpr const char* kVersionName = "m6-websocket-control";
```

```cpp
struct ControlClientSession {
    std::atomic<bool> stop_requested{ false };
    std::atomic<bool> finished{ false };
    std::mutex socket_mutex;
    tcp::socket* socket = nullptr;
    std::thread thread;
};
```

```cpp
std::string controlReadyJson(const RendererHealthSnapshot& renderer) { ... }
std::string controlAckJson(uint64_t sequence, bool superseded_previous) { ... }
std::string controlErrorJson(const std::string& error) { ... }

std::string healthJson(const ServerStats& stats, const RendererHealthSnapshot& renderer, double uptime_sec)
{
    std::ostringstream stream;
    stream
        << "{\n"
        << "  \"ok\": true,\n"
        << "  \"service\": \"...\",\n"
        << "  \"version\": \"m6-websocket-control\",\n"
        << "  \"stream\": { ... },\n"
        << "  \"control\": {\n"
        << "    \"websocket_path\": \"/control\",\n"
        << "    \"queue_mode\": \"latest_only\",\n"
        << "    \"messages_received\": ...,\n"
        << "    \"messages_applied\": ...,\n"
        << "    \"last_applied_sequence\": ...,\n"
        << "    \"message_pending\": ...\n"
        << "  },\n"
        << "  \"renderer\": {\n"
        << "    \"camera_pose_available\": ...\n"
        << "  }\n"
        << "}";
}
```

```cpp
bool RemoteStreamServer::enqueueLatestControlMessage(...)
{
    std::lock_guard<std::mutex> lock(control_message_mutex_);
    superseded_previous = pending_control_message_.has_value();
    PendingControlMessage pending;
    pending.sequence = next_control_sequence_++;
    pending.message = message;
    sequence = pending.sequence;
    pending_control_message_ = pending;
    ...
}
```

```cpp
if (target == "/control") {
    if (!websocket::is_upgrade(request)) {
        ...
        controlErrorJson("WebSocket upgrade required for /control.");
        ...
        continue;
    }

    auto session = std::make_unique<ControlClientSession>();
    session->thread = std::thread([this, session_ptr, accepted_socket = std::move(socket), accepted_request = std::move(request)]() mutable {
        websocket::stream<tcp::socket> ws(std::move(accepted_socket));
        ws.accept(accepted_request, ws_error);
        ...
        writeWebSocketText(ws, controlReadyJson(rendererHealthSnapshot()), ws_error);
        while (!stop_requested_.load() && !session_ptr->stop_requested.load()) {
            ws.read(ws_buffer, ws_error);
            if (ws.got_binary()) {
                response_payload = controlErrorJson("Binary WebSocket messages are not supported.");
                writeWebSocketText(ws, response_payload, ws_error);
                continue;
            }
            const ParseControlMessageResult parsed = ParseControlMessageJson(payload);
            if (!parsed) {
                response_payload = controlErrorJson(parsed.error);
                writeWebSocketText(ws, response_payload, ws_error);
                continue;
            }
            enqueueLatestControlMessage(parsed.message, sequence, superseded_previous, enqueue_error);
            response_payload = controlAckJson(sequence, superseded_previous);
            writeWebSocketText(ws, response_payload, ws_error);
        }
        ws.close(websocket::close_code::normal, close_error);
        finish();
    });
    ...
    continue;
}
```

```cpp
void RemoteStreamServer::recordControlMessageApplied(uint64_t sequence, bool applied)
{
    std::lock_guard<std::mutex> lock(stats_mutex_);
    if (applied) {
        ++stats_.control_messages_applied;
        stats_.control_last_applied_sequence = sequence;
    } else {
        ++stats_.control_apply_failures;
    }
}
```

#### 바뀐 이유

- M6의 핵심은 `/control` 을 placeholder route 에서 실제 WebSocket endpoint 로 바꾸는 것이다.
- `ready` / `ack` / `error` frame 을 따로 둔 이유는 browser-side reference client 와 CLI smoke 둘 다 공통으로 이해하기 쉬운 최소 contract 를 만들기 위해서다.
- queue policy 를 `latest_only` 로 고정한 이유는 camera control 이 누적 backlog 를 재생하는 타입의 작업이 아니기 때문이다.
- mailbox 를 단일 슬롯으로 유지하면 server thread 와 render loop 사이의 동기화 비용과 stale payload 문제를 함께 줄일 수 있다.
- shutdown path 에 control session socket close 를 명시적으로 넣은 이유는 blocking `ws.read()` 를 가진 worker thread 를 clean stop 하기 위해서다.
- `last_applied_sequence` 는 실제 apply 성공 시점에만 갱신되도록 유지해, `apply_failures` 와 의미가 충돌하지 않게 했다.

## 요약

- M6의 `RemoteStreamServer` 변화는 단순 route 추가가 아니라, WebSocket session lifecycle / latest-only mailbox / health telemetry / clean shutdown 을 한 묶음으로 정리한 것이다.
- 이 구조 덕분에 현재 브랜치는 MJPEG preview 와 camera control 을 같은 process 안에서 안정적으로 제공할 수 있다.
