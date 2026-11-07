#include <Core/Core.h>
// Include 3rd party headers
#include <gtest/gtest.h>
// Include system headers
#include <iostream>
#include <new>
#include <string>
#include <type_traits>
#include <vector>

namespace {
/// A problem whose code and text say only what the test needs.
Core::Problem bad(std::string text) { return Core::Problem("test.bad", text); }
}  // namespace

TEST(test_Core_result, result_properties) {
    auto result = Result<int>(5);
    EXPECT_EQ(result(), 5);
    EXPECT_EQ(result.isValue(), true);
    EXPECT_EQ(result.isError(), false);
}

TEST(test_Core_error, error_properties) {
    auto error = Core::Error<>(bad("error"));
    auto result = Result<int>(error);
    EXPECT_STREQ(result.error().text().c_str(), error().text().c_str());
    EXPECT_EQ(result.isValue(), false);
    EXPECT_EQ(result.isError(), true);
}

TEST(test_Core_result_simple, simple_result_properties) {
    auto error = Core::Error<>(bad("error"));
    auto result = Result<int>(error);
    EXPECT_STREQ(result.error().text().c_str(), error().text().c_str());
    EXPECT_EQ(result.isValue(), false);
    EXPECT_EQ(result.isError(), true);
}

TEST(test_Core_result_void, void_result_properties) {
    auto error = Core::Error<>(bad("error"));
    auto result = Result<>(error);
    EXPECT_STREQ(result.error().text().c_str(), error().text().c_str());
    EXPECT_EQ(result.isValue(), false);
    EXPECT_EQ(result.isError(), true);
}
TEST(test_Core_error_result, builds_the_message) {
    auto result = Core::error_result<int>([] { return bad("bad input"); });
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.error().text(), "bad input");
}

TEST(test_Core_error_result, failing_message_becomes_out_of_memory) {
    auto result = Core::error_result([]() -> Core::Problem { throw std::bad_alloc(); });
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.error().text(), "out of memory");
    EXPECT_EQ(result.error().code(), "out_of_memory");
}

TEST(test_Core_result_access, value_can_be_read_twice) {
    auto result = Result<std::vector<int>>(std::vector<int>{1, 2, 3});
    auto first = result();
    auto second = result();
    EXPECT_EQ(first.size(), 3u);
    EXPECT_EQ(second.size(), 3u);
}

TEST(test_Core_result_access, error_can_be_read_twice) {
    auto result = Result<int>(Core::Error<>(bad("bad input")));
    std::string first = result.error().text();
    std::string second = result.error().text();
    EXPECT_EQ(first, "bad input");
    EXPECT_EQ(second, "bad input");
}

TEST(test_Core_result_access, const_result_is_readable) {
    const auto value = Result<int>(4);
    const auto error = Result<int>(Core::Error<>(bad("bad")));
    EXPECT_EQ(value(), 4);
    EXPECT_EQ(error.error().text(), "bad");
    EXPECT_TRUE(static_cast<bool>(value));
    EXPECT_FALSE(static_cast<bool>(error));
}

TEST(test_Core_result_access, moving_takes_the_content) {
    auto result = Result<std::vector<int>>(std::vector<int>{1, 2, 3});
    auto taken = std::move(result)();
    EXPECT_EQ(taken.size(), 3u);
    auto error = Result<int>(Core::Error<>(bad("bad")));
    Core::Problem problem = std::move(error).error();
    EXPECT_EQ(problem.text(), "bad");
}

TEST(test_Core_result_access, lvalue_read_returns_a_reference) {
    auto result = Result<std::vector<int>>(std::vector<int>{1, 2, 3});
    result().push_back(4);
    EXPECT_EQ(result().size(), 4u);
}

static_assert(!std::is_convertible_v<std::string, Core::Error<>>, "Error must be built explicitly");
static_assert(!std::is_constructible_v<Core::Error<>, std::string>, "a library error is a Problem, never bare text");
static_assert(!std::is_convertible_v<std::string, Result<int>>, "a string is not a Result");
static_assert(!std::is_convertible_v<Result<int>, bool>, "operator bool is explicit");
static_assert(std::is_constructible_v<bool, Result<int>>);

using ResultDeathTest = ::testing::Test;

TEST(ResultDeathTest, reading_the_value_of_an_error_aborts) {
    auto result = Result<int>(Core::Error<>(bad("bad")));
    EXPECT_DEATH((void)result(), "Result: value read from an error: bad");
}

TEST(ResultDeathTest, reading_the_error_of_a_value_aborts) {
    auto result = Result<int>(3);
    EXPECT_DEATH((void)result.error(), "Result: error read from a value");
}

TEST(test_Core_problem, fills_the_placeholders) {
    Core::Problem const p("zip.entry_crc", "entry \"{name}\": CRC-32 mismatch", {{"name", "a.txt"}});
    EXPECT_EQ(p.code(), "zip.entry_crc");
    EXPECT_EQ(p.text(), "entry \"a.txt\": CRC-32 mismatch");
    ASSERT_NE(p.value("name"), nullptr);
    EXPECT_EQ(p.value("name")->plain, "a.txt");
    EXPECT_EQ(p.value("other"), nullptr);
}

TEST(test_Core_problem, nests_a_reason) {
    auto const p = Core::wrapped("zip.failed", "zip: {reason}", Core::Problem("zip.not_a_zip", "not a zip"));
    EXPECT_EQ(p.text(), "zip: not a zip");
    ASSERT_NE(p.value("reason"), nullptr);
    ASSERT_TRUE(p.value("reason")->nested);
    EXPECT_EQ(p.value("reason")->nested->code(), "zip.not_a_zip");
}

TEST(test_Core_problem, unknown_placeholder_stays) {
    EXPECT_EQ(Core::Problem("x", "a {b} c {d").text(), "a {b} c {d");
}

TEST(test_Core_problem, raw_text_is_taken_as_it_is) {
    auto const p = Core::Problem::raw("cv: {not a placeholder}");
    EXPECT_EQ(p.code(), "raw");
    EXPECT_EQ(p.text(), "cv: {not a placeholder}");
    EXPECT_EQ(p.value("text")->plain, "cv: {not a placeholder}");
}

TEST(test_Core_problem, thrown_and_caught) {
    try {
        throw Core::ProblemError(bad("inside"));
    } catch (std::exception const& e) {
        EXPECT_STREQ(e.what(), "inside");
        EXPECT_EQ(dynamic_cast<Core::ProblemError const&>(e).problem().code(), "test.bad");
    }
}
