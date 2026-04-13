#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sibr {

class JpegEncoder {
public:
    explicit JpegEncoder(int quality = 85);
    ~JpegEncoder();

    JpegEncoder(const JpegEncoder&) = delete;
    JpegEncoder& operator=(const JpegEncoder&) = delete;

    std::string backendName() const;
    bool encodeRgb(const uint8_t* rgb_bytes, int width, int height, std::vector<uint8_t>& jpeg_bytes, std::string& error);

private:
    struct Impl;

    int quality_ = 85;
    std::unique_ptr<Impl> impl_;
};

} // namespace sibr
