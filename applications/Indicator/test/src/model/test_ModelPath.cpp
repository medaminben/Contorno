#include "model/ModelPath.h"

#include "TempDir.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

namespace {

void touch(fs::path const& path) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    ASSERT_TRUE(file.is_open());
    file << "x";
}

}  // namespace

TEST(WxModelPath, ConfiguredWins) {
    TempDir dir;
    fs::path const configured = dir.path / "mine.pt";
    fs::path const app = dir.path / "bin";
    fs::path const compiled = dir.path / "compiled.pt";
    touch(configured);
    touch(dir.path / "share/contorno/models/isnet-general-use.pt");
    touch(compiled);
    EXPECT_EQ(find_model(configured, app, compiled), configured);
}

TEST(WxModelPath, InstalledBeforeCompiledDefault) {
    TempDir dir;
    fs::path const app = dir.path / "bin";
    fs::path const installed = dir.path / "share/contorno/models/isnet-general-use.pt";
    fs::path const compiled = dir.path / "compiled.pt";
    touch(installed);
    touch(compiled);
    // the configured file does not exist
    EXPECT_EQ(find_model(dir.path / "missing.pt", app, compiled), installed.lexically_normal());
}

TEST(WxModelPath, NothingFoundListsEveryCandidate) {
    TempDir dir;
    std::vector<fs::path> looked;
    fs::path const result =
        find_model(dir.path / "a.pt", dir.path / "bin", dir.path / "c.pt", &looked);
    EXPECT_TRUE(result.empty());
    EXPECT_EQ(looked.size(), 3u);
}
