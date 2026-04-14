# `MjpegStreamer` timing / header metadata 상세

이 문서는 M7에서 `MjpegStreamer` 가 단순 JPEG fan-out 경로에서 **raw readback, encode, publish 시점을 수치화하는 계측 파이프라인**으로 확장된 변화를 정리한다.

## 디렉터리: `src/projects/extended_gaussian/renderer/server`

### 파일: `src/projects/extended_gaussian/renderer/server/MjpegStreamer.hpp`

#### 초기 코드

```cpp
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
```

```cpp
void captureFrame(const IRenderTarget& render_target, uint64_t source_frame_index);
```

#### 현재 코드

```cpp
struct TimingSummary {
    uint64_t samples = 0;
    double average_ms = 0.0;
    double p50_ms = 0.0;
    double p95_ms = 0.0;
    double max_ms = 0.0;
};

struct EncodedFrame {
    uint64_t sequence = 0;
    uint64_t source_frame_index = 0;
    uint64_t control_sequence = 0;
    int width = 0;
    int height = 0;
    double capture_to_raw_ready_ms = 0.0;
    double encode_ms = 0.0;
    double capture_to_encoded_ms = 0.0;
    uint64_t encoded_unix_time_ms = 0;
    std::shared_ptr<std::vector<unsigned char>> jpeg_bytes;
};

struct Stats {
    ...
    int target_fps = 0;
    uint64_t encoded_bytes_total = 0;
    uint64_t encoded_bytes_last = 0;
    double encoded_bytes_average = 0.0;
    TimingSummary capture_to_raw_ready_ms;
    TimingSummary encode_ms;
    TimingSummary capture_to_encoded_ms;
};
```

```cpp
void captureFrame(const IRenderTarget& render_target, uint64_t source_frame_index, uint64_t control_sequence);
```

#### 바뀐 이유

- M6의 `EncodedFrame` 은 sequence/size/jpeg bytes 만 가져서, viewer 쪽 control apply 결과나 encoder latency 를 frame 에 붙일 수 없었다.
- M7에서는 frame-level correlation 을 위해 `control_sequence` 와 timing field 를 `EncodedFrame` 에 넣고, 누적 요약은 `Stats` 에 넣었다.
- `captureFrame(...)` 시그니처에 `control_sequence` 를 추가한 이유는 render submit 시점의 visible control sequence 를 raw frame 까지 잃지 않고 전달하기 위해서다.

### 파일: `src/projects/extended_gaussian/renderer/server/MjpegStreamer.cpp`

#### 초기 코드

```cpp
struct RawFrame {
    uint64_t source_frame_index = 0;
    int width = 0;
    int height = 0;
    bool bottom_up = true;
    std::vector<uint8_t> rgb_bytes;
};

struct PboSlot {
    GLuint pbo = 0;
    GLsync fence = nullptr;
    bool pending = false;
    int width = 0;
    int height = 0;
    uint64_t source_frame_index = 0;
    size_t byte_size = 0;
};
```

```cpp
void MjpegStreamer::captureFrame(const IRenderTarget& render_target, uint64_t source_frame_index)
{
    ...
    selected_slot->width = static_cast<int>(render_target.w());
    selected_slot->height = static_cast<int>(render_target.h());
    selected_slot->source_frame_index = source_frame_index;
    selected_slot->byte_size = static_cast<size_t>(render_target.w()) * static_cast<size_t>(render_target.h()) * 3;
    last_capture_submit_ = now;
}
```

```cpp
auto frame = std::make_shared<EncodedFrame>();
frame->sequence = latest_sequence_.fetch_add(1) + 1;
frame->source_frame_index = raw_frame->source_frame_index;
frame->width = output_width;
frame->height = output_height;
frame->jpeg_bytes = std::make_shared<std::vector<unsigned char>>(std::move(jpeg_bytes));
```

#### 현재 코드

```cpp
struct RawFrame {
    uint64_t source_frame_index = 0;
    uint64_t control_sequence = 0;
    int width = 0;
    int height = 0;
    bool bottom_up = true;
    std::chrono::steady_clock::time_point capture_submit_time = std::chrono::steady_clock::time_point::min();
    std::chrono::steady_clock::time_point raw_ready_time = std::chrono::steady_clock::time_point::min();
    std::chrono::system_clock::time_point capture_submit_wall_time = std::chrono::system_clock::time_point::min();
    std::vector<uint8_t> rgb_bytes;
};

struct PboSlot {
    ...
    uint64_t source_frame_index = 0;
    uint64_t control_sequence = 0;
    size_t byte_size = 0;
    std::chrono::steady_clock::time_point capture_submit_time = std::chrono::steady_clock::time_point::min();
    std::chrono::system_clock::time_point capture_submit_wall_time = std::chrono::system_clock::time_point::min();
};
```

