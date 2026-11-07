#ifndef TOOLS_ZIP_IMPL_H
#define TOOLS_ZIP_IMPL_H
#include <Tools/Zip.h>
#include <string>
#include <string_view>
#include <vector>

namespace Tools {

Result<std::string> to_zip_impl(std::vector<ZipEntry> const& entries) noexcept;
Result<std::vector<ZipEntry>> parse_zip_impl(std::string_view bytes) noexcept;

}  // namespace Tools

#endif  // TOOLS_ZIP_IMPL_H
