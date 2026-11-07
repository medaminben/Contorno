#ifndef CORE_H
#define CORE_H
#include <Core/Problem.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
static_assert(sizeof(void*) == 8, "expected 64-bit platform");
namespace Core {
/**
 * @brief an error wrapper
 *
 * Built explicitly (Error<>(Problem(...))), so a plain value never turns into
 * an error by accident. Reading does not empty it; std::move(error)() takes
 * the content.
 *
 * @param E any movable error type; the libraries use Problem
 */
template <typename E = Problem>
struct Error {
public:
    constexpr explicit Error(E error) noexcept(std::is_nothrow_move_constructible_v<E>)
        : _error(std::move(error)) {}

    constexpr E& operator()() & noexcept { return _error; }
    constexpr const E& operator()() const& noexcept { return _error; }
    constexpr E&& operator()() && noexcept { return std::move(_error); }

protected:
    E _error;
};

namespace detail {
/// Wrong-side access of a Result: a programming error, reported without throwing.
[[noreturn]] inline void result_misuse(const char* what, const char* message = nullptr) noexcept {
    if (message != nullptr) {
        std::fprintf(stderr, "Result: %s: %s\n", what, message);
    } else {
        std::fprintf(stderr, "Result: %s\n", what);
    }
    std::abort();
}

template <typename ET>
const char* error_text(const ET& error) noexcept {
    if constexpr (std::is_invocable_v<const ET&>) {
        if constexpr (std::is_same_v<std::remove_cvref_t<std::invoke_result_t<const ET&>>,
                                     Problem>) {
            return error().text().c_str();
        } else if constexpr (std::is_convertible_v<std::invoke_result_t<const ET&>,
                                                   const std::string&>) {
            return static_cast<const std::string&>(error()).c_str();
        } else {
            return nullptr;
        }
    } else {
        return nullptr;
    }
}
}  // namespace detail
}  // namespace Core

/**
 * @brief  a result wrapper: a value or an error
 *
 * Check isError() (or the explicit bool, true for a value) before reading.
 * r() and r.error() return references and can be read any number of times;
 * std::move(r)() and std::move(r).error() take the content. Reading the side
 * that is not held prints the error and aborts (it never throws).
 *
 * @param T  a value type
 * @param ET  an error type, callable to read its content (like Error<E>)
 */
template <typename T = std::monostate, typename ET = Core::Error<>>
struct [[nodiscard]] Result {
    static_assert(!std::is_same_v<std::remove_cv_t<T>, ET>,
                  "Result: value and error types must differ");
    static_assert(!std::is_reference_v<T>, "Result: the value type cannot be a reference");
    static_assert(std::is_invocable_v<ET&>,
                  "Result: the error type must be callable, like Error<E>");

public:
    constexpr Result(T value) noexcept(std::is_nothrow_move_constructible_v<T>)
        : result(std::move(value)) {}
    constexpr Result(ET error) noexcept(std::is_nothrow_move_constructible_v<ET>)
        : result(std::move(error)) {}

    constexpr bool isValue() const noexcept { return result.index() == 0; }
    constexpr bool isError() const noexcept { return result.index() == 1; }
    constexpr explicit operator bool() const noexcept { return isValue(); }

    constexpr T& operator()() & noexcept { return *value_ptr(); }
    constexpr const T& operator()() const& noexcept { return *value_ptr(); }
    constexpr T&& operator()() && noexcept { return std::move(*value_ptr()); }

    constexpr decltype(auto) error() & noexcept { return (*error_ptr())(); }
    constexpr decltype(auto) error() const& noexcept { return (*error_ptr())(); }
    constexpr decltype(auto) error() && noexcept { return std::move(*error_ptr())(); }

private:
    constexpr T* value_ptr() noexcept { return const_cast<T*>(std::as_const(*this).value_ptr()); }
    constexpr const T* value_ptr() const noexcept {
        if (const T* value = std::get_if<0>(&result)) {
            return value;
        }
        Core::detail::result_misuse("value read from an error",
                                    Core::detail::error_text(*std::get_if<1>(&result)));
    }
    constexpr ET* error_ptr() noexcept { return const_cast<ET*>(std::as_const(*this).error_ptr()); }
    constexpr const ET* error_ptr() const noexcept {
        if (const ET* error = std::get_if<1>(&result)) {
            return error;
        }
        Core::detail::result_misuse("error read from a value");
    }

    std::variant<T, ET> result;
};

namespace Core {
/**
 * @brief Error result for use inside a catch handler; never throws
 *
 * make_problem builds the Problem. If that throws (for example bad_alloc),
 * the problem is Problem::out_of_memory() instead, which needs no allocation.
 */
template <typename T = std::monostate, typename MakeProblem>
Result<T> error_result(MakeProblem&& make_problem) noexcept {
    try {
        return Result<T>(Error<>(make_problem()));
    } catch (...) {
        return Result<T>(Error<>(Problem::out_of_memory()));
    }
}

/// A problem wrapped in another: `english` holds {reason}, the inner problem's text.
inline Problem wrapped(std::string code, std::string_view english, Problem reason) {
    return Problem(std::move(code), english, {Problem::Value("reason", std::move(reason))});
}

}  // namespace Core

#endif  // CORE_H
