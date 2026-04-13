#include "MjpegStreamer.hpp"

#include "JpegEncoder.hpp"

#include <core/system/Config.hpp>

#include <boost/asio.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr size_t kPboRingSize = 3;
constexpr size_t kRawQueueCapacity = 3;
constexpr int kDefaultJpegQuality = 80;
constexpr std::chrono::milliseconds kDefaultWaitTimeout(250);

bool isFenceReady(GLsync fence)
{
    if (fence == nullptr) {
        return false;
    }
    const GLenum result = glClientWaitSync(fence, 0, 0);
    return result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED;
}

} // namespace

namespace sibr {

struct MjpegStreamer::RawFrame {
    uint64_t source_frame_index = 0;
    int width = 0;
    int height = 0;
    bool bottom_up = true;
    std::vector<uint8_t> rgb_bytes;
};

struct MjpegStreamer::PboSlot {
    GLuint buffer_id = 0;
    GLsync fence = nullptr;
    bool pending = false;
    int width = 0;
    int height = 0;
    uint64_t source_frame_index = 0;
    size_t byte_size = 0;
};

MjpegStreamer::MjpegStreamer(ServerOptions options)
    : options_(std::move(options))
{
}

MjpegStreamer::~MjpegStreamer()
{
    stop();
}

void MjpegStreamer::start()
{
    if (running_.exchange(true)) {
        return;
    }
    stop_requested_.store(false);
    encoder_thread_ = std::thread(&MjpegStreamer::encoderThreadMain, this);
}

void MjpegStreamer::stop()
{
    stop_requested_.store(true);
    raw_cv_.notify_all();
    latest_cv_.notify_all();
    if (encoder_thread_.joinable()) {
        encoder_thread_.join();
    }
    running_.store(false);

    {
        std::lock_guard<std::mutex> lock(raw_mutex_);
        raw_queue_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(latest_mutex_);
        latest_frame_.reset();
    }
}

void MjpegStreamer::addClient()
{
    active_clients_.fetch_add(1);
}

void MjpegStreamer::removeClient()
{
    const size_t previous = active_clients_.load();
    if (previous == 0) {
        return;
    }
    active_clients_.fetch_sub(1);
}

size_t MjpegStreamer::clientCount() const
{
    return active_clients_.load();
}

void MjpegStreamer::ensureCaptureResources(int width, int height)
{
    if (width <= 0 || height <= 0) {
        return;
    }

    if (capture_width_ == width && capture_height_ == height && pbo_slots_.size() == kPboRingSize) {
        return;
    }

    destroyCaptureResources();
    capture_width_ = width;
    capture_height_ = height;
    pbo_slots_.resize(kPboRingSize);
    const size_t byte_size = static_cast<size_t>(width) * static_cast<size_t>(height) * 3;

    for (auto& slot : pbo_slots_) {
        glGenBuffers(1, &slot.buffer_id);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, slot.buffer_id);
        glBufferData(GL_PIXEL_PACK_BUFFER, static_cast<GLsizeiptr>(byte_size), nullptr, GL_STREAM_READ);
        slot.byte_size = byte_size;
        slot.width = width;
        slot.height = height;
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    next_submit_index_ = 0;
}

void MjpegStreamer::destroyCaptureResources()
{
    for (auto& slot : pbo_slots_) {
        if (slot.fence != nullptr) {
            glDeleteSync(slot.fence);
            slot.fence = nullptr;
        }
        if (slot.buffer_id != 0) {
            glDeleteBuffers(1, &slot.buffer_id);
            slot.buffer_id = 0;
        }
        slot.pending = false;
        slot.byte_size = 0;
    }
    pbo_slots_.clear();
    capture_width_ = 0;
    capture_height_ = 0;
    next_submit_index_ = 0;
}

void MjpegStreamer::drainReadyReadbacks()
{
    for (auto& slot : pbo_slots_) {
        if (!slot.pending || slot.fence == nullptr) {
            continue;
        }
        if (!isFenceReady(slot.fence)) {
            continue;
        }

        glBindBuffer(GL_PIXEL_PACK_BUFFER, slot.buffer_id);
        const void* mapped = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, static_cast<GLsizeiptr>(slot.byte_size), GL_MAP_READ_BIT);
        if (mapped == nullptr) {
            ++raw_frames_dropped_;
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
            glDeleteSync(slot.fence);
            slot.fence = nullptr;
            slot.pending = false;
            continue;
        }

        auto frame = std::make_shared<RawFrame>();
        frame->source_frame_index = slot.source_frame_index;
        frame->width = slot.width;
        frame->height = slot.height;
        frame->bottom_up = true;
        frame->rgb_bytes.resize(slot.byte_size);
        std::memcpy(frame->rgb_bytes.data(), mapped, slot.byte_size);
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

        glDeleteSync(slot.fence);
        slot.fence = nullptr;
        slot.pending = false;

        ++raw_frames_captured_;
        enqueueRawFrame(std::move(frame));
    }
}

