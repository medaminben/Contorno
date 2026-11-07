#include "Ai_impl.h"

#ifdef CONTORNO_WITH_TORCH
#include <opencv2/imgproc.hpp>

#include <torch/script.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <optional>
#include <system_error>
#include <utility>
#endif

namespace Vision {

#ifdef CONTORNO_WITH_TORCH

namespace {

constexpr int model_side = 1024;  // the TorchScript contract: [1, 3, 1024, 1024] in, [1, 1, 1024, 1024] out

}  // namespace

Result<cv::Mat> ai_probability_impl(cv::Mat const& bgr, std::string const& model_path, double threshold) {
    std::error_code ec;
    if (model_path.empty() || !std::filesystem::is_regular_file(model_path, ec)) {
        return Core::error_result<cv::Mat>([&] { return Core::Problem("ai.cannot_read_model", "cannot read model '{path}'", {{"path", model_path}}); });
    }
    if (bgr.empty() || bgr.type() != CV_8UC3) {
        return Core::error_result<cv::Mat>([] { return Core::Problem("ai.image_format", "the image must be 8-bit, 3-channel"); });
    }

    // Pre-processing: RGB, 1024 x 1024 bilinear, x / 255 - 0.5, NCHW float32.
    cv::Mat rgb;
    cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
    cv::Mat resized;
    cv::resize(rgb, resized, cv::Size(model_side, model_side), 0, 0, cv::INTER_LINEAR);
    cv::Mat input;
    resized.convertTo(input, CV_32F, 1.0 / 255.0, -0.5);

    cv::Mat probability(model_side, model_side, CV_32F);
    {
        static struct {
            std::mutex m;
            std::string path;
            std::optional<torch::jit::Module> module;
        } cache;
        std::lock_guard<std::mutex> lock(cache.m);
        if (!cache.module || cache.path != model_path) {
            cache.module.reset();
            cache.path.clear();
            torch::jit::Module loaded = torch::jit::load(model_path, torch::kCPU);
            loaded.eval();
            cache.module = std::move(loaded);
            cache.path = model_path;
        }

        c10::InferenceMode guard;
        torch::Tensor const x = torch::from_blob(input.data, {1, model_side, model_side, 3}, torch::kFloat32)
                                    .permute({0, 3, 1, 2})
                                    .contiguous();
        torch::Tensor y = cache.module->forward({x}).toTensor().to(torch::kFloat32).contiguous();
        if (y.sizes() != c10::IntArrayRef({1, 1, model_side, model_side})) {
            return Core::error_result<cv::Mat>([] {
                return Core::Problem("ai.bad_shape", "the model's output is not {shape}", {{"shape", "1 x 1 x 1024 x 1024"}});
            });
        }
        std::memcpy(probability.data, y.data_ptr<float>(), sizeof(float) * model_side * model_side);
    }
    if (!cv::checkRange(probability)) {
        return Core::error_result<cv::Mat>([] { return Core::Problem("ai.not_finite", "the model's output is not finite"); });
    }

    // Post-processing: min-max normalise (unless nothing reaches the
    // threshold), back to the image size.
    double lo = 0;
    double hi = 0;
    cv::minMaxLoc(probability, &lo, &hi);
    if (hi >= threshold && hi > lo) {
        probability = (probability - lo) / (hi - lo);
    }
    cv::Mat full;
    cv::resize(probability, full, bgr.size(), 0, 0, cv::INTER_LINEAR);
    return full;
}

#else

Result<cv::Mat> ai_probability_impl(cv::Mat const&, std::string const&, double) {
    return Core::error_result<cv::Mat>([] { return Core::Problem("ai.no_libtorch", "Vision was built without LibTorch"); });
}

#endif

Result<cv::Mat> ai_mask_impl(cv::Mat const& bgr, std::string const& model_path, double threshold) {
    auto probability = ai_probability_impl(bgr, model_path, threshold);
    if (probability.isError()) return probability;
    cv::Mat mask = probability() > threshold;  // CV_8U 0/255
    return mask;
}

}  // namespace Vision
