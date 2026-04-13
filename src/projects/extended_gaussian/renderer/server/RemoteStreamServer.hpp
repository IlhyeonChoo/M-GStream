#pragma once

#include <core/graphics/RenderTarget.hpp>
#include "Config.hpp"
#include "ServerProtocol.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace sibr {

class MjpegStreamer;

struct SIBR_EXTENDED_GAUSSIAN_SERVER_EXPORT RendererHealthSnapshot {
    bool initialized = false;
    bool has_manifest = false;
    uint64_t frame_index = 0;
    double app_time_sec = 0.0;
    bool has_camera_pose = false;
    RemoteCameraPose camera_pose;
};

struct SIBR_EXTENDED_GAUSSIAN_SERVER_EXPORT ServerStats {
    bool running = false;
    std::string listen_host;
    int listen_port = 0;
    std::string www_root;
    uint64_t total_http_requests = 0;
    uint64_t active_stream_clients = 0;
    uint64_t stream_frames_captured = 0;
    uint64_t stream_frames_dropped = 0;
    uint64_t stream_queue_dropped = 0;
    uint64_t stream_frames_published = 0;
    uint64_t stream_latest_sequence = 0;
    int stream_width = 0;
    int stream_height = 0;
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

class SIBR_EXTENDED_GAUSSIAN_SERVER_EXPORT RemoteStreamServer {
public:
    explicit RemoteStreamServer(ServerOptions options);
    ~RemoteStreamServer();

    RemoteStreamServer(const RemoteStreamServer&) = delete;
    RemoteStreamServer& operator=(const RemoteStreamServer&) = delete;

    bool start(std::string& error);
    void stop();
    bool running() const;

    void setRendererHealthSnapshot(const RendererHealthSnapshot& snapshot);
    void submitRenderedFrame(const IRenderTarget& render_target, uint64_t source_frame_index);
    void releaseRenderThreadResources();
    bool consumePendingControlMessage(ControlMessage& message, uint64_t& sequence);
    void recordControlMessageApplied(uint64_t sequence, bool applied);

    RendererHealthSnapshot rendererHealthSnapshot() const;
    ServerStats stats() const;

private:
    class Impl;
    struct PendingControlMessage {
        uint64_t sequence = 0;
        ControlMessage message;
    };

    void serverThreadMain();
    std::string resolveWwwRoot(std::string& error) const;
    bool enqueueLatestControlMessage(const ControlMessage& message, uint64_t& sequence, bool& superseded_previous, std::string& error);

    ServerOptions options_;
    std::string www_root_;
    std::atomic<bool> stop_requested_{ false };
    std::atomic<bool> running_{ false };
    std::thread server_thread_;
    std::chrono::steady_clock::time_point start_time_ = std::chrono::steady_clock::time_point::min();

    mutable std::mutex renderer_health_mutex_;
    RendererHealthSnapshot renderer_health_;

    mutable std::mutex stats_mutex_;
    ServerStats stats_;

    mutable std::mutex control_message_mutex_;
    std::optional<PendingControlMessage> pending_control_message_;
    uint64_t next_control_sequence_ = 1;

    std::unique_ptr<MjpegStreamer> mjpeg_streamer_;
    std::unique_ptr<Impl> impl_;
};

} // namespace sibr
