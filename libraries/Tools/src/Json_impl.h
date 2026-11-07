#ifndef TOOLS_JSON_IMPL_H
#define TOOLS_JSON_IMPL_H
#include <Tools/Json.h>
#include <string>
#include <string_view>

namespace Tools {

Result<Json> parse_json_impl(std::string_view text) noexcept;
Result<std::string> to_json_text_impl(Json const& value, int indent) noexcept;

}  // namespace Tools

#endif  // TOOLS_JSON_IMPL_H
