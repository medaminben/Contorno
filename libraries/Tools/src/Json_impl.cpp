#include "Json_impl.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <exception>
#include <iterator>
#include <system_error>

namespace Tools {
namespace {
constexpr int max_depth = 512;

// Thrown inside the parser only; parse_json_impl turns it into an error Result.
struct ParseError {
    std::size_t offset;
    Core::Problem problem;
};

class Parser {
public:
    explicit Parser(std::string_view text) : _text(text) {}

    Json parse_document() {
        if (_text.substr(0, 3) == "\xEF\xBB\xBF") {
            _text.remove_prefix(3);
        }
        skip_whitespace();
        Json value = parse_value(0);
        skip_whitespace();
        if (_pos != _text.size()) {
            fail(problem("json.trailing", "unexpected content after the JSON value"));
        }
        return value;
    }

    Core::Problem describe(ParseError const& error) const {
        std::size_t line = 1;
        std::size_t column = 1;
        for (std::size_t i = 0; i < error.offset && i < _text.size(); ++i) {
            if (_text[i] == '\n') {
                ++line;
                column = 1;
            } else {
                ++column;
            }
        }
        return Core::Problem("json.at", "line {line}, column {column}: {reason}",
                             {{"line", std::to_string(line)},
                              {"column", std::to_string(column)},
                              {"reason", error.problem}});
    }

private:
    std::string_view _text;
    std::size_t _pos {0};

    static Core::Problem problem(char const* code, std::string_view english,
                                 std::vector<Core::Problem::Value> values = {}) {
        return Core::Problem(code, english, std::move(values));
    }
    [[noreturn]] void fail(Core::Problem problem) const {
        throw ParseError {_pos, std::move(problem)};
    }
    [[noreturn]] void fail_at(std::size_t offset, Core::Problem problem) const {
        throw ParseError {offset, std::move(problem)};
    }

    bool at_end() const { return _pos >= _text.size(); }
    char peek() const { return _text[_pos]; }

    void skip_whitespace() {
        while (!at_end() && (peek() == ' ' || peek() == '\t' || peek() == '\n' || peek() == '\r')) {
            ++_pos;
        }
    }

    Json parse_value(int depth) {
        if (at_end()) {
            fail(problem("json.end", "unexpected end of input"));
        }
        switch (peek()) {
            case '{':
                return parse_object(depth + 1);
            case '[':
                return parse_array(depth + 1);
            case '"':
                return Json(parse_string());
            case 't':
                parse_literal("true");
                return Json(true);
            case 'f':
                parse_literal("false");
                return Json(false);
            case 'n':
                parse_literal("null");
                return Json();
            default:
                break;
        }
        if (peek() == '-' || (peek() >= '0' && peek() <= '9')) {
            return parse_number();
        }
        fail(problem("json.character", "unexpected character"));
    }

    void parse_literal(std::string_view literal) {
        if (_text.substr(_pos, literal.size()) != literal) {
            fail(problem("json.literal", "invalid literal, expected '{literal}'",
                         {{"literal", std::string(literal)}}));
        }
        _pos += literal.size();
    }

    Json parse_array(int depth) {
        if (depth > max_depth) {
            fail(problem("json.depth", "nesting deeper than {depth}",
                         {{"depth", std::to_string(max_depth)}}));
        }
        ++_pos;  // '['
        Json::Array items;
        skip_whitespace();
        if (!at_end() && peek() == ']') {
            ++_pos;
            return Json(std::move(items));
        }
        while (true) {
            skip_whitespace();
            items.push_back(parse_value(depth));
            skip_whitespace();
            if (at_end()) {
                fail(problem("json.array_open", "unterminated array"));
            }
            if (peek() == ',') {
                ++_pos;
            } else if (peek() == ']') {
                ++_pos;
                return Json(std::move(items));
            } else {
                fail(problem("json.comma_bracket", "expected ',' or ']'"));
            }
        }
    }

