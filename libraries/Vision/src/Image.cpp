#include <Vision/Image.h>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace Vision {

namespace {

/// The bytes as an OpenCV buffer (imdecode needs a Mat; it does not keep it).
cv::Mat buffer_of(std::string_view bytes) {
    return cv::Mat(1, static_cast<int>(bytes.size()), CV_8U,
                   const_cast<void*>(static_cast<void const*>(bytes.data())));
}

}  // namespace

std::optional<ImageSize> shown_image_size(std::string_view bytes) noexcept {
    try {
        if (bytes.empty()) return std::nullopt;
        cv::Mat const image = cv::imdecode(buffer_of(bytes), cv::IMREAD_COLOR);  // the orientation applied
        if (image.empty()) return std::nullopt;
        return ImageSize {image.cols, image.rows};
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::string> encode_mask_png(Mask const& mask) noexcept {
    try {
        if (mask.width <= 0 || mask.height <= 0
            || mask.data.size() != static_cast<std::size_t>(mask.width) * static_cast<std::size_t>(mask.height)) {
            return std::nullopt;
        }
        cv::Mat const image(mask.height, mask.width, CV_8U, const_cast<std::uint8_t*>(mask.data.data()));
        std::vector<std::uint8_t> png;
        if (!cv::imencode(".png", image, png)) return std::nullopt;
        return std::string(png.begin(), png.end());
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<Mask> decode_mask_png(std::string_view bytes) noexcept {
    try {
        static constexpr char signature[] = "\x89PNG\r\n\x1A\n";
        if (bytes.size() < 8 || bytes.substr(0, 8) != std::string_view(signature, 8)) return std::nullopt;
        cv::Mat const grey = cv::imdecode(buffer_of(bytes), cv::IMREAD_GRAYSCALE);
        if (grey.empty() || grey.depth() != CV_8U) return std::nullopt;
        Mask mask {grey.cols, grey.rows, std::vector<std::uint8_t>(grey.total())};
        for (int row = 0; row < grey.rows; ++row) {
            std::memcpy(mask.data.data() + static_cast<std::size_t>(row) * static_cast<std::size_t>(grey.cols),
                        grey.ptr<std::uint8_t>(row), static_cast<std::size_t>(grey.cols));
        }
        return mask;
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace Vision
