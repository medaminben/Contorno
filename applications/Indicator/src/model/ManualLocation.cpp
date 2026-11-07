#include "model/ManualLocation.h"

#include "model/Utf8Path.h"

#include <algorithm>
#include <string_view>
#include <system_error>

namespace fs = std::filesystem;

namespace {

/// `file://` URL of a local path with the path percent-encoded as UTF-8.
std::string file_url(fs::path const& path) {
    std::string const text = utf8(path);
    std::string url = "file://";
    if (text.empty() || text.front() != '/') url += '/';  // C:/x becomes file:///C:/x
    constexpr char const* hex = "0123456789ABCDEF";
    for (unsigned char const c : text) {
        bool const plain = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                           std::string_view("-._~/!$&'()*+,;=:@").find(static_cast<char>(c)) != std::string_view::npos;
        if (plain) {
            url += static_cast<char>(c);
        } else {
            url += '%';
            url += hex[c >> 4];
            url += hex[c & 15];
        }
    }
    return url;
}

}  // namespace

fs::path find_manual(std::string const& language, fs::path const& app_dir, fs::path const& source_manual) {
    std::vector<fs::path> roots {(app_dir / ".." / "share/contorno/manual").lexically_normal()};
    if (!source_manual.empty()) roots.push_back(source_manual);
    std::vector<fs::path> candidates;
    for (std::string const& code : {language, std::string("en")}) {
        for (fs::path const& root : roots) candidates.push_back(root / code / "index.html");
    }
    for (fs::path const& root : roots) candidates.push_back(root / "index.html");
    for (fs::path const& candidate : candidates) {
        std::error_code ignored;
        if (fs::is_regular_file(candidate, ignored)) return candidate;
    }
    return {};
}

std::vector<std::string> manual_browser_candidates(bool windows, std::string const& program_files,
                                                   std::string const& program_files_x86) {
    if (!windows) return {"chromium", "chromium-browser", "google-chrome", "google-chrome-stable"};
    constexpr char const* edge = "Microsoft/Edge/Application/msedge.exe";
    return {utf8(path_from_utf8(program_files) / edge), utf8(path_from_utf8(program_files_x86) / edge),
            "msedge"};
}

Rect manual_window_rect(Rect const& available) {
    // a reading size on a normal desktop: the manual's two columns start at 820 px
    constexpr int width = 1200;
    constexpr int height = 800;
    // --window-size is the content of an --app window; its title bar is drawn outside
    constexpr int margin = 32;
    if (!available.valid()) return {};
    if (available.width <= 2 * margin || available.height <= 2 * margin) return available;
    int const w = std::min(width, available.width - 2 * margin);
    int const h = std::min(height, available.height - 2 * margin);
    return {available.x + (available.width - w) / 2, available.y + (available.height - h) / 2, w, h};
}

ManualBrowserCommand manual_browser_launch(std::vector<std::string> const& browsers, fs::path const& index_path,
                                           fs::path const& profile_dir, Rect const& window) {
    if (browsers.empty()) return {};
    ManualBrowserCommand command;
    command.program = browsers.front();
    command.arguments = {"--user-data-dir=" + utf8(profile_dir), "--app=" + file_url(index_path),
                         "--no-first-run", "--disable-extensions"};
    if (window.valid()) {
        command.arguments.push_back("--window-position=" + std::to_string(window.x) + "," + std::to_string(window.y));
        command.arguments.push_back("--window-size=" + std::to_string(window.width) + "," + std::to_string(window.height));
    }
    return command;
}
