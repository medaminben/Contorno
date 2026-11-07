#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <regex>
#include <string>

namespace test_Layers {

// The model layer is plain C++ over the libraries: no Qt and no wxWidgets header,
// so it builds and is tested without a display.
TEST(WxLayers, ModelHasNoToolkit) {
    std::regex const toolkit_include(R"(#include\s*[<"](Q|wx/))");
    std::filesystem::path const model_dir = INDICATOR_SOURCE_DIR "/model";
    ASSERT_TRUE(std::filesystem::is_directory(model_dir)) << model_dir.string();
    int files_scanned = 0;
    for (auto const& entry : std::filesystem::recursive_directory_iterator(model_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        ++files_scanned;
        std::ifstream file(entry.path());
        ASSERT_TRUE(file.is_open()) << entry.path().string();
        std::string line;
        int number = 0;
        while (std::getline(file, line)) {
            ++number;
            EXPECT_FALSE(std::regex_search(line, toolkit_include))
                << entry.path().string() << ":" << number << ": " << line;
        }
    }
    EXPECT_GT(files_scanned, 0) << model_dir.string();
}

}  // namespace test_Layers
