#include <Vision/Lens.h>
#include <Vision/Objects.h>

#include "Geometry.h"

// Include 3rd party headers
#include <gtest/gtest.h>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>

namespace test_Objects {

namespace {

/// The TorchScript IS-Net of the tests (empty when it is not built).
std::string model() { return CONTORNO_ISNET_MODEL; }

/// A sample photo of docs/samples.
std::string sample(std::string const& rel) { return std::string(CONTORNO_SAMPLES_DIR "/") + rel; }

/// The scale of the default lens camera.
Vision::PixelScale default_scale() { return Vision::lens_scale(Vision::LensCamera {}); }

bool have_model() {
    std::error_code ec;
    return Vision::ai_available() && !model().empty() && std::filesystem::is_regular_file(model(), ec);
}

#define REQUIRE_MODEL()                                         \
    do {                                                        \
        if (!have_model()) GTEST_SKIP() << "no IS-Net model";   \
    } while (false)

Vision::ObjectOptions options_with_model() {
    Vision::ObjectOptions options;
    options.model_path = model();
    return options;
}

/// Asserts that no vertex of the outer ring lies within 1 mm of the image
/// border, nor on a border pixel (whose centre is half a pixel from it).
void expect_clear_of_border(Pot::Path64 const& ring, Vision::PixelFrame const& frame) {
    double const s = frame.mm_per_px_x;
    double const margin = std::max(1.0, s);
    double const w = frame.width * s;
    double const h = frame.height * s;
    for (auto const& p : ring) {
        double const x = p.x / Pot::units_per_mm;
        double const y = p.y / Pot::units_per_mm;
        double const d = std::min({x, y, w - x, h - y});
        EXPECT_GT(d, margin) << "vertex " << x << ", " << y << " mm is " << d << " mm from the border";
    }
}

/// A temporary file name unique to this test process.
std::string temp_file(std::string const& name) {
    static std::string const token = std::to_string(std::random_device {}());
    return (std::filesystem::temp_directory_path() / ("contorno_objects_" + token + "_" + name)).string();
}

}  // namespace

TEST(Objects, AiAvailableMatchesBuild) {
#ifdef CONTORNO_TEST_WITH_TORCH
    EXPECT_TRUE(Vision::ai_available());
#else
    EXPECT_FALSE(Vision::ai_available());
#endif
}

TEST(Objects, ScaleIsUsedAsGiven) {
    REQUIRE_MODEL();
    auto const r = Vision::find_objects(sample("leather/calf_black.jpg"), Vision::PixelScale {0.5, 0.7},
                                        options_with_model());
    ASSERT_FALSE(r.isError());
    EXPECT_EQ(r().frame.mm_per_px_x, 0.5);
    EXPECT_EQ(r().frame.mm_per_px_y, 0.7);
    auto const white = std::count(r().mask.data.begin(), r().mask.data.end(), 255);
    EXPECT_DOUBLE_EQ(r().area_mm2, white * 0.5 * 0.7);
}

// runs without LibTorch: the scale is checked before the model is read
TEST(Objects, InvalidScaleIsAnError) {
    for (auto const s : {Vision::PixelScale {0, 1}, Vision::PixelScale {1, -1}, Vision::PixelScale {std::nan(""), 1},
                         Vision::PixelScale {1, INFINITY}}) {
        auto const r = Vision::find_objects(sample("leather/calf_black.jpg"), s, {"any.pt"});
        ASSERT_TRUE(r.isError());
        EXPECT_NE(r.error().text().find("scale"), std::string::npos);
    }
}

TEST(Objects, EmptyModelPathIsAnError) {
    Vision::ObjectOptions options;
    options.model_path = "";
    auto r = Vision::find_objects(sample("leather/calf_black.jpg"), default_scale(), options);
    ASSERT_TRUE(r.isError());
    EXPECT_EQ(r.error().text().rfind("objects: ", 0), 0u) << r.error();
}

TEST(Objects, HoleBackgroundOutOfRangeIsAnError) {
    for (double const background : {0.0, 0.6}) {  // the threshold is 0.5
        Vision::ObjectOptions options = options_with_model();
        options.hole_background = background;
        auto r = Vision::find_objects(sample("leather/calf_black.jpg"), default_scale(), options);
        ASSERT_TRUE(r.isError()) << background;
        EXPECT_NE(r.error().text().find("hole background"), std::string::npos) << r.error();
    }
}

TEST(Objects, MissingModelFileIsAnError) {
    Vision::ObjectOptions options;
    options.model_path = "/nonexistent.pt";
    auto r = Vision::find_objects(sample("leather/calf_black.jpg"), default_scale(), options);
    ASSERT_TRUE(r.isError());
    EXPECT_EQ(r.error().text().rfind("objects: ", 0), 0u) << r.error();
    if (Vision::ai_available()) {
        EXPECT_NE(r.error().text().find("cannot read model"), std::string::npos) << r.error();
    }
}

TEST(Objects, WithoutTorchIsAnError) {
    if (Vision::ai_available()) GTEST_SKIP() << "Vision was built with LibTorch";
    Vision::ObjectOptions options;
    options.model_path = model().empty() ? std::string("/nonexistent.pt") : model();
    auto r = Vision::find_objects(sample("leather/calf_black.jpg"), default_scale(), options);
    ASSERT_TRUE(r.isError());
    EXPECT_NE(r.error().text().find("built without LibTorch"), std::string::npos) << r.error();
}

TEST(Objects, CorruptModelIsAnError) {
    if (!Vision::ai_available()) GTEST_SKIP() << "Vision was built without LibTorch";
    auto const path = temp_file("corrupt.pt");
    {
        std::ofstream out(path, std::ios::binary);
        out << "not a model";
    }
    Vision::ObjectOptions options;
    options.model_path = path;
    auto r = Vision::find_objects(sample("leather/calf_black.jpg"), default_scale(), options);
    std::filesystem::remove(path);
    ASSERT_TRUE(r.isError());
    EXPECT_EQ(r.error().text().rfind("objects: ", 0), 0u) << r.error();
}

TEST(Objects, CalfHasItsHole) {
    REQUIRE_MODEL();
    // Warm-up: loads the model into the cache, so that only inference is timed.
    Vision::ObjectOptions options = options_with_model();
    options.min_area_pct = 0.01;  // the hole is 631 of 1334 x 1602 pixels
    auto warm = Vision::find_objects(sample("leather/calf_black.jpg"), default_scale(), options);
    ASSERT_FALSE(warm.isError()) << warm.error();
    auto const start = std::chrono::steady_clock::now();
    auto r = Vision::find_objects(sample("leather/calf_black.jpg"), default_scale(), options);
    auto const seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    ASSERT_FALSE(r.isError()) << r.error();
    auto const& a = r();
    // Recorded, not bounded: a wall-clock limit is flaky when ctest runs tests in parallel.
    RecordProperty("cached_seconds", std::to_string(seconds));
    std::cout << "calf cached inference: " << seconds << " s\n";
    ASSERT_EQ(a.objects.size(), 1u);
    EXPECT_EQ(a.objects[0].holes.size(), 1u);
    EXPECT_EQ(a.mask.width, 1334);
    EXPECT_EQ(a.mask.height, 1602);
    EXPECT_GT(a.area_mm2, 0.0);
    EXPECT_FALSE(a.from_background) << "the model's region stands";
    geom::expect_oriented(a.objects[0]);
    expect_clear_of_border(a.objects[0].outer, a.frame);
}

TEST(Objects, WholeSkinDoesNotReachBorder) {
    REQUIRE_MODEL();
    auto r = Vision::find_objects(sample("leather/chekiang_lamb.jpg"), default_scale(), options_with_model());
    ASSERT_FALSE(r.isError()) << r.error();
    auto const& a = r();
    ASSERT_EQ(a.objects.size(), 1u);
    geom::expect_oriented(a.objects[0]);
    expect_clear_of_border(a.objects[0].outer, a.frame);
    double const outline_mm2 =
        std::abs(geom::signed_area(a.objects[0].outer)) / (Pot::units_per_mm * Pot::units_per_mm);
    double const s = a.frame.mm_per_px_x;
    double const image_mm2 = a.frame.width * s * a.frame.height * s;
    EXPECT_GT(outline_mm2, 0.30 * image_mm2);
    EXPECT_LT(outline_mm2, 0.90 * image_mm2);
}

TEST(Objects, OnlyTheLargestIsKept) {
    REQUIRE_MODEL();
    // a drawn photo: a large and a small brown ellipse on light grey
    cv::Mat photo(1200, 1600, CV_8UC3, cv::Scalar(200, 200, 200));
    cv::ellipse(photo, cv::Point(500, 600), cv::Size(300, 200), 0, 0, 360, cv::Scalar(60, 90, 140), cv::FILLED);
    cv::ellipse(photo, cv::Point(1250, 600), cv::Size(150, 100), 0, 0, 360, cv::Scalar(60, 90, 140), cv::FILLED);
    auto const path = temp_file("two_ellipses.png");
    ASSERT_TRUE(cv::imwrite(path, photo));
    auto options = options_with_model();
    options.min_area_pct = 0;  // no size filter: the small ellipse must go by size alone
    auto r = Vision::find_objects(path, default_scale(), options);
    std::filesystem::remove(path);
    ASSERT_FALSE(r.isError()) << r.error();
    auto const& a = r();
    ASSERT_EQ(a.objects.size(), 1u);
    // the object's bounding box in pixels holds the large ellipse's centre and not the small one's
    double x0 = 1e18, y0 = 1e18, x1 = -1e18, y1 = -1e18;
    for (auto const& p : a.objects[0].outer) {
        auto const px = Vision::to_px(a.frame, {p.x / Pot::units_per_mm, p.y / Pot::units_per_mm});
        double const x = px.x;
        double const y = px.y;
        x0 = std::min(x0, x);
        y0 = std::min(y0, y);
        x1 = std::max(x1, x);
        y1 = std::max(y1, y);
    }
    EXPECT_TRUE(x0 <= 500 && 500 <= x1 && y0 <= 600 && 600 <= y1) << x0 << ", " << y0 << " to " << x1 << ", " << y1;
    EXPECT_FALSE(x0 <= 1250 && 1250 <= x1 && y0 <= 600 && 600 <= y1) << x0 << ", " << y0 << " to " << x1 << ", " << y1;
}

TEST(Objects, EmptyTableHasNoObjects) {
    REQUIRE_MODEL();
    auto const path = temp_file("empty_table.png");
    ASSERT_TRUE(cv::imwrite(path, cv::Mat(600, 800, CV_8UC3, cv::Scalar(128, 128, 128))));
    auto r = Vision::find_objects(path, default_scale(), options_with_model());
    std::filesystem::remove(path);
    ASSERT_FALSE(r.isError()) << r.error();
    auto const& a = r();
    EXPECT_TRUE(a.objects.empty());
    EXPECT_EQ(a.area_mm2, 0.0);
    EXPECT_EQ(a.mask.width, 800);
    EXPECT_EQ(a.mask.height, 600);
    EXPECT_TRUE(std::all_of(a.mask.data.begin(), a.mask.data.end(), [](uint8_t v) { return v == 0; }));
}

TEST(Objects, GreyImageWorks) {
    REQUIRE_MODEL();
    cv::Mat const colour = cv::imread(sample("leather/calf_black.jpg"), cv::IMREAD_COLOR);
    ASSERT_FALSE(colour.empty());
    cv::Mat grey;
    cv::cvtColor(colour, grey, cv::COLOR_BGR2GRAY);
    auto const path = temp_file("calf_grey.png");
    ASSERT_TRUE(cv::imwrite(path, grey));
    auto r = Vision::find_objects(path, default_scale(), options_with_model());
    std::filesystem::remove(path);
    ASSERT_FALSE(r.isError()) << r.error();
    EXPECT_EQ(r().objects.size(), 1u);
}

namespace {

/// Pixel counts of the mask's holes: its 4-connected background regions clear of the border.
std::vector<int> mask_holes(Vision::Mask const& m) {
    cv::Mat const mask(m.height, m.width, CV_8U, const_cast<uint8_t*>(m.data.data()));
    cv::Mat labels, stats, centroids;
    int const n = cv::connectedComponentsWithStats(~mask, labels, stats, centroids, 4, CV_32S);
    std::vector<int> out;
    for (int i = 1; i < n; ++i) {
        int const x = stats.at<int>(i, cv::CC_STAT_LEFT), y = stats.at<int>(i, cv::CC_STAT_TOP);
        int const w = stats.at<int>(i, cv::CC_STAT_WIDTH), h = stats.at<int>(i, cv::CC_STAT_HEIGHT);
        if (x > 0 && y > 0 && x + w < mask.cols && y + h < mask.rows) out.push_back(stats.at<int>(i, cv::CC_STAT_AREA));
    }
    return out;
}

}  // namespace

// every black hole in the mask has its ring in the outlines, and no ring
// lacks its hole, also for a hole right at the minimum area
TEST(Objects, EveryMaskHoleHasARing) {
    REQUIRE_MODEL();
    auto run = [](char const* photo, double pct) {
        Vision::ObjectOptions options = options_with_model();
        options.min_area_pct = pct;
        auto r = Vision::find_objects(sample(photo), default_scale(), options);
        EXPECT_FALSE(r.isError()) << r.error();
        if (r.isError()) return std::vector<int> {};
        std::size_t rings = 0;
        for (auto const& o : r().objects) rings += o.holes.size();
        auto const holes = mask_holes(r().mask);
        EXPECT_EQ(rings, holes.size()) << photo << " at " << pct << " %";
        return holes;
    };
    auto const all = run("leather/calf_black.jpg", 0);
    ASSERT_EQ(all.size(), 1u);  // the hole in the skin
    double const image_px = 1334.0 * 1602;
    for (int const px : all) {
        run("leather/calf_black.jpg", 100.0 * px / image_px);        // about exactly the minimum
        run("leather/calf_black.jpg", 100.0 * (px + 1) / image_px);  // one pixel under it: filled
    }
    run("leather/cowhide_stretched.jpg", 0.1);
    run("leather/cowhide_stretched.jpg", 1);
}

// the outline drawn on a hide (a processed image opened as a photo) makes
// the model unsure in a dark patch: that is not a hole, the area stays the
// clean photo's
TEST(Objects, MarkedHideHasNoFalseHole) {
    REQUIRE_MODEL();
    auto clean = Vision::find_objects(sample("leather/cowhide_stretched.jpg"), default_scale(), options_with_model());
    auto marked = Vision::find_objects(sample("leather/cowhide_marked.png"), default_scale(), options_with_model());
    ASSERT_FALSE(clean.isError()) << clean.error();
    ASSERT_FALSE(marked.isError()) << marked.error();
    ASSERT_EQ(marked().objects.size(), 1u);
    double const image_px = 1600.0 * 1200;
    for (int const px : mask_holes(marked().mask)) EXPECT_LT(px, 0.01 * image_px);
    auto white = [](Vision::Mask const& m) { return std::count(m.data.begin(), m.data.end(), 255); };
    EXPECT_NEAR(static_cast<double>(white(marked().mask)), static_cast<double>(white(clean().mask)),
                0.03 * static_cast<double>(white(clean().mask)));
}

// a real hole, where the table shows through, is kept
TEST(Objects, CalfKeepsItsHole) {
    REQUIRE_MODEL();
    Vision::ObjectOptions options = options_with_model();
    options.min_area_pct = 0.01;  // the hole is about 630 of 1334 x 1602 pixels
    auto r = Vision::find_objects(sample("leather/calf_black.jpg"), default_scale(), options);
    ASSERT_FALSE(r.isError()) << r.error();
    auto const& m = r().mask;
    ASSERT_EQ(m.width, 1334);
    EXPECT_EQ(m.data[static_cast<std::size_t>(1414 * m.width + 916)], 0);  // the hole's centre
    std::size_t rings = 0;
    for (auto const& o : r().objects) rings += o.holes.size();
    EXPECT_GE(rings, 1u);
}

// a black and white calf filling most of a dark carpet: the model takes one
// black spot for the object; the hide, black patches at the photo's edge
// included, stands out from the plain carpet, so it is taken instead
TEST(Objects, SpottedCalfIsTheWholeHide) {
    REQUIRE_MODEL();
    auto r = Vision::find_objects(sample("leather/calf_spotted.jpg"), default_scale(), options_with_model());
    ASSERT_FALSE(r.isError()) << r.error();
    ASSERT_EQ(r().objects.size(), 1u);
    auto const& m = r().mask;
    ASSERT_EQ(m.width, 1531);
    ASSERT_EQ(m.height, 1414);
    auto at = [&](int x, int y) { return m.data[static_cast<std::size_t>(y * m.width + x)]; };
    EXPECT_EQ(at(500, 700), 255) << "white fur";
    EXPECT_EQ(at(800, 450), 255) << "black spot";
    EXPECT_EQ(at(150, 650), 255) << "black patch at the left edge";
    EXPECT_EQ(at(1440, 500), 255) << "black patch at the right edge";
    EXPECT_EQ(at(380, 270), 255) << "black spot top left";
    EXPECT_EQ(at(40, 40), 0) << "carpet";
    EXPECT_EQ(at(700, 1380), 0) << "carpet";
    EXPECT_EQ(at(1500, 1390), 0) << "carpet";
    double const share = static_cast<double>(std::count(m.data.begin(), m.data.end(), 255)) / (1531.0 * 1414);
    EXPECT_GT(share, 0.70);
    EXPECT_LT(share, 0.90);
    EXPECT_TRUE(r().from_background) << "the object found from the background replaced the model's region";
}

// without the background check the model's region stands: on the spotted
// calf, the one black spot it takes for the object
TEST(Objects, BackgroundCheckCanBeTurnedOff) {
    REQUIRE_MODEL();
    auto options = options_with_model();
    options.check_background = false;
    auto r = Vision::find_objects(sample("leather/calf_spotted.jpg"), default_scale(), options);
    ASSERT_FALSE(r.isError()) << r.error();
    auto const& m = r().mask;
    double const share = static_cast<double>(std::count(m.data.begin(), m.data.end(), 255)) / (1.0 * m.width * m.height);
    EXPECT_LT(share, 0.20);
    EXPECT_FALSE(r().from_background);
}

namespace {

/// Relative area error, in %, of find_objects on a synthetic photo against its true mask.
double synthetic_area_error_pct(std::string const& stem) {
    auto r = Vision::find_objects(sample("synthetic/" + stem + ".jpg"), default_scale(), options_with_model());
    EXPECT_FALSE(r.isError()) << r.error();
    if (r.isError()) return 100;
    cv::Mat const truth = cv::imread(sample("synthetic/" + stem + "_mask.png"), cv::IMREAD_GRAYSCALE);
    EXPECT_FALSE(truth.empty());
    double const true_px = cv::countNonZero(truth > 127);
    auto const& m = r().mask;
    double const found_px = static_cast<double>(std::count(m.data.begin(), m.data.end(), 255));
    return 100 * (found_px - true_px) / true_px;
}

}  // namespace

// a speckled hide: the model takes the light parts for the background and
// misses 60 % of the hide; the background found from the border holds the
// model's region and the whole hide, so it is taken
TEST(Objects, SpeckledHideIsWhole) {
    REQUIRE_MODEL();
    EXPECT_NEAR(synthetic_area_error_pct("speckled_kraft"), 0, 1);
}

// a three-colour hide the model outlines well (+0.2 %): the outline found
// from the background is only a few % larger (edge, shadow), so the model's stands
TEST(Objects, SmallDifferenceKeepsTheModelOutline) {
    REQUIRE_MODEL();
    EXPECT_NEAR(synthetic_area_error_pct("tricolour_kraft"), 0, 0.5);
}

TEST(Objects, RepeatedCallsAgree) {
    REQUIRE_MODEL();
    auto first = Vision::find_objects(sample("leather/calf_black.jpg"), default_scale(), options_with_model());
    auto second = Vision::find_objects(sample("leather/calf_black.jpg"), default_scale(), options_with_model());
    ASSERT_FALSE(first.isError()) << first.error();
    ASSERT_FALSE(second.isError()) << second.error();
    EXPECT_EQ(first().mask.data, second().mask.data);
}

}  // namespace test_Objects
