#ifndef CORE_PROBLEM_H
#define CORE_PROBLEM_H
#include <exception>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Core {

/**
 * @brief What went wrong, in a form any language can word
 *
 * A stable code naming the sentence ("zip.entry_crc"), the values of its
 * {name} placeholders, and the English sentence they make (for logs and
 * tests). A value is plain text (a path, a number with its unit) or a nested
 * problem (the reason). An application translates by the code and the
 * values; the English text is never parsed.
 */
class Problem {
public:
    struct Value {
        Value(std::string name, std::string plain) : name(std::move(name)), plain(std::move(plain)) {}
        Value(std::string name, Problem nested)
            : name(std::move(name)), nested(std::make_shared<Problem const>(std::move(nested))) {}

        std::string name;
        std::string plain {};                    ///< empty for a nested problem
        std::shared_ptr<Problem const> nested {};  ///< set for a nested problem
    };

    /// `english` holds {name} placeholders, filled from `values` (plain text,
    /// or a nested problem's text); a {name} without a value stays as written.
    Problem(std::string code, std::string_view english, std::vector<Value> values = {})
        : _code(std::move(code)), _values(std::move(values)), _text(fill(english, _values)) {}

    /// Text from outside the libraries (another library's exception): code
    /// "raw", one value "text", the text as it is.
    static Problem raw(std::string text) { return Problem("raw", "{text}", {Value("text", std::move(text))}); }

    /// Code and text "out_of_memory" / "out of memory": short enough to need no allocation.
    static Problem out_of_memory() noexcept { return Problem(OutOfMemory {}); }

    std::string const& code() const noexcept { return _code; }
    std::vector<Value> const& values() const noexcept { return _values; }
    std::string const& text() const noexcept { return _text; }

    /// The value called `name`; nullptr when there is none.
    Value const* value(std::string_view name) const noexcept {
        for (auto const& v : _values) {
            if (v.name == name) return &v;
        }
        return nullptr;
    }

private:
    struct OutOfMemory {};
    explicit Problem(OutOfMemory) noexcept : _code("out_of_memory"), _text("out of memory") {}

    static std::string fill(std::string_view english, std::vector<Value> const& values) {
        std::string out;
        std::size_t at = 0;
        while (at < english.size()) {
            std::size_t const open = english.find('{', at);
            std::size_t const close = open == std::string_view::npos ? open : english.find('}', open);
            if (close == std::string_view::npos) break;
            out += english.substr(at, open - at);
            std::string_view const name = english.substr(open + 1, close - open - 1);
            Value const* found = nullptr;
            for (auto const& v : values) {
                if (v.name == name) {
                    found = &v;
                    break;
                }
            }
            if (found == nullptr) out += english.substr(open, close - open + 1);
            else out += found->nested ? found->nested->text() : found->plain;
            at = close + 1;
        }
        out += english.substr(at);
        return out;
    }

    std::string _code;
    std::vector<Value> _values;
    std::string _text;
};

/// The English text (logs, test output).
inline std::ostream& operator<<(std::ostream& out, Problem const& problem) { return out << problem.text(); }

/// A problem thrown inside a library; its API catches it and returns it as an error.
class ProblemError : public std::exception {
public:
    explicit ProblemError(Problem problem) : _problem(std::move(problem)) {}
    Problem const& problem() const noexcept { return _problem; }
    char const* what() const noexcept override { return _problem.text().c_str(); }

private:
    Problem _problem;
};

}  // namespace Core

#endif  // CORE_PROBLEM_H
