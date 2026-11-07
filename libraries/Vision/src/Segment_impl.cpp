#include "Segment_impl.h"

#include "Camera_impl.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <string>
#include <utility>
#include <vector>

namespace Vision {

namespace {

/// Calls f(row, col) for each pixel of a band along the image border,
/// band_pct % of the image's smaller side wide (at least one pixel).
template <class F>
void for_each_border_pixel(cv::Size size, double band_pct, F&& f) {
    int const band =
        std::max(1, static_cast<int>(std::lround(band_pct / 100.0 * std::min(size.width, size.height))));
    for (int row = 0; row < size.height; ++row) {
        bool const full = row < band || row >= size.height - band;
        for (int col = 0; col < size.width; ++col) {
            if (!full && col == band) {
                col = std::max(col, size.width - band);  // skip the interior of the row
            }
            f(row, col);
        }
    }
}

bool touches_border(cv::Mat const& stats, int label, cv::Size size) {
    int const x = stats.at<int>(label, cv::CC_STAT_LEFT);
    int const y = stats.at<int>(label, cv::CC_STAT_TOP);
    int const w = stats.at<int>(label, cv::CC_STAT_WIDTH);
    int const h = stats.at<int>(label, cv::CC_STAT_HEIGHT);
    return x == 0 || y == 0 || x + w == size.width || y + h == size.height;
}

}  // namespace

cv::Mat enclosed_holes_impl(cv::Mat const& region, double max_area_px) {
    // Components of the complement (4-connected, dual of the region's
    // 8-connectivity) that do not reach the image border.
    cv::Mat outside_labels;
    cv::Mat stats;
    cv::Mat centroids;
    int const m = cv::connectedComponentsWithStats(~region, outside_labels, stats, centroids, 4, CV_32S);
    std::vector<uint8_t> enclosed(static_cast<std::size_t>(m), 0);
    for (int i = 1; i < m; ++i) {
        bool const small = stats.at<int>(i, cv::CC_STAT_AREA) < max_area_px;
        enclosed[static_cast<std::size_t>(i)] = small && !touches_border(stats, i, region.size()) ? 255 : 0;
    }
    cv::Mat holes = cv::Mat::zeros(region.size(), CV_8U);
    for (int row = 0; row < region.rows; ++row) {
        auto const* l = outside_labels.ptr<int>(row);
        auto* h = holes.ptr<uint8_t>(row);
        for (int col = 0; col < region.cols; ++col) {
            h[col] = enclosed[static_cast<std::size_t>(l[col])];
        }
    }
    return holes;
}

std::pair<cv::Mat, int> largest_region_impl(cv::Mat const& mask) {
    cv::Mat labels;
    cv::Mat stats;
    cv::Mat centroids;
    int const n = cv::connectedComponentsWithStats(mask, labels, stats, centroids, 8, CV_32S);
    int best = 0;
    int best_area = 0;
    for (int i = 1; i < n; ++i) {
        int const a = stats.at<int>(i, cv::CC_STAT_AREA);
        if (a > best_area) {
            best_area = a;
            best = i;
        }
    }
    if (best == 0) {
        return {cv::Mat::zeros(mask.size(), CV_8U), 0};
    }
    return {labels == best, best_area};
}

namespace {

/// Smallest epsilon (pixels) tried before 0 when halving.
constexpr double min_epsilon_px = 0.125;

double next_epsilon(double epsilon) {
    return epsilon / 2 >= min_epsilon_px ? epsilon / 2 : 0.0;
}

/// A ring on the grid in mm and the pixel vertices it was made from.
struct Ring {
    Pot::Path64 mm;
    std::vector<cv::Point> px;
};

/// The contour simplified within epsilon pixels, counter-clockwise in mm on
/// the grid; empty when fewer than three vertices or no area are left.
Ring simplified_ring(std::vector<cv::Point> const& contour, PixelFrame const& frame, double epsilon) {
    std::vector<cv::Point> approx;
    cv::approxPolyDP(contour, approx, epsilon, true);
    Ring ring;
    for (auto const& p : approx) {
        if (ring.px.empty() || ring.px.back() != p) {
            ring.px.push_back(p);
        }
    }
    while (ring.px.size() > 1 && ring.px.front() == ring.px.back()) {
        ring.px.pop_back();
    }
    if (ring.px.size() < 3) {
        return {};
    }
    double const signed_px = cv::contourArea(ring.px, true);
    if (signed_px == 0) {
        return {};
    }
    if (signed_px > 0) {  // clockwise in mm (rows run down)
        std::reverse(ring.px.begin(), ring.px.end());
    }
    ring.mm.reserve(ring.px.size());
    for (auto const& p : ring.px) {
        auto const q = to_mm_impl(frame, p.x, p.y);
        ring.mm.push_back({std::llround(q.x * Pot::units_per_mm), std::llround(q.y * Pot::units_per_mm)});
    }
    return ring;
}

/// The contour as a simple counter-clockwise ring, simplified within at most
/// epsilon pixels (halved down to 0 until the ring is simple); empty when none is.
Ring simple_ring(std::vector<cv::Point> const& contour, PixelFrame const& frame, double epsilon) {
    for (;;) {
        auto ring = simplified_ring(contour, frame, epsilon);
        if (!ring.mm.empty() && simple_polygon_impl({ring.mm, {}}, {ring.px})) {
            return ring;
        }
        if (epsilon <= 0) {
            return {};
        }
        epsilon = next_epsilon(epsilon);
    }
}

int sign(std::int64_t v) {
    return (v > 0) - (v < 0);
}

/// Sign of the cross product (b - a) x (c - a).
int orientation(Pot::Point64 const& a, Pot::Point64 const& b, Pot::Point64 const& c) {
    return sign((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x));
}

/// c, collinear with a and b, lies on the closed segment ab.
bool on_segment(Pot::Point64 const& a, Pot::Point64 const& b, Pot::Point64 const& c) {
    return std::min(a.x, b.x) <= c.x && c.x <= std::max(a.x, b.x) && std::min(a.y, b.y) <= c.y &&
           c.y <= std::max(a.y, b.y);
}

/// The closed segments ab and cd share a point.
bool segments_meet(Pot::Point64 const& a, Pot::Point64 const& b, Pot::Point64 const& c, Pot::Point64 const& d) {
    int const o1 = orientation(a, b, c);
    int const o2 = orientation(a, b, d);
    int const o3 = orientation(c, d, a);
    int const o4 = orientation(c, d, b);
    if (o1 != o2 && o3 != o4 && o1 != 0 && o2 != 0 && o3 != 0 && o4 != 0) {
        return true;
    }
    return (o1 == 0 && on_segment(a, b, c)) || (o2 == 0 && on_segment(a, b, d)) ||
           (o3 == 0 && on_segment(c, d, a)) || (o4 == 0 && on_segment(c, d, b));
}

}  // namespace

bool simple_polygon_impl(Pot::Polygon64 const& polygon, std::vector<std::vector<cv::Point>> const& pixel_rings) {
    if (pixel_rings.size() != 1 + polygon.holes.size()) {
        return false;
    }
    std::vector<Pot::Path64 const*> rings {&polygon.outer};
    for (auto const& h : polygon.holes) {
        rings.push_back(&h);
    }
    struct Edge {
        std::size_t ring;
        std::size_t index;  // from vertex index to index + 1
        std::int64_t min_x;
        std::int64_t max_x;
    };
    std::vector<Edge> edges;
    for (std::size_t r = 0; r < rings.size(); ++r) {
        auto const& ring = *rings[r];
        std::size_t const n = ring.size();
        if (n < 3) {
            return false;
        }
        std::int64_t twice_area = 0;
        for (std::size_t i = 0; i < n; ++i) {
            auto const& a = ring[i];
            auto const& b = ring[(i + 1) % n];
            if (a.x == b.x && a.y == b.y) {
                return false;
            }
            twice_area += a.x * b.y - b.x * a.y;
            edges.push_back({r, i, std::min(a.x, b.x), std::max(a.x, b.x)});
        }
        if (twice_area == 0) {
            return false;
        }
    }
    std::sort(edges.begin(), edges.end(), [](Edge const& x, Edge const& y) { return x.min_x < y.min_x; });
    auto point = [&](std::size_t ring, std::size_t index) -> Pot::Point64 const& {
        auto const& path = *rings[ring];
        return path[index % path.size()];
    };
    for (std::size_t i = 0; i < edges.size(); ++i) {
        auto const& e = edges[i];
        auto const& a = point(e.ring, e.index);
        auto const& b = point(e.ring, e.index + 1);
        for (std::size_t j = i + 1; j < edges.size() && edges[j].min_x <= e.max_x; ++j) {
            auto const& f = edges[j];
            auto const& c = point(f.ring, f.index);
            auto const& d = point(f.ring, f.index + 1);
            if (std::max(std::min(a.y, b.y), std::min(c.y, d.y)) > std::min(std::max(a.y, b.y), std::max(c.y, d.y))) {
                continue;  // y extents apart
            }
            if (e.ring == f.ring) {
                std::size_t const n = rings[e.ring]->size();
                bool const f_follows = (e.index + 1) % n == f.index;
                bool const e_follows = (f.index + 1) % n == e.index;
                if (f_follows || e_follows) {
                    // Adjacent edges meet at their common vertex; they must not
                    // fold back over each other. (In a triangle both orders
                    // hold; a fold there leaves no area, rejected above.)
                    auto const& p = f_follows ? a : c;  // start of the first edge
                    auto const& q = f_follows ? b : d;  // the common vertex
                    auto const& t = f_follows ? d : b;  // end of the second edge
                    if (orientation(p, q, t) == 0 && (p.x - q.x) * (t.x - q.x) + (p.y - q.y) * (t.y - q.y) > 0) {
                        return false;
                    }
                    continue;
                }
            }
            if (segments_meet(a, b, c, d)) {
                return false;
            }
        }
    }
    // No ring meets another, so one vertex tells on which side a ring lies:
    // tested on the pixel rings the grid rings were made from.
    for (std::size_t h = 1; h < pixel_rings.size(); ++h) {
        cv::Point2f const v(static_cast<float>(pixel_rings[h].front().x), static_cast<float>(pixel_rings[h].front().y));
        if (cv::pointPolygonTest(pixel_rings[0], v, false) <= 0) {
            return false;
        }
        for (std::size_t k = 1; k < pixel_rings.size(); ++k) {
            if (k != h && cv::pointPolygonTest(pixel_rings[k], v, false) >= 0) {
                return false;
            }
        }
    }
    return true;
}

Pot::Path64 contour_to_path_impl(std::vector<cv::Point> const& contour, PixelFrame const& frame,
                                 double tolerance_mm) {
    double const epsilon = tolerance_mm > 0 ? px_for_mm_impl(frame, tolerance_mm) : 0.0;
    return simple_ring(contour, frame, epsilon).mm;
}

void clean_mask_impl(cv::Mat& mask) {
    int const rows = mask.rows;
    int const cols = mask.cols;
    if (rows < 2 || cols < 2) {
        mask.setTo(0);
        return;
    }
    cv::Mat keep(rows, cols, CV_8U);
    for (bool changed = true; changed;) {
        changed = false;
        // Pixels in a 2x2 block of set pixels stay.
        keep.setTo(0);
        for (int r = 0; r + 1 < rows; ++r) {
            auto const* m0 = mask.ptr<uint8_t>(r);
            auto const* m1 = mask.ptr<uint8_t>(r + 1);
            auto* k0 = keep.ptr<uint8_t>(r);
            auto* k1 = keep.ptr<uint8_t>(r + 1);
            for (int c = 0; c + 1 < cols; ++c) {
                if (m0[c] != 0 && m0[c + 1] != 0 && m1[c] != 0 && m1[c + 1] != 0) {
                    k0[c] = k0[c + 1] = k1[c] = k1[c + 1] = 255;
                }
            }
        }
        for (int r = 0; r < rows; ++r) {
            auto* m = mask.ptr<uint8_t>(r);
            auto const* k = keep.ptr<uint8_t>(r);
            for (int c = 0; c < cols; ++c) {
                if (m[c] != 0 && k[c] == 0) {
                    m[c] = 0;
                    changed = true;
                }
            }
        }
        // Diagonal-only 2x2 blocks: the upper pixel of the pair is cleared.
        for (int r = 0; r + 1 < rows; ++r) {
            auto* m0 = mask.ptr<uint8_t>(r);
            auto const* m1 = mask.ptr<uint8_t>(r + 1);
            for (int c = 0; c + 1 < cols; ++c) {
                bool const a = m0[c] != 0;
                bool const b = m0[c + 1] != 0;
                bool const d = m1[c] != 0;
                bool const e = m1[c + 1] != 0;
                if (a && e && !b && !d) {
                    m0[c] = 0;
                    changed = true;
                } else if (b && d && !a && !e) {
                    m0[c + 1] = 0;
                    changed = true;
                }
            }
        }
    }
}

Result<Pot::Polygon64> trace_polygon_impl(cv::Mat const& region, PixelFrame const& frame, double tolerance_mm,
                                          double min_hole_area_px) {
    auto fail = [](Core::Problem problem) {
        return Core::error_result<Pot::Polygon64>([&] { return std::move(problem); });
    };
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(region, contours, hierarchy, cv::RETR_CCOMP, cv::CHAIN_APPROX_NONE);
    int outer = -1;
    double outer_area = -1;
    for (std::size_t i = 0; i < contours.size(); ++i) {
        if (hierarchy[i][3] < 0) {
            double const a = cv::contourArea(contours[i]);
            if (a > outer_area) {
                outer_area = a;
                outer = static_cast<int>(i);
            }
        }
    }
    if (outer < 0) {
        return fail(Core::Problem("segment.nothing_to_trace", "nothing to trace"));
    }
    std::vector<std::size_t> holes;
    for (std::size_t i = 0; i < contours.size(); ++i) {
        if (hierarchy[i][3] == outer && cv::contourArea(contours[i]) >= min_hole_area_px) {
            holes.push_back(i);
        }
    }
    double epsilon = tolerance_mm > 0 ? px_for_mm_impl(frame, tolerance_mm) : 0.0;
    for (;;) {
        Pot::Polygon64 polygon;
        std::vector<std::vector<cv::Point>> pixel_rings;
        auto outline = simple_ring(contours[static_cast<std::size_t>(outer)], frame, epsilon);
        bool complete = !outline.mm.empty();
        polygon.outer = std::move(outline.mm);
        pixel_rings.push_back(std::move(outline.px));
        for (std::size_t k = 0; k < holes.size() && complete; ++k) {
            auto ring = simple_ring(contours[holes[k]], frame, epsilon);
            complete = !ring.mm.empty();
            std::reverse(ring.mm.begin(), ring.mm.end());
            polygon.holes.push_back(std::move(ring.mm));
            pixel_rings.push_back(std::move(ring.px));
        }
        if (complete && simple_polygon_impl(polygon, pixel_rings)) {
            return polygon;
        }
        if (epsilon <= 0) {
            return fail(Core::Problem("segment.simple_outline", "cannot trace a simple outline"));
        }
        epsilon = next_epsilon(epsilon);
    }
}

Mask to_mask_impl(cv::Mat const& mask) {
    Mask out {mask.cols, mask.rows, {}};
    out.data.resize(mask.total());
    for (int row = 0; row < mask.rows; ++row) {
        auto const* p = mask.ptr<uint8_t>(row);
        std::copy(p, p + mask.cols, out.data.begin() + static_cast<std::ptrdiff_t>(row) * mask.cols);
    }
    return out;
}

namespace {

/// Median of values (reordered); the mean of the two middle ones for an even count.
double median_of(std::vector<double>& values) {
    auto const mid = values.begin() + static_cast<std::ptrdiff_t>(values.size() / 2);
    std::nth_element(values.begin(), mid, values.end());
    double const upper = *mid;
    if (values.size() % 2 != 0) {
        return upper;
    }
    return 0.5 * (upper + *std::max_element(values.begin(), mid));
}

/// Terms of the background surface at (u, v), the position from the image
/// centre over its size: quadratic, and the fourth power of the radius (vignetting).
std::array<double, 7> surface_terms(double u, double v) {
    double const r2 = u * u + v * v;
    return {1, u, v, u * u, u * v, v * v, r2 * r2};
}

constexpr int feature_count = 3;
using Features = cv::Vec<float, feature_count>;

/// Per pixel of an 8-bit BGR image: log lightness (CIELAB L), and the green
/// and red shares of the linear RGB sum, x 100.
cv::Mat features_of(cv::Mat const& bgr) {
    cv::Mat scaled;
    bgr.convertTo(scaled, CV_32F, 1.0 / 255.0);
    cv::Mat lab;
    cv::cvtColor(scaled, lab, cv::COLOR_BGR2Lab);
    cv::Mat out(bgr.size(), CV_32FC(feature_count));
    auto linear = [](float c) { return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); };
    for (int row = 0; row < bgr.rows; ++row) {
        auto const* s = scaled.ptr<cv::Vec3f>(row);
        auto const* l = lab.ptr<cv::Vec3f>(row);
        auto* o = out.ptr<Features>(row);
        for (int col = 0; col < bgr.cols; ++col) {
            float const b = linear(s[col][0]);
            float const g = linear(s[col][1]);
            float const r = linear(s[col][2]);
            float const sum = b + g + r + 1e-4f;
            o[col] = Features(std::log(std::max(l[col][0], 0.5f)), 100 * g / sum, 100 * r / sum);
        }
    }
    return out;
}

/// The background surface of each feature, fitted to the border band, and its robust sigma.
struct Surface {
    std::array<cv::Mat, feature_count> coef;  ///< 7 x 1 CV_64F each
    std::array<double, feature_count> sigma {};
};

Surface fit_background(cv::Mat const& features, double band_pct) {
    std::vector<cv::Point> band;
    for_each_border_pixel(features.size(), band_pct, [&](int row, int col) { band.emplace_back(col, row); });
    std::size_t const n = band.size();
    double const w = features.cols;
    double const h = features.rows;
    cv::Mat terms(static_cast<int>(n), 7, CV_64F);
    for (std::size_t i = 0; i < n; ++i) {
        auto const t = surface_terms(band[i].x / w - 0.5, band[i].y / h - 0.5);
        std::copy(t.begin(), t.end(), terms.ptr<double>(static_cast<int>(i)));
    }
    // Least squares, refitted without the pixels off the surface (the object where it reaches the border).
    constexpr int fit_passes = 5;
    constexpr double outlier_sigmas = 3.0;
    Surface out;
    for (std::size_t c = 0; c < out.coef.size(); ++c) {
        std::vector<double> y(n);
        for (std::size_t i = 0; i < n; ++i) y[i] = features.at<Features>(band[i])[static_cast<int>(c)];
        std::vector<bool> keep(n, true);
        for (int pass = 0; pass < fit_passes; ++pass) {
            auto const rows = static_cast<int>(std::count(keep.begin(), keep.end(), true));
            cv::Mat a(rows, 7, CV_64F);
            cv::Mat b(rows, 1, CV_64F);
            int j = 0;
            for (std::size_t i = 0; i < n; ++i) {
                if (keep[i]) {
                    terms.row(static_cast<int>(i)).copyTo(a.row(j));
                    b.at<double>(j++) = y[i];
                }
            }
            cv::solve(a, b, out.coef[c], cv::DECOMP_SVD);
            cv::Mat const fitted = terms * out.coef[c];
            std::vector<double> residual(n);
            std::vector<double> kept;
            for (std::size_t i = 0; i < n; ++i) {
                residual[i] = y[i] - fitted.at<double>(static_cast<int>(i));
                if (keep[i]) kept.push_back(residual[i]);
            }
            double const centre = median_of(kept);
            for (double& r : kept) r = std::abs(r - centre);
            out.sigma[c] = 1.4826 * median_of(kept);
            double const cut = outlier_sigmas * std::max(out.sigma[c], 1e-6);
            for (std::size_t i = 0; i < n; ++i) keep[i] = std::abs(residual[i]) <= cut;
        }
    }
    return out;
}

/// Per pixel: how unlike the background it is, in robust sigmas; a shadow (darker up to max_shadow) is forgiven.
cv::Mat deviation_from(cv::Mat const& features, Surface const& surface, double max_shadow) {
    // A background without noise would make any change unlike it: sigmas of at least these.
    constexpr std::array<double, feature_count> min_sigma {0.03, 0.4, 0.4};
    std::array<double, feature_count> sigma {};
    for (std::size_t c = 0; c < sigma.size(); ++c) sigma[c] = std::max(surface.sigma[c], min_sigma[c]);
    double const shadow = std::log(max_shadow);
    double const w = features.cols;
    double const h = features.rows;
    cv::Mat out(features.size(), CV_32F);
    for (int row = 0; row < features.rows; ++row) {
        auto const* f = features.ptr<Features>(row);
        auto* o = out.ptr<float>(row);
        for (int col = 0; col < features.cols; ++col) {
            auto const t = surface_terms(col / w - 0.5, row / h - 0.5);
            std::array<double, feature_count> d {};
            for (std::size_t c = 0; c < d.size(); ++c) {
                double expected = 0;
                for (std::size_t j = 0; j < t.size(); ++j) expected += t[j] * surface.coef[c].at<double>(static_cast<int>(j));
                d[c] = f[col][static_cast<int>(c)] - expected;
            }
            double lightness = d[0] / sigma[0];
            if (d[0] < 0 && d[0] > -shadow) lightness = 0;
            else if (d[0] <= -shadow) lightness = (d[0] + shadow) / sigma[0];
            o[col] = static_cast<float>(
                std::sqrt(lightness * lightness + std::pow(d[1] / sigma[1], 2) + std::pow(d[2] / sigma[2], 2)));
        }
    }
    return out;
}

/// The coarse object: what the background-like areas reaching the border do not cover (CV_8U 0/255, may be empty).
cv::Mat coarse_object(cv::Mat const& deviation, double max_deviation) {
    cv::Mat const background_like = deviation <= max_deviation;
    cv::Mat labels;
    cv::connectedComponents(background_like, labels, 4, CV_32S);
    auto const cols = static_cast<std::size_t>(labels.cols);
    std::vector<uint8_t> reaches_border(static_cast<std::size_t>(*std::max_element(labels.begin<int>(), labels.end<int>())) + 1, 0);
    for_each_border_pixel(labels.size(), 0, [&](int row, int col) { reaches_border[static_cast<std::size_t>(labels.at<int>(row, col))] = 1; });
    reaches_border[0] = 0;
    cv::Mat object(labels.size(), CV_8U);
    for (int row = 0; row < labels.rows; ++row) {
        auto const* l = labels.ptr<int>(row);
        auto* o = object.ptr<uint8_t>(row);
        for (std::size_t col = 0; col < cols; ++col) {
            o[col] = reaches_border[static_cast<std::size_t>(l[col])] ? 0 : 255;
        }
    }
    cv::morphologyEx(object, object, cv::MORPH_OPEN, cv::Mat::ones(3, 3, CV_8U));
    object = largest_region_impl(object).first;
    // Enclosed areas: holes where the background shows through (background-like, of a few pixels), else filled.
    constexpr int min_hole_px = 4;
    cv::Mat stats;
    cv::Mat centroids;
    auto const holes = static_cast<std::size_t>(
        cv::connectedComponentsWithStats(enclosed_holes_impl(object), labels, stats, centroids, 4, CV_32S));
    std::vector<double> sum(holes, 0);
    for (int row = 0; row < labels.rows; ++row) {
        auto const* l = labels.ptr<int>(row);
        auto const* d = deviation.ptr<float>(row);
        for (std::size_t col = 0; col < cols; ++col) sum[static_cast<std::size_t>(l[col])] += d[col];
    }
    std::vector<uint8_t> fill(holes, 0);
    for (std::size_t i = 1; i < holes; ++i) {
        int const area = stats.at<int>(static_cast<int>(i), cv::CC_STAT_AREA);
        fill[i] = area < min_hole_px || sum[i] / area > max_deviation ? 255 : 0;
    }
    for (int row = 0; row < labels.rows; ++row) {
        auto const* l = labels.ptr<int>(row);
        auto* o = object.ptr<uint8_t>(row);
        for (std::size_t col = 0; col < cols; ++col) o[col] |= fill[static_cast<std::size_t>(l[col])];
    }
    return object;
}

}  // namespace

