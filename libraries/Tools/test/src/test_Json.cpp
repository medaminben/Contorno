#include <Tools/Tools.h>
// Include 3rd party headers
#include <gtest/gtest.h>
// Include system headers
#include <cmath>
#include <string>

namespace test_Json {

TEST(test_Tools_json, values) {
    auto json = Tools::parse_json(R"({"a": [1, -2.5, 1e3, true, null], "s": "x\"y\u00e9", "o": {}})");
    ASSERT_TRUE(json.isValue()) << json.error();
    auto const root = json();
    auto const* a = root.find("a")->as_array();
    ASSERT_NE(a, nullptr);
    ASSERT_EQ(a->size(), 5u);
    EXPECT_DOUBLE_EQ(*(*a)[0].as_number(), 1.0);
    EXPECT_DOUBLE_EQ(*(*a)[1].as_number(), -2.5);
    EXPECT_DOUBLE_EQ(*(*a)[2].as_number(), 1000.0);
    EXPECT_EQ((*a)[3].as_bool(), true);
    EXPECT_TRUE((*a)[4].is_null());
    ASSERT_NE(root.find("s")->as_string(), nullptr);
    EXPECT_EQ(*root.find("s")->as_string(), std::string("x\"y\xC3\xA9"));
    EXPECT_TRUE(root.find("o")->is_object());
    EXPECT_EQ(root.find("missing"), nullptr);
}

TEST(test_Tools_json, crlf_and_bom) {
    auto json = Tools::parse_json("\xEF\xBB\xBF{\r\n\"k\": 1\r\n}\r\n");
    ASSERT_TRUE(json.isValue()) << json.error();
    EXPECT_DOUBLE_EQ(*json().find("k")->as_number(), 1.0);
}

TEST(test_Tools_json, errors_have_position) {
    auto json = Tools::parse_json("{\n  \"k\": tru\n}");
    ASSERT_TRUE(json.isError());
    EXPECT_EQ(json.error().text().rfind("line 2, column 8:", 0), 0u) << json.error();
    EXPECT_TRUE(Tools::parse_json("[1, 2").isError());
    EXPECT_TRUE(Tools::parse_json("{} x").isError());
    EXPECT_TRUE(Tools::parse_json(std::string(600, '[') + std::string(600, ']')).isError());
}

TEST(test_Tools_json, raw_utf8_and_simple_escapes) {
    auto json = Tools::parse_json(R"(["é\n\t\/\\", "😀", "€"])");
    ASSERT_TRUE(json.isValue()) << json.error();
    auto const& a = *json().as_array();
    ASSERT_NE(a[0].as_string(), nullptr);
    EXPECT_EQ(*a[0].as_string(), std::string("\xC3\xA9\n\t/\\"));
    ASSERT_NE(a[1].as_string(), nullptr);
    EXPECT_EQ(*a[1].as_string(), std::string("\xF0\x9F\x98\x80"));
    ASSERT_NE(a[2].as_string(), nullptr);
    EXPECT_EQ(*a[2].as_string(), std::string("\xE2\x82\xAC"));
    EXPECT_TRUE(Tools::parse_json(R"("\ud83d")").isError());
    EXPECT_TRUE(Tools::parse_json(R"("\ude00")").isError());
    EXPECT_TRUE(Tools::parse_json(R"("\q")").isError());
}

TEST(test_Tools_json, unicode_escapes_decode) {
    auto decode = [](std::string const& text) {
        auto json = Tools::parse_json(text);
        EXPECT_TRUE(json.isValue()) << json.error();
        auto const* s = json.isValue() ? json().as_string() : nullptr;
        return s != nullptr ? std::optional<std::string>(*s) : std::nullopt;
    };
    EXPECT_EQ(decode(R"("\u0041")"), std::string("A"));
    EXPECT_EQ(decode(R"("\u00e9")"), std::string("\xC3\xA9"));
    EXPECT_EQ(decode(R"("\u00E9")"), std::string("\xC3\xA9"));
    EXPECT_EQ(decode(R"("\u20ac")"), std::string("\xE2\x82\xAC"));
    EXPECT_EQ(decode(R"("\ud83d\ude00")"), std::string("\xF0\x9F\x98\x80"));
}

TEST(test_Tools_json, depth_limit_boundary) {
    EXPECT_TRUE(Tools::parse_json(std::string(512, '[') + std::string(512, ']')).isValue());
    EXPECT_TRUE(Tools::parse_json(std::string(513, '[') + std::string(513, ']')).isError());
}

TEST(test_Tools_json, malformed_numbers) {
    EXPECT_TRUE(Tools::parse_json("01").isError());
    EXPECT_TRUE(Tools::parse_json("1.").isError());
    EXPECT_TRUE(Tools::parse_json("-").isError());
    EXPECT_TRUE(Tools::parse_json("[1,]").isError());
    EXPECT_TRUE(Tools::parse_json("").isError());
}