void MjpegStreamer::enqueueRawFrame(std::shared_ptr<RawFrame> frame)
{
    if (!frame) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(raw_mutex_);
        if (raw_queue_.size() >= kRawQueueCapacity) {
            const size_t overflow = raw_queue_.size() - kRawQueueCapacity + 1;
            raw_queue_.erase(raw_queue_.begin(), raw_queue_.begin() + static_cast<std::ptrdiff_t>(overflow));
            raw_queue_dropped_.fetch_add(overflow);
        }
        raw_queue_.push_back(std::move(frame));
    }
    raw_cv_.notify_one();
}

void MjpegStreamer::captureFrame(const IRenderTarget& render_target, uint64_t source_frame_index)
{
    if (!running_.load()) {
        return;
    }

    drainReadyReadbacks();

    if (active_clients_.load() == 0) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const int fps = std::max(1, options_.stream_fps);
    const auto min_interval = std::chrono::microseconds(1000000 / fps);
    if (last_capture_submit_ != std::chrono::steady_clock::time_point::min() && (now - last_capture_submit_) < min_interval) {
        return;
    }

    ensureCaptureResources(static_cast<int>(render_target.w()), static_cast<int>(render_target.h()));
    if (pbo_slots_.empty()) {
        return;
    }

    PboSlot* selected_slot = nullptr;
    for (size_t attempt = 0; attempt < pbo_slots_.size(); ++attempt) {
        PboSlot& candidate = pbo_slots_[(next_submit_index_ + attempt) % pbo_slots_.size()];
        if (!candidate.pending) {
            selected_slot = &candidate;
            next_submit_index_ = (next_submit_index_ + attempt + 1) % pbo_slots_.size();
            break;
        }
    }

    if (selected_slot == nullptr) {
        ++raw_frames_dropped_;
        return;
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, render_target.fbo());
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, selected_slot->buffer_id);
    glReadPixels(0, 0,
        static_cast<GLsizei>(render_target.w()),
        static_cast<GLsizei>(render_target.h()),
        GL_RGB,
        GL_UNSIGNED_BYTE,
        nullptr);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    if (selected_slot->fence != nullptr) {
        glDeleteSync(selected_slot->fence);
    }
    selected_slot->fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    selected_slot->pending = selected_slot->fence != nullptr;
    selected_slot->width = static_cast<int>(render_target.w());
    selected_slot->height = static_cast<int>(render_target.h());
    selected_slot->source_frame_index = source_frame_index;
    selected_slot->byte_size = static_cast<size_t>(render_target.w()) * static_cast<size_t>(render_target.h()) * 3;
    last_capture_submit_ = now;
}

void MjpegStreamer::releaseRenderThreadResources()
{
    drainReadyReadbacks();
    destroyCaptureResources();
}

bool MjpegStreamer::waitForFrameAfter(
    uint64_t last_sequence,
    std::shared_ptr<const EncodedFrame>& frame,
    std::chrono::milliseconds timeout) const
{
    std::unique_lock<std::mutex> lock(latest_mutex_);
    const bool ready = latest_cv_.wait_for(lock, timeout, [&]() {
        return stop_requested_.load() || (latest_frame_ && latest_frame_->sequence > last_sequence);
    });
    if (!ready || stop_requested_.load() || !latest_frame_ || latest_frame_->sequence <= last_sequence) {
        frame.reset();
        return false;
    }
    frame = latest_frame_;
    return true;
}

