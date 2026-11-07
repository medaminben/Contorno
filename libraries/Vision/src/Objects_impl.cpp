#include "Objects_impl.h"

#include "Ai_impl.h"
#include "Camera_impl.h"
#include "Segment_impl.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

namespace Vision {

namespace {

Result<bool> check_options(ObjectOptions const& options) {
    auto fail = [](Core::Problem problem) { return Core::error_result<bool>([&] { return std::move(problem); }); };
    if (options.model_path.empty()) {
        return fail(Core::Problem("objects.no_model", "no model path given"));
    }
    if (!(options.outline_tolerance_mm >= 0) || !std::isfinite(options.outline_tolerance_mm)) {
        return fail(Core::Problem("objects.tolerance", "the outline tolerance must be finite and not negative"));
    }
    if (!(options.threshold > 0 && options.threshold < 1)) {
        return fail(Core::Problem("objects.threshold", "the threshold must be in {range}", {{"range", "(0, 1)"}}));
    }
    if (!(options.min_area_pct >= 0 && options.min_area_pct <= 100)) {
        return fail(Core::Problem("objects.min_area", "the minimum area must be in {range}", {{"range", "[0, 100] %"}}));
    }
    if (!(options.hole_background > 0 && options.hole_background <= options.threshold)) {
        return fail(Core::Problem("objects.hole_background", "the hole background must be in {range}", {{"range", "(0, threshold]"}}));
    }
    return true;
}

/// The holes of region whose mean probability is not under background:
/// the model is unsure there, it does not see the table through them.
cv::Mat unsure_holes(cv::Mat const& region, cv::Mat const& probability, double background) {
    cv::Mat labels;
    int const n = cv::connectedComponents(enclosed_holes_impl(region), labels, 4, CV_32S);
    std::vector<double> sum(static_cast<std::size_t>(n), 0);
    std::vector<int> count(static_cast<std::size_t>(n), 0);
    for (int row = 0; row < labels.rows; ++row) {
        auto const* l = labels.ptr<int>(row);
        auto const* p = probability.ptr<float>(row);
        for (int col = 0; col < labels.cols; ++col) {
            sum[static_cast<std::size_t>(l[col])] += p[col];
            ++count[static_cast<std::size_t>(l[col])];
        }
    }
    std::vector<uint8_t> unsure(static_cast<std::size_t>(n), 0);
    for (int i = 1; i < n; ++i) {
        auto const k = static_cast<std::size_t>(i);
        unsure[k] = sum[k] / count[k] >= background ? 255 : 0;
    }
    cv::Mat out = cv::Mat::zeros(region.size(), CV_8U);
    for (int row = 0; row < labels.rows; ++row) {
        auto const* l = labels.ptr<int>(row);
        auto* o = out.ptr<uint8_t>(row);
        for (int col = 0; col < labels.cols; ++col) o[col] = unsure[static_cast<std::size_t>(l[col])];
    }
    return out;
}

/// Share of the model's region that must lie in the object found from the background ...
constexpr double background_holds_model = 0.95;
/// ... and how much larger that object must be, for the model to have missed part of the hide.
constexpr double background_min_gain = 1.10;

/// The object found from the background (background_object_impl), when the
/// model's region is only part of it: a salient-object model can take the
/// dark patches of a spotted or speckled hide for the object and the light
/// parts for the background. The object found from the background then holds
/// the model's region and is clearly larger. Empty otherwise (no object
/// found, the two differ only at the edge, or they disagree: the background
/// is not plain, the hide reaches the border), and the model's region stands.
cv::Mat hide_missed_by_model(cv::Mat const& image, cv::Mat const& region) {
    cv::Mat const object = background_object_impl(image);
    if (object.empty()) return {};
    double const model_px = cv::countNonZero(region);
    double const object_px = cv::countNonZero(object);
    double const inside_px = cv::countNonZero(region & object);
    if (model_px > 0 && object_px > background_min_gain * model_px && inside_px >= background_holds_model * model_px) {
        return object;
    }
    return {};
}

}  // namespace

Result<ObjectsAnalysis> find_objects_impl(std::string const& image_path, PixelScale scale,
                                          ObjectOptions const& options) {
    auto fail = [](Core::Problem reason) {
        return Core::error_result<ObjectsAnalysis>([&] { return objects_problem(std::move(reason)); });
    };
    if (auto checked = check_options(options); checked.isError()) {
        return fail(checked.error());
    }
    if (!(std::isfinite(scale.x) && scale.x > 0 && std::isfinite(scale.y) && scale.y > 0)) {
        return fail(camera_problem(Core::Problem("camera.scale", "the scale must be positive and finite")));
    }
    cv::Mat const image = cv::imread(image_path, cv::IMREAD_COLOR);
    if (image.empty()) {
        return fail(Core::Problem("cannot_read_image", "cannot read image '{path}'", {{"path", image_path}}));
    }

    ObjectsAnalysis out;
    out.frame = {image.cols, image.rows, scale.x, scale.y};

    auto probability = ai_probability_impl(image, options.model_path, options.threshold);
    if (probability.isError()) {
        return fail(probability.error());
    }
    cv::Mat mask = probability() > options.threshold;
    clean_mask_impl(mask);

    double const min_px = options.min_area_pct / 100.0 * image.cols * image.rows;
    cv::Mat labels;
    cv::Mat stats;
    cv::Mat centroids;
    int const n = cv::connectedComponentsWithStats(mask, labels, stats, centroids, 8, CV_32S);
    std::vector<int> kept;
    for (int i = 1; i < n; ++i) {
        if (stats.at<int>(i, cv::CC_STAT_AREA) >= min_px) kept.push_back(i);
    }
    std::stable_sort(kept.begin(), kept.end(), [&](int a, int b) {
        return stats.at<int>(a, cv::CC_STAT_AREA) > stats.at<int>(b, cv::CC_STAT_AREA);
    });
    // Only the largest is measured: the rest is dust and specks on the table.
    if (kept.size() > 1) kept.resize(1);

    cv::Mat objects = cv::Mat::zeros(image.size(), CV_8U);
    for (int label : kept) {
        cv::Mat region = labels == label;
        // Holes under the minimum area (pixel count), and holes where the
        // model is unsure, are filled in the mask; the polygon then takes
        // every hole left, with no second measure (a contour-area test), so
        // that every black hole of the mask has its ring and the mask's area
        // is the polygons' area.
        cv::Mat filled = unsure_holes(region, probability(), options.hole_background);
        if (min_px > 0) filled |= enclosed_holes_impl(region, min_px);
        if (cv::countNonZero(filled) > 0) {
            region |= filled;
            clean_mask_impl(region);
        }
        if (auto object = options.check_background ? hide_missed_by_model(image, region) : cv::Mat {};
            !object.empty()) {
            region = object;
            out.from_background = true;
            if (min_px > 0) region |= enclosed_holes_impl(region, min_px);
            clean_mask_impl(region);
        }
        auto polygon = trace_polygon_impl(region, out.frame, options.outline_tolerance_mm);
        if (polygon.isError()) {
            return fail(polygon.error());
        }
        out.objects.push_back(std::move(polygon)());
        objects |= region;
    }

    out.area_mm2 = cv::countNonZero(objects) * out.frame.mm_per_px_x * out.frame.mm_per_px_y;
    out.mask = to_mask_impl(objects);
    return out;
}

}  // namespace Vision
