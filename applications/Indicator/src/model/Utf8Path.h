#ifndef INDICATOR_UTF8PATH_H
#define INDICATOR_UTF8PATH_H
#include <filesystem>
#include <string>

/// The path as UTF-8 with `/` separators, on every platform. `string()` would
/// be the ANSI code page on Windows.
inline std::string utf8(std::filesystem::path const& path) {
    auto const text = path.generic_u8string();
    return std::string(text.begin(), text.end());
}

/// The path for UTF-8 `text`, on every platform.
inline std::filesystem::path path_from_utf8(std::string const& text) {
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

#endif  // INDICATOR_UTF8PATH_H