MjpegStreamer::Stats MjpegStreamer::stats() const
{
    Stats output;
    output.active_clients = active_clients_.load();
    output.raw_frames_captured = raw_frames_captured_.load();
    output.raw_frames_dropped = raw_frames_dropped_.load();
    output.raw_queue_dropped = raw_queue_dropped_.load();
    output.jpeg_frames_published = jpeg_frames_published_.load();
    output.latest_sequence = latest_sequence_.load();
    output.output_width = options_.stream_width;
    output.output_height = options_.stream_height;
    output.target_fps = options_.stream_fps;
    return output;
}

void MjpegStreamer::encoderThreadMain()
{
    JpegEncoder encoder(kDefaultJpegQuality);
    std::vector<uint8_t> top_down_rgb;
    std::vector<uint8_t> resized_rgb;

    while (!stop_requested_.load()) {
        std::shared_ptr<RawFrame> raw_frame;
        {
            std::unique_lock<std::mutex> lock(raw_mutex_);
            raw_cv_.wait_for(lock, kDefaultWaitTimeout, [&]() {
                return stop_requested_.load() || !raw_queue_.empty();
            });
            if (stop_requested_.load()) {
                break;
            }
            if (raw_queue_.empty()) {
                continue;
            }
            raw_frame = std::move(raw_queue_.back());
            if (raw_queue_.size() > 1) {
                raw_queue_dropped_.fetch_add(raw_queue_.size() - 1);
            }
            raw_queue_.clear();
        }

        if (!raw_frame || raw_frame->width <= 0 || raw_frame->height <= 0) {
            continue;
        }

        const size_t row_bytes = static_cast<size_t>(raw_frame->width) * 3;
        top_down_rgb.resize(raw_frame->rgb_bytes.size());
        for (int row = 0; row < raw_frame->height; ++row) {
            const size_t src_index = static_cast<size_t>(raw_frame->height - 1 - row) * row_bytes;
            const size_t dst_index = static_cast<size_t>(row) * row_bytes;
            std::memcpy(top_down_rgb.data() + dst_index, raw_frame->rgb_bytes.data() + src_index, row_bytes);
        }

        int output_width = raw_frame->width;
        int output_height = raw_frame->height;
        const uint8_t* encode_rgb = top_down_rgb.data();

        if (options_.stream_width > 0 && options_.stream_height > 0 &&
            (options_.stream_width != raw_frame->width || options_.stream_height != raw_frame->height)) {
            cv::Mat input(raw_frame->height, raw_frame->width, CV_8UC3, top_down_rgb.data(), row_bytes);
            cv::Mat resized(options_.stream_height, options_.stream_width, CV_8UC3);
            cv::resize(input, resized, resized.size(), 0.0, 0.0, cv::INTER_LINEAR);
            const size_t resized_bytes = static_cast<size_t>(resized.cols) * static_cast<size_t>(resized.rows) * 3;
            resized_rgb.resize(resized_bytes);
            std::memcpy(resized_rgb.data(), resized.data, resized_bytes);
            encode_rgb = resized_rgb.data();
            output_width = resized.cols;
            output_height = resized.rows;
        }

        std::vector<unsigned char> jpeg_bytes;
        std::string encode_error;
        if (!encoder.encodeRgb(encode_rgb, output_width, output_height, jpeg_bytes, encode_error)) {
            SIBR_WRG << "MjpegStreamer JPEG encode failed: " << encode_error << std::endl;
            continue;
        }

        auto frame = std::make_shared<EncodedFrame>();
        frame->sequence = latest_sequence_.fetch_add(1) + 1;
        frame->source_frame_index = raw_frame->source_frame_index;
        frame->width = output_width;
        frame->height = output_height;
        frame->jpeg_bytes = std::make_shared<std::vector<unsigned char>>(std::move(jpeg_bytes));

        {
            std::lock_guard<std::mutex> lock(latest_mutex_);
            latest_frame_ = frame;
        }
        jpeg_frames_published_.fetch_add(1);
        latest_cv_.notify_all();
    }
}

} // namespace sibr
