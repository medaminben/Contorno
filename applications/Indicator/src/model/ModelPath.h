#ifndef INDICATOR_MODELPATH_H
#define INDICATOR_MODELPATH_H
#include <filesystem>
#include <vector>

/// The IS-Net model file: the configured path, then
/// `<app_dir>/../share/contorno/models/isnet-general-use.pt`, then the path
/// compiled in. Empty candidates are skipped. Returns the first existing
/// file, else an empty path; `looked_in` receives every candidate tried.
std::filesystem::path find_model(std::filesystem::path const& configured, std::filesystem::path const& app_dir,
                                 std::filesystem::path const& compiled_default,
                                 std::vector<std::filesystem::path>* looked_in = nullptr);

#endif  // INDICATOR_MODELPATH_H
