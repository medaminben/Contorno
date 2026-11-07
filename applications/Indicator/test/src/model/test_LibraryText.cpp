#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>

#include <Tools/Json.h>
#include <Tools/Zip.h>
#include <Vision/Objects.h>

#include "model/Language.h"
#include "model/LibraryText.h"
#include "model/Message.h"

namespace test_LibraryText {

namespace {

bool contains(std::string const& text, std::string const& part) { return text.find(part) != std::string::npos; }

/// No part of the message, however deeply nested, is the raw fallback.
bool fully_known(Message const& message) {
    if (message.key() == "common.raw") return false;
    for (auto const& inner : message.nested()) {
        if (!fully_known(inner)) return false;
    }
    return true;
}

/// A C++ string literal's content as the compiler reads it (\" and \\ only, all the libraries use).
std::string unescaped(std::string const& literal) {
    std::string text;
    for (std::size_t i = 0; i < literal.size(); ++i) {
        if (literal[i] == '\\' && i + 1 < literal.size()) ++i;
        text += literal[i];
    }
    return text;
}

/// U+2066 and U+2069 as UTF-8: the isolates the right to left languages put around plain text.
void remove_isolates(std::string& text) {
    for (std::string const mark : {"\xE2\x81\xA6", "\xE2\x81\xA9"}) {
        for (auto at = text.find(mark); at != std::string::npos; at = text.find(mark)) text.erase(at, mark.size());
    }
}

}  // namespace

TEST(WxLibraryText, MissingZip) {
    register_indicator_strings();
    auto const result = Tools::read_zip("/nonexistent/x.zip");
    ASSERT_TRUE(result.isError());
    Message const message = from_library(result.error());
    EXPECT_EQ(message.key(), "lib.zip.cannot_open");
    EXPECT_TRUE(contains(message.render("de"), "/nonexistent/x.zip"));
    EXPECT_NE(message.render("de"), message.english());
    EXPECT_EQ(message.english(), result.error().text());
}

TEST(WxLibraryText, DamagedZip) {
    register_indicator_strings();
    auto const result = Tools::parse_zip("not a zip");
    ASSERT_TRUE(result.isError());
    Message const message = from_library(result.error());
    EXPECT_TRUE(fully_known(message));
    EXPECT_NE(message.render("fr"), message.english());
    EXPECT_EQ(message.english(), result.error().text());
}

TEST(WxLibraryText, BadJson) {
    register_indicator_strings();
    auto const result = Tools::parse_json("{\"a\": \"\\q\"}");
    ASSERT_TRUE(result.isError());
    Message const message = from_library(result.error());
    EXPECT_TRUE(fully_known(message));
    EXPECT_EQ(message.english(), result.error().text());
    std::string const french = message.render("fr");
    EXPECT_NE(french, message.english());
    EXPECT_EQ(french.rfind("ligne 1, colonne 8", 0), 0u) << french;
}

TEST(WxLibraryText, NoTorch) {
    register_indicator_strings();
    if (Vision::ai_available()) GTEST_SKIP();
    Vision::ObjectOptions options;
    options.model_path = "/nonexistent.pt";
    auto const result = Vision::find_objects(INDICATOR_SAMPLES_DIR "/leather/calf_black.jpg",
                                            Vision::lens_scale(Vision::LensCamera {}), options);
    ASSERT_TRUE(result.isError());
    EXPECT_TRUE(fully_known(from_library(result.error())));
}

TEST(WxLibraryText, UnreadableModel) {
    register_indicator_strings();
    if (!Vision::ai_available()) GTEST_SKIP();
    Vision::ObjectOptions options;
    options.model_path = "/nonexistent.pt";
    auto const result = Vision::find_objects(INDICATOR_SAMPLES_DIR "/leather/calf_black.jpg",
                                            Vision::lens_scale(Vision::LensCamera {}), options);
    ASSERT_TRUE(result.isError());
    Message const message = from_library(result.error());
    EXPECT_TRUE(fully_known(message));
    EXPECT_TRUE(contains(message.render("es"), "/nonexistent.pt"));
}

// a nested reason is a message of its own at every level
TEST(WxLibraryText, WrappedAtEveryLevel) {
    register_indicator_strings();
    Vision::ObjectOptions options;
    options.model_path = "/m.pt";
    auto const result = Vision::find_objects(INDICATOR_SAMPLES_DIR "/leather/calf_black.jpg", Vision::PixelScale {0, 1},
                                            options);
    ASSERT_TRUE(result.isError());
    Message const message = from_library(result.error());
    EXPECT_TRUE(fully_known(message));
    EXPECT_EQ(message.english(), "objects: camera: the scale must be positive and finite");
    EXPECT_NE(message.render("it"), message.english());
}

TEST(WxLibraryText, RawStaysAsItIs) {
    register_indicator_strings();
    for (auto const& code : language_codes()) {
        // the right to left languages isolate the plain text; nothing else changes
        std::string text = from_library(Core::Problem::raw("something new: 12 mm {x}")).render(code);
        remove_isolates(text);
        EXPECT_EQ(text, "something new: 12 mm {x}") << code;
    }
    EXPECT_EQ(from_library(Core::Problem::raw("a sentence nobody wrote")).key(), "common.raw");
}

TEST(WxLibraryText, OutOfMemoryIsTranslated) {
    register_indicator_strings();
    Message const message = from_library(Core::Problem::out_of_memory());
    EXPECT_EQ(message.english(), Core::Problem::out_of_memory().text());
    EXPECT_NE(message.render("de"), message.english());
}

// Every problem a library can report has its sentence in every language, and the
// English is the library's own sentence word for word.
TEST(WxLibraryText, EveryLibraryProblemHasItsSentence) {
    register_indicator_strings();
    std::regex const made(
        R"re(\b(?:Problem|wrapped|entry_problem|entry|path_problem|problem)\(\s*"([a-z_]+(?:\.[a-z_]+)*)",\s*"((?:[^"\\]|\\.)*)")re");
    int found = 0;
    namespace fs = std::filesystem;
    for (auto const& entry : fs::recursive_directory_iterator(CONTORNO_LIBRARIES_DIR)) {
        if (!entry.is_regular_file()) continue;
        std::string const ext = entry.path().extension().string();
        if (ext != ".cpp" && ext != ".h") continue;
        std::string const path = entry.path().generic_string();
        if (contains(path, "/test/")) continue;
        std::ifstream file(entry.path(), std::ios::binary);
        ASSERT_TRUE(file) << path;
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string const source = buffer.str();
        for (std::sregex_iterator it(source.begin(), source.end(), made), end; it != end; ++it) {
            std::string const code = (*it)[1].str();
            if (code == "raw" || code.rfind("test.", 0) == 0) continue;
            std::string const key = "lib." + code;
            std::string const english = unescaped((*it)[2].str());
            EXPECT_EQ(lookup("en", key), english) << key << " in " << path;
            for (auto const& language : language_codes()) {
                EXPECT_NE(lookup(language, key), key) << key << " missing in " << language;
            }
            ++found;
        }
    }
    EXPECT_GE(found, 70);
}

}  // namespace test_LibraryText
