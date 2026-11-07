#include "model/ManualLocation.h"
#include "model/Utf8Path.h"

#include "TempDir.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

void touch(fs::path const& path) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    ASSERT_TRUE(file.is_open());
}

}  // namespace

TEST(WxManualLocation, InstalledCopyWins) {
    TempDir dir;
    fs::path const app = dir.path / "bin";
    fs::path const source = dir.path / "docs/manual";
    touch(dir.path / "share/contorno/manual/fr/index.html");
    touch(source / "fr/index.html");
    EXPECT_EQ(find_manual("fr", app, source), (dir.path / "share/contorno/manual/fr/index.html").lexically_normal());
}

TEST(WxManualLocation, SourceTreeWhenNotInstalled) {
    TempDir dir;
    fs::path const source = dir.path / "docs/manual";
    touch(source / "ar/index.html");
    EXPECT_EQ(find_manual("ar", dir.path / "bin", source), source / "ar/index.html");
}

TEST(WxManualLocation, FallsBackToEnglishThenTheChooser) {
    TempDir dir;
    fs::path const source = dir.path / "docs/manual";
    touch(source / "en/index.html");
    EXPECT_EQ(find_manual("nl", dir.path / "bin", source), source / "en/index.html");
    TempDir other;
    touch(other.path / "manual/index.html");
    EXPECT_EQ(find_manual("nl", other.path / "bin", other.path / "manual"), other.path / "manual/index.html");
}

TEST(WxManualLocation, EmptyWhenThereIsNoManual) {
    TempDir dir;
    EXPECT_TRUE(find_manual("en", dir.path / "bin", dir.path / "docs/manual").empty());
    EXPECT_TRUE(find_manual("en", dir.path / "bin", "").empty());
}

TEST(WxManualLocation, LinuxBrowsersInOrder) {
    auto const browsers = manual_browser_candidates(false, "C:/Program Files", "C:/Program Files (x86)");
    EXPECT_EQ(browsers, (std::vector<std::string> {"chromium", "chromium-browser", "google-chrome", "google-chrome-stable"}));
}

TEST(WxManualLocation, WindowsBrowsersAreEdgeThenTheBareName) {
    auto const browsers = manual_browser_candidates(true, "C:/Program Files", "C:/Program Files (x86)");
    ASSERT_EQ(browsers.size(), 3u);
    EXPECT_EQ(browsers[0], "C:/Program Files/Microsoft/Edge/Application/msedge.exe");
    EXPECT_EQ(browsers[1], "C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe");
    EXPECT_EQ(browsers[2], "msedge");
}

TEST(WxManualLocation, WindowIsCentredAndClearOfTheEdges) {
    EXPECT_EQ(manual_window_rect({0, 0, 1920, 1080}), (Rect {360, 140, 1200, 800}));
    EXPECT_EQ(manual_window_rect({0, 0, 1280, 720}), (Rect {40, 32, 1200, 656}));
    EXPECT_EQ(manual_window_rect({100, 50, 60, 60}), (Rect {100, 50, 60, 60}));
    EXPECT_FALSE(manual_window_rect({}).valid());
}

TEST(WxManualLocation, LaunchOpensTheFileInAnAppWindow) {
    auto const none = manual_browser_launch({}, "/m/en/index.html", "/p");
    EXPECT_TRUE(none.program.empty());
    auto const command = manual_browser_launch({"chromium", "google-chrome"}, "/m/a b/index.html", "/p", Rect {10, 20, 300, 400});
    EXPECT_EQ(command.program, "chromium");
    EXPECT_EQ(command.arguments,
              (std::vector<std::string> {"--user-data-dir=/p", "--app=file:///m/a%20b/index.html", "--no-first-run",
                                         "--disable-extensions", "--window-position=10,20", "--window-size=300,400"}));
    EXPECT_EQ(manual_browser_launch({"chromium"}, "/m/index.html", "/p").arguments.size(), 4u);
}

TEST(WxManualLocation, LaunchEncodesNonAsciiAndReservedCharacters) {
    auto const command = manual_browser_launch({"chromium"}, path_from_utf8("/m/\xC3\xBC \xC3\xA4/\xD8\xB9#?%.html"), "/p");
    ASSERT_EQ(command.arguments.size(), 4u);
    EXPECT_EQ(command.arguments[1], "--app=file:///m/%C3%BC%20%C3%A4/%D8%B9%23%3F%25.html");
}
