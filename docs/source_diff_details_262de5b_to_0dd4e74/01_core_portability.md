# `src/core` portability 수정 상세

이 문서는 M1에서 Ubuntu 24 toolchain과 라이브러리 버전 차이를 흡수하기 위해 `src/core/*`에 들어간 수정을 정리한다.

## 디렉터리: `src/core/raycaster`

### 파일 묶음

- `src/core/raycaster/Raycaster.hpp`
- `src/core/raycaster/Raycaster.cpp`

#### 초기 코드

```cpp
// Raycaster.hpp
#  include <embree3/rtcore.h>
#  include <embree3/rtcore_ray.h>
```

```cpp
// Raycaster.cpp
RTCIntersectContext context;
rtcInitIntersectContext(&context);
rtcIntersect1(*_scene.get(), &context, &rh);
...
rtcOccluded1(*_scene.get(), &context, &ray);
```

#### 현재 코드

```cpp
// Raycaster.hpp
#  if __has_include(<embree3/rtcore.h>)
#    include <embree3/rtcore.h>
#    include <embree3/rtcore_ray.h>
#  elif __has_include(<embree4/rtcore.h>)
#    include <embree4/rtcore.h>
#    include <embree4/rtcore_ray.h>
#  else
#    error "Embree headers not found: expected embree3/rtcore.h or embree4/rtcore.h"
#  endif
```

```cpp
// Raycaster.cpp
void rtcIntersect1Compat(RTCScene scene, RTCRayHit* rayhit)
{
#if RTC_VERSION_MAJOR >= 4
    rtcIntersect1(scene, rayhit, nullptr);
#else
    RTCIntersectContext context;
    rtcInitIntersectContext(&context);
    rtcIntersect1(scene, &context, rayhit);
#endif
}
```

#### 바뀐 이유

- Ubuntu 24 환경에서는 Embree 3과 Embree 4가 모두 현실적인 대상이어서, header 경로를 하드코딩하면 configure/build가 깨졌다.
- 또한 Embree 4는 `rtcIntersect*` / `rtcOccluded*` 호출 시그니처가 바뀌므로, 기존 직접 호출을 그대로 쓰면 컴파일이 실패한다.
- M1에서는 런타임 동작을 바꾸는 대신, 같은 Raycaster API를 유지하면서 하위에서 compat wrapper로 버전 차이를 흡수했다.

## 디렉터리: `src/core/video`

### 파일 묶음

- `src/core/video/FFmpegVideoEncoder.cpp`
- `src/core/video/FFmpegVideoEncoder.hpp`

#### 초기 코드

```cpp
// FFmpegVideoEncoder.cpp
if (!ffmpegInitDone) {
    av_register_all();
    ffmpegInitDone = true;
}
...
pCodecCtx = video_st->codec;
...
int ret = avcodec_encode_video2(pCodecCtx, pkt, frameYUV, &got_picture);
```

```cpp
// FFmpegVideoEncoder.hpp
AVFormatContext* pFormatCtx;
AVOutputFormat* fmt;
AVStream* video_st;
AVCodecContext* pCodecCtx;
AVCodec* pCodec;
AVPacket * pkt;
```

#### 현재 코드

```cpp
// FFmpegVideoEncoder.cpp
if (!ffmpegInitDone) {
    SIBR_LOG << "[FFMPEG] Initializing." << std::endl;
    ffmpegInitDone = true;
}
...
pCodecCtx = avcodec_alloc_context3(pCodec);
...
res = avcodec_parameters_from_context(video_st->codecpar, pCodecCtx);
res = avformat_write_header(pFormatCtx, NULL);
headerWritten = true;
...
int ret = avcodec_send_frame(pCodecCtx, frame);
while (ret >= 0) {
    ret = avcodec_receive_packet(pCodecCtx, pkt);
    ...
    ret = av_interleaved_write_frame(pFormatCtx, pkt);
}
```

```cpp
// FFmpegVideoEncoder.hpp
AVFormatContext* pFormatCtx = NULL;
const AVOutputFormat* fmt = NULL;
AVStream* video_st = NULL;
AVCodecContext* pCodecCtx = NULL;
const AVCodec* pCodec = NULL;
AVPacket * pkt = NULL;
bool headerWritten = false;
```

#### 바뀐 이유

- Ubuntu 24에서 잡히는 최신 FFmpeg 계열은 `av_register_all`, `video_st->codec`, `avcodec_encode_video2` 같은 구 API에 그대로 의존하기 어렵다.
- M1에서는 encoder를 현대 API (`avcodec_alloc_context3`, `avcodec_send_frame`, `avcodec_receive_packet`)로 옮기고, header/trailer flush와 context 해제를 더 엄격하게 정리했다.
- pointer 기본 초기화와 `headerWritten` 플래그를 넣은 이유는 실패 경로가 많아진 최신 API에서 `close()`가 안전하게 동작하도록 만들기 위해서다.

### 파일: `src/core/video/VideoUtils.hpp`

#### 초기 코드

```cpp
uint getModeIndice() const {
    uint mode, mode_size = 0;
    for (const auto & [key, val] : bins) {
        if (val > mode_size) {
            mode_size = val;
            mode = key;
        }
    }
    return mode;
}
```

#### 현재 코드

```cpp
uint getModeIndice() const {
    uint mode = 0, mode_size = 0;
    for (uint key = 0; key < bins.size(); ++key) {
        const uint val = bins[key];
        if (val > mode_size) {
            mode_size = val;
            mode = key;
        }
    }
    return mode;
}
```

#### 바뀐 이유

- 이 변경 자체는 작지만, 초기 코드는 미초기화 `mode`와 structured binding 가정이 섞여 있어 toolchain/경고 설정에 따라 불안정했다.
- M1에서는 Ubuntu 빌드에서 바로 문제가 될 수 있는 작은 portability 이슈도 같이 정리했다.

## 요약

- M1의 `src/core` 수정은 새 기능 추가가 아니라 Ubuntu 24 패키지 버전 차이를 흡수하기 위한 compatibility layer 정리다.
- Embree 4와 최신 FFmpeg를 받아들이면서도 기존 상위 API와 런타임 의미를 최대한 유지하는 방식으로 바뀌었다.
