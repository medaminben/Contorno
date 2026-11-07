#ifndef INDICATOR_MANUALLOCATION_H
#define INDICATOR_MANUALLOCATION_H
#include <filesystem>
#include <string>
#include <vector>

/// A rectangle in screen pixels. Valid when it has a positive width and height.
struct Rect {
    int x {0}, y {0}, width {0}, height {0};
    bool valid() const { return width > 0 && height > 0; }
    bool operator==(Rect const&) const = default;
};

/// The manual page to open for `language`: the first that exists of
/// `<app_dir>/../share/contorno/manual/<language>/index.html`,
/// `<source_manual>/<language>/index.html`, the same two for English, then
/// the language chooser `index.html` of each. An empty `source_manual` is
/// skipped. Empty when none exists.
std::filesystem::path find_manual(std::string const& language, std::filesystem::path const& app_dir,
                                  std::filesystem::path const& source_manual);

struct ManualBrowserCommand {
    std::string program;
    std::vector<std::string> arguments;
};

/// Ordered browsers for one platform. `windows` false is the Linux list and
/// ignores the two directories. `windows` true is Edge under each Program Files
/// directory, then the bare name `msedge`. Existence is not checked here.
std::vector<std::string> manual_browser_candidates(bool windows, std::string const& program_files,
                                                   std::string const& program_files_x86);

/// Where the manual window sits inside `available`, the usable rectangle of
/// the screen the application is on: a 1200×800 reading size, centred,
/// shrunk on a smaller screen so the window stays 32 px clear of each edge
/// (the browser's title bar is drawn outside `--window-size`). An area too
/// small for the margin returns `available`; an invalid one an empty Rect.
Rect manual_window_rect(Rect const& available);

/// The first of `browsers` and the arguments that open `index_path` as an
/// app window in the private profile `profile_dir`. An empty list gives an
/// empty program. A valid `window` adds `--window-position` and `--window-size`.
ManualBrowserCommand manual_browser_launch(std::vector<std::string> const& browsers,
                                           std::filesystem::path const& index_path,
                                           std::filesystem::path const& profile_dir,
                                           Rect const& window = {});

#endif  // INDICATOR_MANUALLOCATION_H
