#pragma once

#include <core/graphics/RenderTarget.hpp>
#include "ServerProtocol.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace sibr {

class MjpegStreamer {
public:
    struct EncodedFrame {
        uint64_t sequence = 0;
        uint64_t source_frame_index = 0;
        int width = 0;
        int height = 0;
        std::shared_ptr<std::vector<unsigned char>> jpeg_bytes;
    };

    struct Stats {
        size_t active_clients = 0;
        uint64_t raw_frames_captured = 0;
        uint64_t raw_frames_dropped = 0;
        uint64_t raw_queue_dropped = 0;
        uint64_t jpeg_frames_published = 0;
        uint64_t latest_sequence = 0;
        int output_width = 0;
        int output_height = 0;
        int target_fps = 0;
    };

    explicit MjpegStreamer(ServerOptions options);
    ~MjpegStreamer();

    void start();
    void stop();

    void addClient();
    void removeClient();
    size_t clientCount() const;

    void captureFrame(const IRenderTarget& render_target, uint64_t source_frame_index);
    void releaseRenderThreadResources();

    bool waitForFrameAfter(
        uint64_t last_sequence,
        std::shared_ptr<const EncodedFrame>& frame,
        std::chrono::milliseconds timeout) const;

    Stats stats() const;

private:
    struct RawFrame;
    struct PboSlot;

    void ensureCaptureResources(int width, int height);
    void destroyCaptureResources();
    void drainReadyReadbacks();
    void enqueueRawFrame(std::shared_ptr<RawFrame> frame);
    void encoderThreadMain();

    ServerOptions options_;

    std::atomic<bool> running_{ false };
    std::atomic<bool> stop_requested_{ false };
    std::atomic<size_t> active_clients_{ 0 };
    std::atomic<uint64_t> raw_frames_captured_{ 0 };
    std::atomic<uint64_t> raw_frames_dropped_{ 0 };
    std::atomic<uint64_t> raw_queue_dropped_{ 0 };
    std::atomic<uint64_t> jpeg_frames_published_{ 0 };
    std::atomic<uint64_t> latest_sequence_{ 0 };

    std::chrono::steady_clock::time_point last_capture_submit_ = std::chrono::steady_clock::time_point::min();
    int capture_width_ = 0;
    int capture_height_ = 0;
    size_t next_submit_index_ = 0;
    std::vector<PboSlot> pbo_slots_;

    mutable std::mutex raw_mutex_;
    std::condition_variable raw_cv_;
    std::vector<std::shared_ptr<RawFrame>> raw_queue_;

    mutable std::mutex latest_mutex_;
    mutable std::condition_variable latest_cv_;
    std::shared_ptr<const EncodedFrame> latest_frame_;

    std::thread encoder_thread_;
};

} // namespace sibr