TEST(test_Tools_json, error_columns_point_at_the_token_start) {
    auto column_of = [](std::string const& text) {
        auto json = Tools::parse_json(text);
        EXPECT_TRUE(json.isError()) << text;
        return json.isError() ? json.error().text().substr(0, json.error().text().find(':')) : std::string();
    };
    EXPECT_EQ(column_of("[1, -]"), "line 1, column 5");
    EXPECT_EQ(column_of("[1, 1.]"), "line 1, column 5");
    EXPECT_EQ(column_of("[1, 1e]"), "line 1, column 5");
    EXPECT_EQ(column_of("[1, 1e+]"), "line 1, column 5");
    EXPECT_EQ(column_of("[1, \"abc"), "line 1, column 5");
    EXPECT_EQ(column_of("[1, \"abc\\"), "line 1, column 5");
    EXPECT_EQ(column_of(R"(["ab\u12x4"])"), "line 1, column 5");
    EXPECT_EQ(column_of(R"(["ab\u12"])"), "line 1, column 5");
    EXPECT_EQ(column_of("[1e999]"), "line 1, column 2");
}

TEST(test_Tools_json, underflow_rounds_to_zero) {
    auto json = Tools::parse_json("[1e-999, -1e-999, 0.000001e-400, 1e-300]");
    ASSERT_TRUE(json.isValue()) << json.error();
    auto const& a = *json().as_array();
    EXPECT_EQ(*a[0].as_number(), 0.0);
    EXPECT_TRUE(std::signbit(*a[1].as_number()));
    EXPECT_EQ(*a[2].as_number(), 0.0);
    EXPECT_DOUBLE_EQ(*a[3].as_number(), 1e-300);
    EXPECT_TRUE(Tools::parse_json("1e999").isError());
    EXPECT_TRUE(Tools::parse_json("-1e999").isError());
    EXPECT_TRUE(Tools::parse_json("0.0000001e320").isError());  // 1e313
    EXPECT_TRUE(Tools::parse_json("100000e-330").isValue());   // 1e-325 rounds to 0
}

TEST(test_Tools_json, invalid_utf8_is_rejected) {
    EXPECT_TRUE(Tools::parse_json("\"\xC3\"").isError());              // truncated sequence
    EXPECT_TRUE(Tools::parse_json("\"\x80\"").isError());              // lone continuation byte
    EXPECT_TRUE(Tools::parse_json("\"\xC0\xAF\"").isError());         // overlong '/'
    EXPECT_TRUE(Tools::parse_json("\"\xE0\x80\xAF\"").isError());     // overlong
    EXPECT_TRUE(Tools::parse_json("\"\xED\xA0\x80\"").isError());     // UTF-16 surrogate
    EXPECT_TRUE(Tools::parse_json("\"\xF4\x90\x80\x80\"").isError()); // above U+10FFFF
    EXPECT_TRUE(Tools::parse_json("\"\xFF\"").isError());
    auto json = Tools::parse_json("\"a\xC3(\"");
    ASSERT_TRUE(json.isError());
    EXPECT_EQ(json.error().text().rfind("line 1, column 3:", 0), 0u) << json.error();
    // largest valid code points of each length
    EXPECT_TRUE(Tools::parse_json("\"\xDF\xBF\xEF\xBF\xBF\xF4\x8F\xBF\xBF\"").isValue());
}

TEST(JsonWriter, RoundTrip) {
    Tools::Json::Object object {
        {"s", Tools::Json(std::string("a\"b\nc \xC3\xA9 \x01"))},
        {"a", Tools::Json(0.1)},
        {"b", Tools::Json(1e-300)},
        {"c", Tools::Json(-3.0)},
        {"arr", Tools::Json(Tools::Json::Array {Tools::Json(1.5), Tools::Json(std::string("x")), Tools::Json()})},
        {"t", Tools::Json(true)},
        {"n", Tools::Json()},
        {"o", Tools::Json(Tools::Json::Object {})},
    };
    Tools::Json const value(object);
    for (int indent : {0, 2}) {
        auto text = Tools::to_json_text(value, indent);
        ASSERT_TRUE(text.isValue()) << text.error();
        auto parsed = Tools::parse_json(text());
        ASSERT_TRUE(parsed.isValue()) << parsed.error();
        auto const root = parsed();
        ASSERT_NE(root.find("s")->as_string(), nullptr);
        EXPECT_EQ(*root.find("s")->as_string(), std::string("a\"b\nc \xC3\xA9 \x01"));
        EXPECT_EQ(*root.find("a")->as_number(), 0.1);
        EXPECT_EQ(*root.find("b")->as_number(), 1e-300);
        EXPECT_EQ(*root.find("c")->as_number(), -3.0);
        auto const* arr = root.find("arr")->as_array();
        ASSERT_EQ(arr->size(), 3u);
        EXPECT_EQ(*(*arr)[0].as_number(), 1.5);
        ASSERT_NE((*arr)[1].as_string(), nullptr);
        EXPECT_EQ(*(*arr)[1].as_string(), std::string("x"));
        EXPECT_TRUE((*arr)[2].is_null());
        EXPECT_EQ(root.find("t")->as_bool(), true);
        EXPECT_TRUE(root.find("n")->is_null());
        EXPECT_TRUE(root.find("o")->is_object());
    }
}

TEST(JsonWriter, NegativeIndentIsError) {
    EXPECT_TRUE(Tools::to_json_text(Tools::Json(), -1).isError());
}


TEST(Json, ProblemCarriesPosition) {
    auto const json = Tools::parse_json("[1,\n  2,");
    ASSERT_TRUE(json.isError());
    EXPECT_EQ(json.error().code(), "json.at");
    EXPECT_EQ(json.error().value("line")->plain, "2");
    EXPECT_EQ(json.error().value("reason")->nested->code(), "json.end");
}

}  // namespace test_Json
