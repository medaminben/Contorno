#include "model/DataFolder.h"
#include "model/Utf8Path.h"

#include "TempDir.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

void write(fs::path const& path, std::string const& text) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    ASSERT_TRUE(file.is_open());
    file << text;
}

std::string read(fs::path const& path) {
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), {});
}

}  // namespace

TEST(WxDataFolder, DocumentsWhenTheInstallerRecordedNone) {
    TempDir dir;
    EXPECT_EQ(data_folder(dir.path / "bin", dir.path / "Documents"),
              dir.path / "Documents" / "Contorno");
}

TEST(WxDataFolder, TheFolderTheInstallerRecorded) {
    TempDir dir;
    fs::path const chosen = dir.path / "Données de mesure";
    // as Inno Setup writes it: UTF-8 with a byte order mark and a line end
    write(dir.path / "share/contorno/data-folder", "\xEF\xBB\xBF" + utf8(chosen) + "\r\n");
    EXPECT_EQ(utf8(data_folder(dir.path / "bin", dir.path / "Documents")), utf8(chosen));
}

TEST(WxDataFolder, AnEmptyRecordFallsBackToDocuments) {
    TempDir dir;
    write(dir.path / "share/contorno/data-folder", "\r\n");
    EXPECT_EQ(data_folder(dir.path / "bin", dir.path / "Documents"),
              dir.path / "Documents" / "Contorno");
}

TEST(WxDataFolder, FillCopiesTheInstalledFolders) {
    TempDir dir;
    fs::path const share = dir.path / "share/contorno";
    write(share / "samples/leather/calf.jpg", "calf");
    write(share / "manual/en/index.html", "manual");
    write(share / "articles/a/a.html", "article");
    write(share / "models/isnet-general-use.pt", "model");
    fs::path const data = dir.path / "Documents/Contorno";
    std::string reason;
    EXPECT_TRUE(fill_data_folder(dir.path / "bin", data, reason)) << reason;
    EXPECT_EQ(read(data / "samples/leather/calf.jpg"), "calf");
    EXPECT_EQ(read(data / "manual/en/index.html"), "manual");
    EXPECT_EQ(read(data / "articles/a/a.html"), "article");
    EXPECT_FALSE(fs::exists(data / "models"));  // the model stays with the program
}

TEST(WxDataFolder, FillLeavesAFolderTheUserHasAlone) {
    TempDir dir;
    write(dir.path / "share/contorno/samples/calf.jpg", "installed");
    write(dir.path / "share/contorno/manual/en/index.html", "manual");
    fs::path const data = dir.path / "Contorno";
    write(data / "samples/calf.jpg", "the user's");
    std::string reason;
    EXPECT_TRUE(fill_data_folder(dir.path / "bin", data, reason)) << reason;
    EXPECT_EQ(read(data / "samples/calf.jpg"), "the user's");
    EXPECT_EQ(read(data / "manual/en/index.html"), "manual");  // the missing one is still added
}

TEST(WxDataFolder, FillDoesNothingWithoutAnInstalledCopy) {
    TempDir dir;
    fs::path const data = dir.path / "Contorno";
    std::string reason;
    EXPECT_TRUE(fill_data_folder(dir.path / "bin", data, reason)) << reason;
    EXPECT_FALSE(fs::exists(data));
}

TEST(WxDataFolder, FillReportsAFolderItCannotMake) {
    TempDir dir;
    write(dir.path / "share/contorno/samples/calf.jpg", "calf");
    write(dir.path / "blocked", "a file where the folder should be");
    std::string reason;
    EXPECT_FALSE(fill_data_folder(dir.path / "bin", dir.path / "blocked/Contorno", reason));
    EXPECT_FALSE(reason.empty());
}
