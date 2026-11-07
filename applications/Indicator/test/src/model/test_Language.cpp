#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "model/Language.h"
#include "model/Message.h"

namespace test_Language {

namespace {

/// The {name}s of a text.
std::set<std::string> placeholders(std::string const& text) {
    std::set<std::string> names;
    std::regex const name(R"(\{(\w+)\})");
    for (std::sregex_iterator it(text.begin(), text.end(), name), end; it != end; ++it) {
        names.insert((*it)[1].str());
    }
    return names;
}

// U+2066 and U+2069 as UTF-8
std::string const lri = "\xE2\x81\xA6";
std::string const pdi = "\xE2\x81\xA9";

/// The text before the first '.'.
std::string group_of(std::string const& key) {
    return key.substr(0, key.find('.'));
}

/// The text after the last '.'.
std::string last_section(std::string const& key) {
    return key.substr(key.rfind('.') + 1);
}

}  // namespace

TEST(WxMessage, RendersPlaceholders) {
    add_strings("en", {{"t.hello", "Hello {who}"}});
    EXPECT_EQ(Message("t.hello").with("who", "Ana").english(), "Hello Ana");
}

TEST(WxMessage, UnknownPlaceholderStaysAsWritten) {
    add_strings("en", {{"t.unknown", "A {one} and {two}"}});
    EXPECT_EQ(Message("t.unknown").with("one", "1").english(), "A 1 and {two}");
}

TEST(WxMessage, NestedIsRenderedInTheSameLanguage) {
    add_strings("en", {{"t.outer", "Outer: {inner}"}, {"t.inner", "inner"}});
    add_strings("de", {{"t.outer", "Außen: {inner}"}, {"t.inner", "innen"}});
    Message m("t.outer");
    m.with("inner", Message("t.inner"));
    EXPECT_EQ(m.render("de"), "Außen: innen");
    EXPECT_EQ(m.english(), "Outer: inner");
    EXPECT_EQ(m.nested().size(), 1u);
}

TEST(WxMessage, FallsBackToEnglishThenKey) {
    add_strings("en", {{"t.only_en", "English only"}});
    EXPECT_EQ(Message("t.only_en").render("fr"), "English only");
    EXPECT_EQ(Message("t.nowhere").render("fr"), "t.nowhere");
    EXPECT_EQ(Message().render("fr"), "");
}

TEST(WxMessage, PlainValuesAreNotTranslated) {
    add_strings("en", {{"t.inner", "inner"}, {"t.wrap", "[{v}]"}});
    add_strings("ar", {{"t.wrap", "<{v}>"}});
    EXPECT_EQ(Message("t.wrap").with("v", "t.inner").english(), "[t.inner]");
    EXPECT_EQ(Message("t.wrap").with("v", "12.5 mm").render("ar"),
              "<" + lri + "12.5 mm" + pdi + ">");
    // an inserted value is not searched for placeholders again
    EXPECT_EQ(Message("t.wrap").with("v", "{v}").english(), "[{v}]");
}

TEST(WxMessage, ArabicIsolatesPlainValuesOnly) {
    add_strings("en", {{"t.path", "Opened {v}."}, {"t.out", "[{inner}]"}});
    add_strings("ar", {{"t.path", "تم فتح {v}."}, {"t.out", "{inner}"}});
    std::string const isolated = lri + "/tmp/a.zip" + pdi;
    EXPECT_EQ(Message("t.path").with("v", "/tmp/a.zip").render("ar"),
              std::string("تم فتح ") + isolated + ".");
    // a nested message is isolated only through its own plain values
    EXPECT_EQ(Message("t.out").with("inner", Message("t.path").with("v", "x")).render("ar"),
              std::string("تم فتح ") + lri + "x" + pdi + ".");
    for (char const* code : {"en", "de"}) {
        std::string const text = Message("t.path").with("v", "/tmp/a.zip").render(code);
        EXPECT_EQ(text.find(lri), std::string::npos) << code;
        EXPECT_EQ(text.find(pdi), std::string::npos) << code;
    }
}

TEST(WxLanguage, EveryKeyUsedInTheSourcesExists) {
    register_indicator_strings();
    auto const list = string_keys("en");
    std::set<std::string> const keys(list.begin(), list.end());
    std::set<std::string> groups;
    for (auto const& key : keys)
        groups.insert(group_of(key));
    // every string literal shaped like group.name where the group is one of the tables'
    std::regex const literal(R"re("([A-Za-z]+(?:\.[A-Za-z_]+)+)")re");
    std::set<std::string> const file_names {"json", "png", "jpg", "ini", "zip", "txt", "pt"};
    std::filesystem::path const source_dir = INDICATOR_SOURCE_DIR;
    ASSERT_TRUE(std::filesystem::is_directory(source_dir));
    int checked = 0;
    for (auto const& entry : std::filesystem::recursive_directory_iterator(source_dir)) {
        auto const extension = entry.path().extension();
        if (!entry.is_regular_file() || (extension != ".cpp" && extension != ".h"))
            continue;
        std::ifstream file(entry.path());
        ASSERT_TRUE(file.is_open()) << entry.path().string();
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string const source = buffer.str();
        for (std::sregex_iterator it(source.begin(), source.end(), literal), end; it != end; ++it) {
            std::string const key = (*it)[1].str();
            if (!groups.count(group_of(key)))
                continue;
            if (file_names.count(last_section(key)))
                continue;  // file names
            ++checked;
            EXPECT_TRUE(keys.count(key)) << entry.path().string() << ": " << key;
        }
    }
    EXPECT_GT(checked, 50);
}

TEST(WxLanguage, IndicatorTablesHaveParity) {
    register_indicator_strings();
    auto const english = string_keys("en");
    ASSERT_FALSE(english.empty());
    auto const without_test_keys = [](std::vector<std::string> const& all) {
        std::vector<std::string> out;
        for (auto const& key : all) {
            if (key.rfind("t.", 0) != 0)
                out.push_back(key);
        }
        return out;
    };
    auto const expected = without_test_keys(english);
    for (auto const& code : language_codes()) {
        EXPECT_EQ(without_test_keys(string_keys(code)), expected) << code;
        for (auto const& key : expected) {
            EXPECT_EQ(placeholders(lookup(code, key)), placeholders(lookup("en", key)))
                << code << " " << key;
        }
    }
}

TEST(WxLanguage, NamesInTheirOwnWords) {
    EXPECT_EQ(language_name("ar"), "العربية");
    EXPECT_EQ(language_name("de"), "Deutsch");
    EXPECT_EQ(language_codes(),
              (std::vector<std::string> {"en", "de", "es", "it", "fr", "nl", "ar"}));
}

}  // namespace test_Language
