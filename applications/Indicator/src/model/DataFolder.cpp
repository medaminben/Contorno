#include "model/DataFolder.h"

#include "model/Utf8Path.h"

#include <fstream>
#include <iterator>
#include <system_error>

namespace fs = std::filesystem;

namespace {

fs::path installed_share(fs::path const& app_dir) {
    return (app_dir / ".." / "share/contorno").lexically_normal();
}

/// The first line of `path` without a byte order mark or trailing white space; empty when
/// unreadable.
std::string recorded_folder(fs::path const& path) {
    std::ifstream file(path, std::ios::binary);
    std::string text(std::istreambuf_iterator<char>(file), {});
    if (text.starts_with("\xEF\xBB\xBF"))
        text.erase(0, 3);
    text = text.substr(0, text.find_first_of("\r\n"));
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t'))
        text.pop_back();
    return text;
}

}  // namespace

fs::path data_folder(fs::path const& app_dir, fs::path const& documents) {
    std::string const recorded = recorded_folder(installed_share(app_dir) / "data-folder");
    if (!recorded.empty())
        return path_from_utf8(recorded);
    return documents / "Contorno";
}

bool fill_data_folder(fs::path const& app_dir, fs::path const& data, std::string& reason) {
    fs::path const share = installed_share(app_dir);
    for (char const* name : {"samples", "manual", "articles"}) {
        std::error_code error;
        fs::path const from = share / name;
        fs::path const to = data / name;
        if (!fs::is_directory(from, error) || fs::exists(to, error))
            continue;
        // copied under another name and renamed when complete: an interrupted copy is not
        // taken for the user's folder on the next start
        fs::path const partial = data / (std::string(".") + name + ".partial");
        fs::remove_all(partial, error);
        fs::create_directories(data, error);
        if (!error)
            fs::copy(from, partial, fs::copy_options::recursive, error);
        if (!error)
            fs::rename(partial, to, error);
        if (error) {
            std::error_code ignored;
            fs::remove_all(partial, ignored);
            reason = utf8(to) + ": " + error.message();
            return false;
        }
    }
    return true;
}
