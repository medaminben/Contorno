#ifndef TOOLS_ZIP_H
#define TOOLS_ZIP_H
#include <Tools/Tools_export.h>
#include <Core/Core.h>
#include <string>
#include <string_view>
#include <vector>

namespace Tools {

/// One file of a zip archive.
struct ZipEntry {
    std::string name {};  ///< path inside the archive, '/' separated
    std::string data {};  ///< file contents (bytes)
};

/**
 * @brief A zip archive (PKWARE APPNOTE) of the entries, in their order
 * Entries are stored without compression, with their CRC-32 and the date
 * 1980-01-01 00:00 (the same entries give the same bytes). No zip64: error
 * for more than 65535 entries or sizes and offsets of 4 GiB or more. Error
 * for an empty or duplicate name, or a name that starts with '/', contains
 * '\' or has a ".." segment.
 */
Result<std::string> TOOLS_API to_zip(std::vector<ZipEntry> const& entries) noexcept;
Result<> TOOLS_API write_zip(std::string const& path,
                             std::vector<ZipEntry> const& entries) noexcept;

/**
 * @brief The entries of a zip archive, in the order of its central directory
 * Reads stored entries only (also from other tools, e.g. `zip -0`); a
 * compressed entry is an error naming the entry and its method. Checks every
 * entry's CRC-32 and its name (as to_zip does), and that every header lies
 * within the archive.
 */
Result<std::vector<ZipEntry>> TOOLS_API parse_zip(std::string_view bytes) noexcept;
/// Reads the whole file and parses it with parse_zip.
Result<std::vector<ZipEntry>> TOOLS_API read_zip(std::string const& path) noexcept;

}  // namespace Tools
#endif  // TOOLS_ZIP_H