cv::Mat background_object_impl(cv::Mat const& bgr, BackgroundOptions const& options) {
    double const scale = static_cast<double>(options.coarse_side_px) / std::max(bgr.cols, bgr.rows);
    cv::Mat small;
    cv::resize(bgr, small, cv::Size(), scale, scale, cv::INTER_AREA);
    cv::medianBlur(small, small, options.coarse_median_px);
    cv::Mat const features = features_of(small);
    cv::Mat const deviation = deviation_from(features, fit_background(features, options.border_band_pct), options.max_shadow);
    cv::Mat const coarse = coarse_object(deviation, options.max_deviation);
    if (cv::countNonZero(coarse) == 0) return {};

    // Fine: the edge in a band around the coarse outline, where the colour changes most sharply.
    cv::Mat object;
    cv::resize(coarse, object, bgr.size(), 0, 0, cv::INTER_NEAREST);
    int const r = std::max(3, static_cast<int>(std::lround(options.fine_band_coarse_px / scale)));
    cv::Mat const kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(2 * r + 1, 2 * r + 1));
    cv::Mat outer;
    cv::Mat inner;
    cv::dilate(object, outer, kernel);
    cv::erode(object, inner, kernel);
    if (cv::countNonZero(inner) == 0) return {};
    constexpr int background_marker = 1;
    constexpr int object_marker = 2;
    cv::Mat markers = cv::Mat::zeros(bgr.size(), CV_32S);
    markers.setTo(background_marker, outer == 0);
    markers.setTo(object_marker, inner);
    cv::Mat smooth;
    cv::medianBlur(bgr, smooth, options.fine_median_px);
    cv::watershed(smooth, markers);
    cv::Mat found = markers == object_marker;
    cv::morphologyEx(found, found, cv::MORPH_CLOSE, cv::Mat::ones(3, 3, CV_8U));  // the watershed lines back in
    auto [region, area] = largest_region_impl(found);
    if (area == 0) return {};
    return region;
}

}  // namespace Vision