    Json parse_object(int depth) {
        if (depth > max_depth) {
            fail(problem("json.depth", "nesting deeper than {depth}",
                         {{"depth", std::to_string(max_depth)}}));
        }
        ++_pos;  // '{'
        Json::Object members;
        skip_whitespace();
        if (!at_end() && peek() == '}') {
            ++_pos;
            return Json(std::move(members));
        }
        while (true) {
            skip_whitespace();
            if (at_end()) {
                fail(problem("json.object_open", "unterminated object"));
            }
            if (peek() != '"') {
                fail(problem("json.key", "expected a string key"));
            }
            std::string key = parse_string();
            skip_whitespace();
            if (at_end() || peek() != ':') {
                fail(problem("json.colon", "expected ':'"));
            }
            ++_pos;
            skip_whitespace();
            Json value = parse_value(depth);
            members.emplace_back(std::move(key), std::move(value));
            skip_whitespace();
            if (at_end()) {
                fail(problem("json.object_open", "unterminated object"));
            }
            if (peek() == ',') {
                ++_pos;
            } else if (peek() == '}') {
                ++_pos;
                return Json(std::move(members));
            } else {
                fail(problem("json.comma_brace", "expected ',' or '}'"));
            }
        }
    }

    bool is_digit() const { return !at_end() && peek() >= '0' && peek() <= '9'; }

    Json parse_number() {
        std::size_t const start = _pos;
        bool const negative = peek() == '-';
        if (negative) {
            ++_pos;
        }
        if (!is_digit()) {
            fail_at(start, problem("json.number", "invalid number"));
        }
        std::size_t const int_start = _pos;
        if (peek() == '0') {
            ++_pos;
        } else {
            while (is_digit()) {
                ++_pos;
            }
        }
        std::size_t const int_end = _pos;
        std::size_t frac_start = _pos;
        std::size_t frac_end = _pos;
        if (!at_end() && peek() == '.') {
            ++_pos;
            if (!is_digit()) {
                fail_at(start, problem("json.number_fraction",
                                       "invalid number: digits expected after '.'"));
            }
            frac_start = _pos;
            while (is_digit()) {
                ++_pos;
            }
            frac_end = _pos;
        }
        long exponent = 0;
        if (!at_end() && (peek() == 'e' || peek() == 'E')) {
            ++_pos;
            bool exponent_negative = false;
            if (!at_end() && (peek() == '+' || peek() == '-')) {
                exponent_negative = peek() == '-';
                ++_pos;
            }
            if (!is_digit()) {
                fail_at(start, problem("json.number_exponent",
                                       "invalid number: digits expected in exponent"));
            }
            while (is_digit()) {
                // saturate: anything beyond this is far outside double's range
                exponent = std::min(exponent * 10 + (peek() - '0'), 1000000L);
                ++_pos;
            }
            if (exponent_negative) {
                exponent = -exponent;
            }
        }
        double value = 0.0;
        auto const* first = _text.data() + start;
        auto const* last = _text.data() + _pos;
        auto const result = std::from_chars(first, last, value);
        if (result.ec == std::errc::result_out_of_range &&
            decimal_order(int_start, int_end, frac_start, frac_end, exponent) < 0) {
            value = negative ? -0.0 : 0.0;  // underflow rounds to zero
        } else if (result.ec != std::errc() || result.ptr != last) {
            fail_at(start, problem("json.number_range", "number out of range"));
        }
        return Json(value);
    }

    // n such that the number lies in [10^(n-1), 10^n), from its digits and
    // exponent; only called for non-zero numbers.
    long decimal_order(std::size_t int_start, std::size_t int_end, std::size_t frac_start,
                       std::size_t frac_end, long exponent) const {
        for (std::size_t i = int_start; i < int_end; ++i) {
            if (_text[i] != '0') {
                return static_cast<long>(int_end - i) + exponent;
            }
        }
        for (std::size_t i = frac_start; i < frac_end; ++i) {
            if (_text[i] != '0') {
                return exponent - static_cast<long>(i - frac_start);
            }
        }
        return 0;
    }

