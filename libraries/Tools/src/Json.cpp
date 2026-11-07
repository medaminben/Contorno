#include "Json_impl.h"

#include <Tools/Json.h>

namespace Tools {

Json::Json(bool value) : _value(value) {}
Json::Json(double value) : _value(value) {}
Json::Json(std::string value) : _value(std::move(value)) {}
Json::Json(Array value) : _value(std::move(value)) {}
Json::Json(Object value) : _value(std::move(value)) {}

bool Json::is_null() const noexcept {
    return std::holds_alternative<std::nullptr_t>(_value);
}
bool Json::is_bool() const noexcept {
    return std::holds_alternative<bool>(_value);
}
bool Json::is_number() const noexcept {
    return std::holds_alternative<double>(_value);
}
bool Json::is_string() const noexcept {
    return std::holds_alternative<std::string>(_value);
}
bool Json::is_array() const noexcept {
    return std::holds_alternative<Array>(_value);
}
bool Json::is_object() const noexcept {
    return std::holds_alternative<Object>(_value);
}

std::optional<bool> Json::as_bool() const noexcept {
    if (auto const* v = std::get_if<bool>(&_value)) {
        return *v;
    }
    return std::nullopt;
}

std::optional<double> Json::as_number() const noexcept {
    if (auto const* v = std::get_if<double>(&_value)) {
        return *v;
    }
    return std::nullopt;
}

std::string const* Json::as_string() const noexcept {
    return std::get_if<std::string>(&_value);
}

Json::Array const* Json::as_array() const noexcept {
    return std::get_if<Array>(&_value);
}
Json::Object const* Json::as_object() const noexcept {
    return std::get_if<Object>(&_value);
}

Json const* Json::find(std::string_view key) const noexcept {
    auto const* object = as_object();
    if (object == nullptr) {
        return nullptr;
    }
    for (auto const& member : *object) {
        if (member.first == key) {
            return &member.second;
        }
    }
    return nullptr;
}

Result<std::string> to_json_text(Json const& value, int indent) noexcept {
    return to_json_text_impl(value, indent);
}
Result<Json> parse_json(std::string_view text) noexcept {
    return parse_json_impl(text);
}

}  // namespace Tools
