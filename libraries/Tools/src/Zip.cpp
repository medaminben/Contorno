#include "Zip_impl.h"

#include <Tools/Zip.h>

#include <exception>
#include <fstream>
#include <iterator>
#include <utility>

namespace Tools {

namespace {

Core::Problem path_problem(char const* code, std::string_view english, std::string const& path) {
    return Core::Problem(code, english, {{"path", path}});
}

/// "<path>: <reason>".
Core::Problem path_failed(std::string const& path, Core::Problem reason) {
    return Core::Problem("path_failed", "{path}: {reason}", {{"path", path}, {"reason", std::move(reason)}});
}

}  // namespace

Result<std::string> to_zip(std::vector<ZipEntry> const& entries) noexcept { return to_zip_impl(entries); }

Result<> write_zip(std::string const& path, std::vector<ZipEntry> const& entries) noexcept {
    auto bytes = to_zip_impl(entries);
    if (bytes.isError()) return Result<>(Core::Error<>(std::move(bytes).error()));
    try {
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            return Result<>(Core::Error<>(path_problem("zip.cannot_open_for_writing", "cannot open {path} for writing", path)));
        }
        file << bytes();
        file.close();
        if (!file) return Result<>(Core::Error<>(path_problem("zip.cannot_write", "cannot write {path}", path)));
        return Result<>(std::monostate {});
    } catch (std::exception const& e) {
        return Core::error_result<std::monostate>([&] { return path_failed(path, Core::Problem::raw(e.what())); });
    }
}

Result<std::vector<ZipEntry>> parse_zip(std::string_view bytes) noexcept { return parse_zip_impl(bytes); }

Result<std::vector<ZipEntry>> read_zip(std::string const& path) noexcept {
    using Entries = std::vector<ZipEntry>;
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return Core::error_result<Entries>([&] { return path_problem("zip.cannot_open", "cannot open {path}", path); });
        }
        std::string const bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (file.bad()) {
            return Core::error_result<Entries>([&] { return path_problem("zip.cannot_read", "cannot read {path}", path); });
        }
        auto entries = parse_zip_impl(bytes);
        if (entries.isError()) {
            return Core::error_result<Entries>([&] { return path_failed(path, entries.error()); });
        }
        return entries;
    } catch (std::exception const& e) {
        return Core::error_result<Entries>([&] { return path_failed(path, Core::Problem::raw(e.what())); });
    }
}

}  // namespace Tools