```cpp
void MjpegStreamer::captureFrame(const IRenderTarget& render_target, uint64_t source_frame_index, uint64_t control_sequence)
{
    ...
    selected_slot->source_frame_index = source_frame_index;
    selected_slot->control_sequence = control_sequence;
    selected_slot->byte_size = static_cast<size_t>(render_target.w()) * static_cast<size_t>(render_target.h()) * 3;
    selected_slot->capture_submit_time = now;
    selected_slot->capture_submit_wall_time = std::chrono::system_clock::now();
    last_capture_submit_ = now;
}
```

```cpp
frame->source_frame_index = slot.source_frame_index;
frame->control_sequence = slot.control_sequence;
...
frame->capture_submit_time = slot.capture_submit_time;
frame->raw_ready_time = std::chrono::steady_clock::now();
...
const double capture_to_raw_ms = toMilliseconds(frame->raw_ready_time - frame->capture_submit_time);
pushLatencySample(capture_to_raw_ready_samples_ms_, capture_to_raw_ms);
```

```cpp
const auto encode_start = std::chrono::steady_clock::now();
...
const auto encode_finish = std::chrono::steady_clock::now();

auto frame = std::make_shared<EncodedFrame>();
frame->sequence = latest_sequence_.fetch_add(1) + 1;
frame->source_frame_index = raw_frame->source_frame_index;
frame->control_sequence = raw_frame->control_sequence;
frame->width = output_width;
frame->height = output_height;
frame->encode_ms = toMilliseconds(encode_finish - encode_start);
if (raw_frame->capture_submit_time != std::chrono::steady_clock::time_point::min()) {
    frame->capture_to_encoded_ms = toMilliseconds(encode_finish - raw_frame->capture_submit_time);
}
if (raw_frame->capture_submit_time != std::chrono::steady_clock::time_point::min() &&
    raw_frame->raw_ready_time != std::chrono::steady_clock::time_point::min()) {
    frame->capture_to_raw_ready_ms = toMilliseconds(raw_frame->raw_ready_time - raw_frame->capture_submit_time);
}
frame->encoded_unix_time_ms = toUnixTimeMilliseconds(std::chrono::system_clock::now());
frame->jpeg_bytes = std::make_shared<std::vector<unsigned char>>(std::move(jpeg_bytes));

{
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    pushLatencySample(encode_samples_ms_, frame->encode_ms);
    pushLatencySample(capture_to_encoded_samples_ms_, frame->capture_to_encoded_ms);
    encoded_bytes_last_ = frame->jpeg_bytes ? frame->jpeg_bytes->size() : 0;
    encoded_bytes_total_ += encoded_bytes_last_;
}
```

#### 바뀐 이유

- M7 verification 은 `control send -> ack -> apply -> encoded frame` 흐름을 서로 다른 채널에서 맞춰 봐야 하므로, raw frame 과 encoded frame 모두 timing/context metadata 를 가져야 했다.
- `RawFrame` / `PboSlot` 에 capture submit 시점과 control sequence 를 붙인 이유는 GL readback 준비 시점과 최종 JPEG publish 시점을 같은 원인 프레임으로 묶기 위해서다.
- encoder thread 에서는 `encode_ms`, `capture_to_encoded_ms`, `encoded_unix_time_ms` 를 확정하고, 동시에 누적 summary 를 갱신해 `/healthz` 에서도 같은 수치를 읽을 수 있게 했다.
- `metrics_mutex_` 와 latency sample ring 은 최근 이력만 유지하면서 p50/p95를 계산하기 위한 최소 장치다.

## 요약

- M7의 `MjpegStreamer` 변화는 JPEG delivery 자체를 바꾼 것이 아니라, **각 frame 이 언제 raw-ready 되었고 언제 encode 되었는지**를 외부에서 재구성할 수 있게 만든 것이다.
- 이 계측 덕분에 single-client/two-client/soak/control-visible proxy 측정을 코드 변경 없이 스크립트로 반복할 수 있다.
