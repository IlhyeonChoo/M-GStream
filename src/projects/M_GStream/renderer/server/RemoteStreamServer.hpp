#pragma once

#include <core/graphics/RenderTarget.hpp>
#include "Config.hpp"
#include "ServerProtocol.hpp"

#include <atomic>
#include <cstddef>
#include <chrono>
#include <deque>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace sibr {

class MjpegStreamer;

struct SIBR_MGSTREAM_SERVER_EXPORT TimingStatsSummary {
    uint64_t samples = 0;
    double average_ms = 0.0;
    double p50_ms = 0.0;
    double p95_ms = 0.0;
    double max_ms = 0.0;
};

struct SIBR_MGSTREAM_SERVER_EXPORT RendererHealthSnapshot {
    bool initialized = false;
    bool has_manifest = false;
    uint64_t frame_index = 0;
    double app_time_sec = 0.0;
    bool has_camera_pose = false;
    RemoteCameraPose camera_pose;
    std::string current_phase;
    std::vector<std::string> available_phases;
    size_t total_asset_count = 0;
    bool content_loaded = false;
    std::string loaded_source_kind;
    std::string loaded_source_path;
    std::string load_state = "idle";
    std::string last_load_error;
    uint64_t last_load_sequence = 0;
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
    std::string scene_name;
    std::string scene_path;
    std::string scene_source_kind;
    size_t scene_instance_count = 0;
    size_t scene_renderable_instance_count = 0;
    uint64_t scene_gaussian_count = 0;
    size_t scene_gpu_asset_bytes = 0;
    size_t scene_view_buffer_bytes = 0;
    size_t scene_scratch_buffer_bytes = 0;
    size_t scene_output_buffer_bytes = 0;
    size_t scene_vram_bytes = 0;
};

struct SIBR_MGSTREAM_SERVER_EXPORT ServerStats {
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
    uint64_t stream_encoded_bytes_total = 0;
    uint64_t stream_encoded_bytes_last = 0;
    double stream_encoded_bytes_average = 0.0;
    TimingStatsSummary stream_capture_to_raw_ready_ms;
    TimingStatsSummary stream_encode_ms;
    TimingStatsSummary stream_capture_to_encoded_ms;
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
    TimingStatsSummary control_receive_to_apply_ms;
};

class SIBR_MGSTREAM_SERVER_EXPORT RemoteStreamServer {
public:
    explicit RemoteStreamServer(ServerOptions options);
    ~RemoteStreamServer();

    RemoteStreamServer(const RemoteStreamServer&) = delete;
    RemoteStreamServer& operator=(const RemoteStreamServer&) = delete;

    bool start(std::string& error);
    void stop();
    bool running() const;

    void setRendererHealthSnapshot(const RendererHealthSnapshot& snapshot);
    void submitRenderedFrame(const IRenderTarget& render_target, uint64_t source_frame_index, uint64_t control_sequence);
    void releaseRenderThreadResources();
    bool consumePendingControlMessage(ControlMessage& message, uint64_t& sequence, std::chrono::steady_clock::time_point& received_at);
    void recordControlMessageApplied(uint64_t sequence, bool applied, double receive_to_apply_ms);

    RendererHealthSnapshot rendererHealthSnapshot() const;
    ServerStats stats() const;

private:
    class Impl;
    struct PendingControlMessage {
        uint64_t sequence = 0;
        std::chrono::steady_clock::time_point received_time = std::chrono::steady_clock::time_point::min();
        ControlMessage message;
    };

    void serverThreadMain();
    std::string resolveWwwRoot(std::string& error) const;
    bool enqueueControlMessage(const ControlMessage& message, uint64_t& sequence, bool& superseded_previous, std::string& error);
    void pushControlLatencySample(double value_ms);
    TimingStatsSummary summarizeControlLatencySamples() const;

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
    std::optional<PendingControlMessage> pending_camera_control_message_;
    std::deque<PendingControlMessage> pending_management_control_messages_;
    uint64_t next_control_sequence_ = 1;

    mutable std::mutex control_latency_mutex_;
    std::vector<double> control_receive_to_apply_samples_ms_;

    std::unique_ptr<MjpegStreamer> mjpeg_streamer_;
    std::unique_ptr<Impl> impl_;
};

} // namespace sibr
