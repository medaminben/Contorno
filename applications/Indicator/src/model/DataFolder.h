#ifndef INDICATOR_DATAFOLDER_H
#define INDICATOR_DATAFOLDER_H
#include <filesystem>
#include <string>

/// The user's folder for the samples, the manual and the articles: the one the
/// Windows installer recorded in `<app_dir>/../share/contorno/data-folder`
/// (UTF-8, one line), else `<documents>/Contorno`.
std::filesystem::path data_folder(std::filesystem::path const& app_dir,
                                  std::filesystem::path const& documents);

/// Copies `samples`, `manual` and `articles` from `<app_dir>/../share/contorno`
/// into `data`, each one only when `data` does not have it yet, so the user's
/// own changes stay. Nothing to copy (the Windows installer put them in
/// `data`, or a build that is not installed) is a success. False with
/// `reason` when a folder or file cannot be made.
bool fill_data_folder(std::filesystem::path const& app_dir, std::filesystem::path const& data,
                      std::string& reason);

#endif  // INDICATOR_DATAFOLDER_H
