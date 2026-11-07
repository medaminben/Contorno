#ifndef TOOLS_JSON_H
#define TOOLS_JSON_H
#include <Tools/Tools_export.h>
#include <Core/Core.h>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace Tools {

/**
 * @brief A parsed JSON value (RFC 8259).
 * Objects keep the order of the file; numbers are doubles.
 */
class TOOLS_API Json {
public:
    using Array = std::vector<Json>;
    using Object = std::vector<std::pair<std::string, Json>>;  // keeps file order
    Json() = default;                                          // null
    explicit Json(bool value);
    explicit Json(double value);
    explicit Json(std::string value);
    explicit Json(Array value);
    explicit Json(Object value);

    bool is_null() const noexcept;
    bool is_bool() const noexcept;
    bool is_number() const noexcept;
    bool is_string() const noexcept;
    bool is_array() const noexcept;
    bool is_object() const noexcept;

    std::optional<bool> as_bool() const noexcept;
    std::optional<double> as_number() const noexcept;
    std::string const* as_string() const noexcept;          // nullptr when not a string
    Array const* as_array() const noexcept;                 // nullptr when not an array
    Object const* as_object() const noexcept;               // nullptr when not an object
    Json const* find(std::string_view key) const noexcept;  // nullptr when absent or not an object

private:
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> _value {nullptr};
};

/**
 * @brief Parse a JSON document. A leading UTF-8 byte order mark is skipped,
 * trailing content and nesting deeper than 512 are errors. Strings must be
 * valid UTF-8. Numbers too large for a double are errors; numbers too small
 * round to zero. Every error message starts with "line L, column C:" (1-based)
 * at the first character of the failing token (the opening quote of a string,
 * the backslash of an escape, the first character of a number), or at the
 * offending byte for invalid UTF-8.
 */
Result<Json> TOOLS_API parse_json(std::string_view text) noexcept;

/// Serialize; numbers with up to 17 significant digits (round-trip exact), strings escaped per RFC
/// 8259. indent is the number of spaces per level (0 = one line); arrays of numbers stay on one
/// line. Non-finite numbers are written as null.
/// @return error if indent is negative or the text cannot be allocated
Result<std::string> TOOLS_API to_json_text(Json const& value, int indent = 2) noexcept;

}  // namespace Tools
#endif  // TOOLS_JSON_H
