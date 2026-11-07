#ifndef INDICATOR_TEST_LEGACYINSPECTION_H
#define INDICATOR_TEST_LEGACYINSPECTION_H
#include <Tools/Zip.h>
#include <Vision/Camera.h>

#include <charconv>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

/// The entries of an inspection zip as the hide Indicator wrote them:
/// original.jpg (the bytes of `photo`), processed.png, camera.json v1,
/// contorno-hide-parameters, hide.dxf and contorno-hide-result. Empty when
/// `photo` cannot be read.
inline std::vector<Tools::ZipEntry> legacy_inspection_entries(std::filesystem::path const& photo,
                                                              Vision::CameraConfig const& camera) {
    std::ifstream file(photo, std::ios::binary);
    if (!file) return {};
    std::string const bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    auto const number = [](double v) {  // 17 significant digits, whatever the locale
        char buffer[40];
        auto const end = std::to_chars(buffer, buffer + sizeof buffer, v, std::chars_format::general, 17).ptr;
        return std::string(buffer, end);
    };
    std::string const camera_json = "{\"format\": \"contorno-camera\", \"version\": 1, \"distance_mm\": " +
                                    number(camera.distance_mm) + ", \"fov_deg\": " + number(camera.fov_deg) +
                                    ", \"image_width_px\": " + std::to_string(camera.image_width_px) + "}\n";
    std::string const parameters_json =
        "{\"format\": \"contorno-hide-parameters\", \"version\": 1, \"hide_contour\": {\"outline_tolerance_mm\": 1, "
        "\"smoothing_pct\": 1, \"min_contrast\": 0.15, \"lightness_weight\": 0.3, \"border_band_pct\": 4, "
        "\"refine_passes\": 2, \"background\": false}, \"defect_detection\": {\"enabled\": true, "
        "\"sensitivity\": 2.5, \"min_defect_area_mm2\": 100, \"defect_margin_mm\": 10, \"noise_blur_px\": 2, "
        "\"neighbourhood_pct\": 6, \"edge_band_pct\": 2}}\n";
    std::string const result_json =
        "{\"format\": \"contorno-hide-result\", \"version\": 1, \"image\": {\"width\": 584, \"height\": 350, "
        "\"mm_per_px\": 1}, \"usable_area_mm2\": 50, \"defect_count\": 0, \"defect_area_mm2\": 0, "
        "\"hide_outline\": [[0, 0], [10, 0], [10, 10]], \"sheet\": [[[0, 0], [10, 0], [10, 10]]], \"defects\": []}\n";
    return {
        {"original.jpg", bytes},
        {"processed.png", std::string("not read")},
        {"camera.json", camera_json},
        {"parameters.json", parameters_json},
        {"hide.dxf", std::string("not read")},
        {"result.json", result_json},
    };
}

#endif  // INDICATOR_TEST_LEGACYINSPECTION_H
