#include <Tools/Tools.h>
#include <Tools/Zip.h>
#include <gtest/gtest.h>
#include <cstdint>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

namespace test_Zip {
namespace fs = std::filesystem;

std::string all_bytes() {
    std::string s;
    for (int i = 0; i < 256; ++i) s.push_back(static_cast<char>(i));
    return s;
}

std::string random_bytes(std::size_t n) {
    std::mt19937 rng(7);
    std::string s(n, '\0');
    for (auto& c : s) c = static_cast<char>(rng() & 0xFF);
    return s;
}

void expect_same(std::vector<Tools::ZipEntry> const& got, std::vector<Tools::ZipEntry> const& want) {
    ASSERT_EQ(got.size(), want.size());
    for (std::size_t i = 0; i < want.size(); ++i) {
        EXPECT_EQ(got[i].name, want[i].name);
        EXPECT_EQ(got[i].data, want[i].data) << want[i].name;
    }
}

TEST(Zip, RoundTrip) {
    std::vector<Tools::ZipEntry> const entries {
        {"empty.txt", ""}, {"bin/all.bin", all_bytes()}, {"photo.png", random_bytes(100000)}};
    auto bytes = Tools::to_zip(entries);
    ASSERT_FALSE(bytes.isError()) << bytes.error();
    auto parsed = Tools::parse_zip(bytes());
    ASSERT_FALSE(parsed.isError()) << parsed.error();
    expect_same(parsed(), entries);
}

TEST(Zip, FileRoundTrip) {
    auto const path = (fs::temp_directory_path() / "contorno_zip_round_trip.zip").string();
    std::vector<Tools::ZipEntry> const entries {{"a.json", "{}\n"}, {"b/c.dxf", "0\nEOF\n"}};
    auto written = Tools::write_zip(path, entries);
    ASSERT_FALSE(written.isError()) << written.error();
    auto read = Tools::read_zip(path);
    ASSERT_FALSE(read.isError()) << read.error();
    expect_same(read(), entries);
}

TEST(Zip, EmptyArchive) {
    auto bytes = Tools::to_zip({});
    ASSERT_FALSE(bytes.isError()) << bytes.error();
    auto parsed = Tools::parse_zip(bytes());
    ASSERT_FALSE(parsed.isError()) << parsed.error();
    EXPECT_TRUE(parsed().empty());
}

TEST(Zip, RejectsUnsafeNames) {
    for (std::string const name : {"", "/abs", "a/../b", "..", "a\\b"}) {
        EXPECT_TRUE(Tools::to_zip({{name, "x"}}).isError()) << name;
    }
    EXPECT_TRUE(Tools::to_zip({{"x", "1"}, {"x", "2"}}).isError());

    // The same archive with the names patched to "../x" (same length).
    auto bytes = Tools::to_zip({{"zz/x", "data"}});
    ASSERT_FALSE(bytes.isError()) << bytes.error();
    std::string patched = bytes();
    for (std::size_t at = patched.find("zz/x"); at != std::string::npos; at = patched.find("zz/x", at)) {
        patched.replace(at, 4, "../x");
    }
    auto parsed = Tools::parse_zip(patched);
    ASSERT_TRUE(parsed.isError());
    EXPECT_NE(parsed.error().text().find("../x"), std::string::npos) << parsed.error();
}

TEST(Zip, CrcMismatch) {
    auto bytes = Tools::to_zip({{"a.txt", "hello world"}});
    ASSERT_FALSE(bytes.isError()) << bytes.error();
    std::string broken = bytes();
    broken[broken.find("hello")] = 'j';
    auto parsed = Tools::parse_zip(broken);
    ASSERT_TRUE(parsed.isError());
    EXPECT_NE(parsed.error().text().find("CRC"), std::string::npos) << parsed.error();
}

TEST(Zip, Truncated) {
    auto bytes = Tools::to_zip({{"a.txt", "hello"}, {"b.txt", all_bytes()}});
    ASSERT_FALSE(bytes.isError()) << bytes.error();
    for (std::size_t n = 0; n < bytes().size(); ++n) {
        EXPECT_TRUE(Tools::parse_zip(std::string_view(bytes()).substr(0, n)).isError()) << n;
    }
}

TEST(Zip, ReadsPythonStored) {
    auto read = Tools::read_zip("../data/contorno_tools/python_stored.zip");
    ASSERT_FALSE(read.isError()) << read.error();
    expect_same(read(), {{"a.txt", "hello\n"}, {"dir/b.bin", all_bytes()}});
}

TEST(Zip, RejectsCompressed) {
    auto read = Tools::read_zip("../data/contorno_tools/python_deflated.zip");
    ASSERT_TRUE(read.isError());
    EXPECT_NE(read.error().text().find("a.txt"), std::string::npos) << read.error();
    EXPECT_NE(read.error().text().find("method 8"), std::string::npos) << read.error();
}

TEST(Zip, MissingFile) {
    auto read = Tools::read_zip("../data/contorno_tools/no_such_file.zip");
    ASSERT_TRUE(read.isError());
    EXPECT_NE(read.error().text().find("no_such_file.zip"), std::string::npos) << read.error();
}


// the error names its sentence by code and carries its values, so an application can word it
TEST(Zip, ProblemCarriesCodeAndValues) {
    auto const bytes = Tools::to_zip({{"a.txt", "x"}, {"a.txt", "y"}});
    ASSERT_TRUE(bytes.isError());
    EXPECT_EQ(bytes.error().code(), "zip.failed");
    auto const* reason = bytes.error().value("reason");
    ASSERT_TRUE(reason != nullptr && reason->nested);
    EXPECT_EQ(reason->nested->code(), "zip.entry_twice");
    EXPECT_EQ(reason->nested->value("name")->plain, "a.txt");
    EXPECT_EQ(bytes.error().text(), "zip: entry \"a.txt\" appears twice");
}

}  // namespace test_Zip
