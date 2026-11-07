#include "model/ModelPath.h"

#include <system_error>

namespace fs = std::filesystem;

fs::path find_model(fs::path const& configured, fs::path const& app_dir,
                    fs::path const& compiled_default, std::vector<fs::path>* looked_in) {
    std::vector<fs::path> candidates;
    if (!configured.empty())
        candidates.push_back(configured);
    candidates.push_back(
        (app_dir / ".." / "share/contorno/models/isnet-general-use.pt").lexically_normal());
    if (!compiled_default.empty())
        candidates.push_back(compiled_default);
    if (looked_in)
        *looked_in = candidates;
    for (auto const& candidate : candidates) {
        std::error_code ignored;
        if (fs::is_regular_file(candidate, ignored))
            return candidate;
    }
    return {};
}
