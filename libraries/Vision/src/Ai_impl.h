#ifndef VISION_AI_IMPL_H
#define VISION_AI_IMPL_H
// Private: the IS-Net segmentation run through LibTorch. This header carries
// no torch type; Ai_impl.cpp is the only file including torch.
#include <Core/Core.h>

#include <opencv2/core.hpp>

#include <string>

namespace Vision {

/**
 * @brief Probability of the objects on a photo, from a TorchScript IS-Net
 *
 * The image is converted to RGB, resized to 1024x1024 (bilinear), scaled to
 * x / 255 - 0.5 and run through the model (eval and inference mode). The
 * output is min-max normalised to 0..1, unless its raw maximum is below
 * threshold (an empty table: noise is not stretched into objects), and
 * resized back to the image size (bilinear). The loaded module is cached for
 * the last model path; a mutex serialises loading and inference.
 *
 * @param bgr 8-bit, 3-channel image
 * @return CV_32F of bgr.size(); error "Vision was built without LibTorch"
 *         without torch, "cannot read model '<path>'" when the file is
 *         missing. Torch errors (c10::Error) are thrown.
 */
Result<cv::Mat> ai_probability_impl(cv::Mat const& bgr, std::string const& model_path, double threshold);

/// ai_probability_impl thresholded: CV_8U, 255 where the probability is above threshold.
Result<cv::Mat> ai_mask_impl(cv::Mat const& bgr, std::string const& model_path, double threshold);

}  // namespace Vision

#endif  // VISION_AI_IMPL_H
