#include "JpegEncoder.hpp"

#include <core/system/Config.hpp>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#if defined(SIBR_MGSTREAM_TURBOJPEG_AVAILABLE)
#include <turbojpeg.h>
#endif

namespace sibr {

struct JpegEncoder::Impl {
#if defined(SIBR_MGSTREAM_TURBOJPEG_AVAILABLE)
    tjhandle turbo_handle = nullptr;
#endif
};

JpegEncoder::JpegEncoder(int quality)
    : quality_(std::clamp(quality, 1, 100)), impl_(std::make_unique<Impl>())
{
#if defined(SIBR_MGSTREAM_TURBOJPEG_AVAILABLE)
    impl_->turbo_handle = tjInitCompress();
#endif
}

JpegEncoder::~JpegEncoder()
{
#if defined(SIBR_MGSTREAM_TURBOJPEG_AVAILABLE)
    if (impl_ && impl_->turbo_handle != nullptr) {
        tjDestroy(impl_->turbo_handle);
        impl_->turbo_handle = nullptr;
    }
#endif
}

std::string JpegEncoder::backendName() const
{
#if defined(SIBR_MGSTREAM_TURBOJPEG_AVAILABLE)
    if (impl_ && impl_->turbo_handle != nullptr) {
        return "TurboJPEG";
    }
#endif
    return "OpenCV";
}

bool JpegEncoder::encodeRgb(
    const uint8_t* rgb_bytes,
    int width,
    int height,
    std::vector<uint8_t>& jpeg_bytes,
    std::string& error)
{
    error.clear();
    jpeg_bytes.clear();

    if (rgb_bytes == nullptr || width <= 0 || height <= 0) {
        error = "encodeRgb requires a non-empty RGB image.";
        return false;
    }

#if defined(SIBR_MGSTREAM_TURBOJPEG_AVAILABLE)
    if (impl_ && impl_->turbo_handle != nullptr) {
        unsigned char* encoded_buffer = nullptr;
        unsigned long encoded_size = 0;
        const int rc = tjCompress2(
            impl_->turbo_handle,
            const_cast<unsigned char*>(rgb_bytes),
            width,
            width * 3,
            height,
            TJPF_RGB,
            &encoded_buffer,
            &encoded_size,
            TJSAMP_420,
            quality_,
            TJFLAG_FASTDCT);
        if (rc == 0 && encoded_buffer != nullptr && encoded_size > 0) {
            jpeg_bytes.assign(encoded_buffer, encoded_buffer + encoded_size);
            tjFree(encoded_buffer);
            return true;
        }
        error = std::string("TurboJPEG encode failed: ") + tjGetErrorStr();
        if (encoded_buffer != nullptr) {
            tjFree(encoded_buffer);
        }
    }
#endif

    cv::Mat rgb(height, width, CV_8UC3, const_cast<uint8_t*>(rgb_bytes));
    cv::Mat bgr;
    cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);

    const std::vector<int> params = {
        cv::IMWRITE_JPEG_QUALITY, quality_
    };
    if (!cv::imencode(".jpg", bgr, jpeg_bytes, params)) {
        if (error.empty()) {
            error = "OpenCV JPEG encode failed.";
        }
        return false;
    }

    return true;
}

} // namespace sibr
