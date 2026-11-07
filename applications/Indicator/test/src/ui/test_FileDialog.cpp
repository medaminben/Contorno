#include <gtest/gtest.h>

#include <wx/button.h>
#include <wx/filectrl.h>
#include <wx/generic/filectrlg.h>
#include <wx/image.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "model/Language.h"
#include "model/Utf8Path.h"
#include "ui/Text.h"
#include "ui/Theme.h"
#include "ui/WxTestApp.h"

#include <filesystem>
#include <fstream>

#include "TempDir.h"

namespace test_FileDialog {

TEST(WxFileDialog, StartsInTheFolderIconsGreen) {
    wxString folder;
    int green_icons = 0, unnamed_tools = 0;
    drive_modal([&](wxDialog& dialog) {
        auto* files = find_descendant<wxGenericFileCtrl>(&dialog);
        if (files) folder = files->GetShownDirectory();
        std::vector<Key*> keys;
        collect_descendants(&dialog, keys);
        for (Key* key : keys) {
            wxBitmapBundle const icon = key->icon();
            if (!icon.IsOk()) continue;
            if (key->GetToolTipText().empty()) ++unnamed_tools;
            wxImage const image = icon.GetBitmap(wxSize(20, 20)).ConvertToImage();
            bool green = false;
            for (int y = 0; y < image.GetHeight() && !green; ++y) {
                for (int x = 0; x < image.GetWidth() && !green; ++x) {
                    bool const opaque = !image.HasAlpha() || image.GetAlpha(x, y) == 255;
                    green = opaque && image.GetGreen(x, y) > 200 && image.GetRed(x, y) < 120 && image.GetBlue(x, y) < 180;
                }
            }
            if (green) ++green_icons;
        }
        dialog.EndModal(wxID_CANCEL);
    });
    EXPECT_TRUE(choose_open_file(nullptr, "Import photo", INDICATOR_SAMPLES_DIR, "Images (*.jpg)").empty());
    EXPECT_EQ(folder, wxString(INDICATOR_SAMPLES_DIR));
    EXPECT_EQ(unnamed_tools, 0);  // every icon-only key has a tooltip
    EXPECT_GE(green_icons, 3);  // back, forward, up, new folder, list, details
}

TEST(WxFileDialog, SaveAddsTheSuffix) {
    wxString const folder = wx(utf8(std::filesystem::temp_directory_path()));
    drive_modal([&](wxDialog& dialog) {
        auto* name = find_descendant<wxTextCtrl>(&dialog);
        if (name) name->SetValue("contorno-suffix-test");
        wxCommandEvent accept(wxEVT_BUTTON, wxID_OK);
        dialog.GetEventHandler()->ProcessEvent(accept);
    });
    wxString const path = choose_save_file(nullptr, "Save", folder + "/a.zip", "Inspections (*.zip)", "zip");
    EXPECT_EQ(path, folder + "/contorno-suffix-test.zip");
}

namespace {
/// Saves over `existing`: the confirmation is answered with `answer`; the dialog is then cancelled if still open.
wxString save_over(std::filesystem::path const& existing, int answer, bool* asked) {
    drive_modal([=](wxDialog& dialog) {
        drive_modal(
            [=](wxDialog& confirm) {
                *asked = true;
                wxCommandEvent press(wxEVT_BUTTON, answer);
                confirm.GetEventHandler()->ProcessEvent(press);
            },
            std::chrono::milliseconds(5000), "contorno-confirm");
        find_descendant<wxTextCtrl>(&dialog)->SetValue(wx(utf8(existing.filename())));
        wxCommandEvent accept(wxEVT_BUTTON, wxID_OK);
        dialog.GetEventHandler()->ProcessEvent(accept);
        if (dialog.IsModal()) dialog.EndModal(wxID_CANCEL);  // answered no: still open
    });
    return choose_save_file(nullptr, "Save", wx(utf8(existing)), "Inspections (*.zip)", "zip");
}
}  // namespace

TEST(WxFileDialog, SaveAsksBeforeOverwriting) {
    TempDir dir;
    auto const existing = dir.path / "taken.zip";
    std::ofstream(existing) << "x";
    bool asked = false;
    EXPECT_TRUE(save_over(existing, wxID_NO, &asked).empty());
    EXPECT_TRUE(asked);
    asked = false;
    EXPECT_EQ(save_over(existing, wxID_YES, &asked), wx(utf8(existing)));
    EXPECT_TRUE(asked);
}

TEST(WxFileDialog, FolderNamesUseTheFieldText) {
    set_light_theme(false);
    TempDir dir;
    std::filesystem::create_directory(dir.path / "a-folder");
    std::ofstream(dir.path / "a-file.txt") << "x";
    std::filesystem::create_directory(dir.path / "sub");

    wxColour opened, after_up, after_report, file_colour;
    bool saw_opened = false, saw_up = false, saw_file = false, saw_report = false;
    drive_modal([&](wxDialog& dialog) {
        auto* files = find_descendant<wxGenericFileCtrl>(&dialog);
        auto* list = files ? files->GetFileList() : nullptr;
        auto colour_of = [&](wxString const& name, wxColour* colour, bool* saw) {
            if (!list) return;
            for (long row = 0; row < list->GetItemCount(); ++row) {
                auto* data = static_cast<wxFileData*>(wxUIntToPtr(list->GetItemData(row)));
                if (!data || data->GetFileName() != name) continue;
                *colour = list->GetItemTextColour(row);
                *saw = true;
                return;
            }
        };
        if (list) {
            colour_of("..", &opened, &saw_opened);
            files->GoToParentDir();
            colour_of("a-folder", &after_up, &saw_up);
            colour_of("a-file.txt", &file_colour, &saw_file);
            files->ChangeToReportMode();
            colour_of("a-folder", &after_report, &saw_report);
        }
        dialog.EndModal(wxID_CANCEL);
    });
    EXPECT_TRUE(choose_open_file(nullptr, "Import", wx(utf8(dir.path / "sub")), "All (*)|*").empty());
    wxColour const ink = palette(false).field_text;
    EXPECT_TRUE(saw_opened);
    EXPECT_EQ(opened, ink);  // ".." is a directory row
    EXPECT_TRUE(saw_up);
    EXPECT_EQ(after_up, ink);  // the up key rebuilds the list
    EXPECT_TRUE(saw_file);
    EXPECT_FALSE(file_colour.IsOk());  // a file keeps the list's foreground
    EXPECT_TRUE(saw_report);
    EXPECT_EQ(after_report, ink);  // the details view rebuilds the list
}

TEST(WxFileDialog, LabelsFollowTheLanguage) {
    set_current_language("de");
    wxString expected = wx(lookup("de", "file.lookIn"));
    wxString found;
    drive_modal([&](wxDialog& dialog) {
        std::vector<wxStaticText*> labels;
        collect_descendants(&dialog, labels);
        for (auto* label : labels)
            if (label->GetLabel() == expected) found = label->GetLabel();
        dialog.EndModal(wxID_CANCEL);
    });
    choose_open_file(nullptr, "Import", INDICATOR_SAMPLES_DIR, "Images (*.jpg)");
    set_current_language("en");
    EXPECT_NE(expected, wx(lookup("en", "file.lookIn")));
    EXPECT_EQ(found, expected);
}

TEST(WxFileDialog, WarnShowsTheMessageAndCloses) {
    wxString title, message;
    drive_modal([&](wxDialog& dialog) {
        title = dialog.GetTitle();
        std::vector<wxStaticText*> labels;
        collect_descendants(&dialog, labels);
        for (auto* label : labels) message += label->GetLabel();
        dialog.EndModal(wxID_OK);
    });
    warn(nullptr, "Careful", "Something happened");
    EXPECT_EQ(title, "Careful");
    EXPECT_NE(message.Find("Something happened"), wxNOT_FOUND);
}

}  // namespace test_FileDialog