    // Reads the 4 hex digits after "\\u"; errors point at escape_start.
    unsigned parse_hex4(std::size_t escape_start) {
        if (_pos + 4 > _text.size()) {
            fail_at(escape_start, problem("json.u_incomplete", "incomplete \\u escape"));
        }
        unsigned code = 0;
        for (int i = 0; i < 4; ++i) {
            char const c = _text[_pos + static_cast<std::size_t>(i)];
            unsigned digit = 0;
            if (c >= '0' && c <= '9') {
                digit = static_cast<unsigned>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                digit = static_cast<unsigned>(c - 'a') + 10;
            } else if (c >= 'A' && c <= 'F') {
                digit = static_cast<unsigned>(c - 'A') + 10;
            } else {
                fail_at(escape_start, problem("json.u_hex", "invalid hex digit in \\u escape"));
            }
            code = code * 16 + digit;
        }
        _pos += 4;
        return code;
    }

    static void append_utf8(std::string& out, unsigned code) {
        if (code < 0x80) {
            out += static_cast<char>(code);
        } else if (code < 0x800) {
            out += static_cast<char>(0xC0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else if (code < 0x10000) {
            out += static_cast<char>(0xE0 | (code >> 12));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (code >> 18));
            out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
    }

    // Length of the valid UTF-8 sequence starting at _pos (RFC 3629: no
    // overlong forms, no surrogates, nothing above U+10FFFF), 0 if invalid.
    std::size_t utf8_length() const {
        auto byte = [&](std::size_t k) {
            return _pos + k < _text.size() ? static_cast<unsigned char>(_text[_pos + k]) : 0u;
        };
        auto continuation = [&](std::size_t k) { return (byte(k) & 0xC0) == 0x80; };
        unsigned const b0 = byte(0);
        unsigned const b1 = byte(1);
        if (b0 >= 0xC2 && b0 <= 0xDF) {
            return continuation(1) ? 2 : 0;
        }
        if (b0 >= 0xE0 && b0 <= 0xEF) {
            bool const second_ok = b0 == 0xE0   ? (b1 >= 0xA0 && b1 <= 0xBF)
                                   : b0 == 0xED ? (b1 >= 0x80 && b1 <= 0x9F)
                                                : continuation(1);
            return second_ok && continuation(2) ? 3 : 0;
        }
        if (b0 >= 0xF0 && b0 <= 0xF4) {
            bool const second_ok = b0 == 0xF0   ? (b1 >= 0x90 && b1 <= 0xBF)
                                   : b0 == 0xF4 ? (b1 >= 0x80 && b1 <= 0x8F)
                                                : continuation(1);
            return second_ok && continuation(2) && continuation(3) ? 4 : 0;
        }
        return 0;
    }

    std::string parse_string() {
        std::size_t const start = _pos;
        ++_pos;  // opening quote
        std::string out;
        while (true) {
            if (at_end()) {
                fail_at(start, problem("json.string_open", "unterminated string"));
            }
            char const c = peek();
            if (c == '"') {
                ++_pos;
                return out;
            }
            if (static_cast<unsigned char>(c) < 0x20) {
                fail(problem("json.control", "control character in string"));
            }
            if (static_cast<unsigned char>(c) >= 0x80) {
                auto const length = utf8_length();
                if (length == 0) {
                    fail(problem("json.utf8", "invalid UTF-8 in string"));
                }
                out.append(_text.substr(_pos, length));
                _pos += length;
                continue;
            }
            if (c != '\\') {
                out += c;
                ++_pos;
                continue;
            }
            std::size_t const escape_start = _pos;
            ++_pos;
            if (at_end()) {
                fail_at(start, problem("json.string_open", "unterminated string"));
            }
            char const e = peek();
            ++_pos;
            switch (e) {
                case '"':
                    out += '"';
                    break;
                case '\\':
                    out += '\\';
                    break;
                case '/':
                    out += '/';
                    break;
                case 'b':
                    out += '\b';
                    break;
                case 'f':
                    out += '\f';
                    break;
                case 'n':
                    out += '\n';
                    break;
                case 'r':
                    out += '\r';
                    break;
                case 't':
                    out += '\t';
                    break;
                case 'u': {
                    unsigned code = parse_hex4(escape_start);
                    if (code >= 0xD800 && code <= 0xDBFF) {
                        if (_text.substr(_pos, 2) != "\\u") {
                            fail_at(escape_start,
                                    problem("json.high_surrogate",
                                            "high surrogate without a low surrogate"));
                        }
                        _pos += 2;
                        unsigned const low = parse_hex4(escape_start);
                        if (low < 0xDC00 || low > 0xDFFF) {
                            fail_at(escape_start,
                                    problem("json.high_surrogate",
                                            "high surrogate without a low surrogate"));
                        }
                        code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                    } else if (code >= 0xDC00 && code <= 0xDFFF) {
                        fail_at(escape_start,
                                problem("json.low_surrogate", "unpaired low surrogate"));
                    }
                    append_utf8(out, code);
                    break;
                }
                default:
                    fail_at(escape_start, problem("json.escape", "invalid escape sequence"));
            }
        }
    }
};
}  // namespace

Result<Json> parse_json_impl(std::string_view text) noexcept {
    try {
        Parser parser(text);
        try {
            return Result<Json>(parser.parse_document());
        } catch (ParseError const& error) {
            return Core::error_result<Json>([&] { return parser.describe(error); });
        }
    } catch (std::exception const& exception) {
        return Core::error_result<Json>([&] {
            return Core::Problem("json.parse_failed", "json parse failed: {what}",
                                 {{"what", exception.what()}});
        });
    } catch (...) {
        return Core::error_result<Json>(
            [] { return Core::Problem("json.parse_unknown", "json parse failed: unknown error"); });
    }
}

namespace {
void write_string(std::string& out, std::string const& text) {
    out += '"';
    for (unsigned char c : text) {
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            default:
                if (c < 0x20) {
                    char buffer[8];
                    std::snprintf(buffer, sizeof buffer, "\\u%04x", static_cast<unsigned>(c));
                    out += buffer;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    out += '"';
}

void write_number(std::string& out, double value) {
    if (!std::isfinite(value)) {
        out += "null";
        return;
    }
    char buffer[64];
    auto const result =
        std::to_chars(buffer, buffer + sizeof buffer, value);  // shortest round-trip form
    out.append(buffer, result.ptr);
}

void write_value(std::string& out, Json const& value, int indent, int level) {
    auto newline = [&](int depth) {
        if (indent > 0) {
            out += '\n';
            out.append(static_cast<std::size_t>(indent) * static_cast<std::size_t>(depth), ' ');
        }
    };
    if (value.is_null()) {
        out += "null";
    } else if (auto b = value.as_bool()) {
        out += *b ? "true" : "false";
    } else if (auto n = value.as_number()) {
        write_number(out, *n);
    } else if (auto const* s = value.as_string()) {
        write_string(out, *s);
    } else if (auto const* array = value.as_array()) {
        if (array->empty()) {
            out += "[]";
            return;
        }
        bool const flat = std::all_of(array->begin(), array->end(),
                                      [](Json const& item) { return item.is_number(); });
        out += '[';
        for (std::size_t i = 0; i < array->size(); ++i) {
            if (i > 0) {
                out += flat && indent > 0 ? ", " : ",";
            }
            if (!flat) {
                newline(level + 1);
            }
            write_value(out, (*array)[i], indent, level + 1);
        }
        if (!flat) {
            newline(level);
        }
        out += ']';
    } else if (auto const* object = value.as_object()) {
        if (object->empty()) {
            out += "{}";
            return;
        }
        out += '{';
        for (std::size_t i = 0; i < object->size(); ++i) {
            if (i > 0) {
                out += ',';
            }
            newline(level + 1);
            write_string(out, (*object)[i].first);
            out += indent > 0 ? ": " : ":";
            write_value(out, (*object)[i].second, indent, level + 1);
        }
        newline(level);
        out += '}';
    }
}
}  // namespace

Result<std::string> to_json_text_impl(Json const& value, int indent) noexcept {
    if (indent < 0) {
        return Core::error_result<std::string>([] {
            return Core::wrapped("json.failed", "json: {reason}",
                                 Core::Problem("json.indent", "indent must not be negative"));
        });
    }
    try {
        std::string out;
        write_value(out, value, indent, 0);
        return Result<std::string>(std::move(out));
    } catch (std::exception const& exception) {
        return Core::error_result<std::string>([&] {
            return Core::wrapped("json.failed", "json: {reason}",
                                 Core::Problem::raw(exception.what()));
        });
    }
}

}  // namespace Tools
