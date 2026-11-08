#include <gtest/gtest.h>

#include <wx/bitmap.h>
#include <wx/combobox.h>
#include <wx/frame.h>
#include <wx/gbsizer.h>
#include <wx/image.h>
#include <wx/radiobut.h>
#include <wx/stattext.h>
#include <wx/timer.h>
#include <wx/textctrl.h>

#include "TempDir.h"
#include "model/Language.h"
#include "model/Message.h"
#include "model/Utf8Path.h"
#include "ui/CameraDialog.h"
#include "ui/Localise.h"
#include "ui/Text.h"
#include "ui/Theme.h"
#include "ui/WxTestApp.h"

#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

#ifdef INDICATOR_GTK3
#include <gtk/gtk.h>
#endif

namespace test_SetUpDialog {

namespace {

using namespace std::chrono_literals;

/// Back to English and the dark theme when a test ends, whatever it did.
struct Restore {
    ~Restore() {
        apply_language("en");
        set_light_theme(false);
    }
};

template <class T>
T* named(wxWindow& dialog, wxString const& name) {
    return dynamic_cast<T*>(wxWindow::FindWindowByName(name, &dialog));
}

/// The grid row of a child of the dialog (-1 when it is not in the grid).
int grid_row(CameraDialog& dialog, wxString const& name) {
    auto* grid = dynamic_cast<wxGridBagSizer*>(
        dialog.GetSizer()->GetItem(static_cast<size_t>(0))->GetSizer());
    wxWindow* widget = wxWindow::FindWindowByName(name, &dialog);
    if (!grid || !widget)
        return -1;
    wxGBPosition const at = grid->GetItemPosition(widget);
    return at.GetRow();
}

wxRadioButton* radio(CameraDialog& dialog, wxString const& text) {
    std::vector<wxRadioButton*> all;
    collect_descendants<wxRadioButton>(&dialog, all);
    for (auto* r : all) {
        if (r->GetLabel() == text)
            return r;
    }
    return nullptr;
}

/// The user's choice of a radio button: the value and the event the toolkit sends.
void choose(wxRadioButton* button) {
    button->SetValue(true);
    wxCommandEvent event(wxEVT_RADIOBUTTON, button->GetId());
    event.SetEventObject(button);
    button->GetEventHandler()->ProcessEvent(event);
}

/// The user's choice of entry `index` of a combo box: the selection and the event the toolkit
/// sends.
void pick(wxComboBox* box, int index) {
    box->SetSelection(index);
    wxCommandEvent event(wxEVT_COMBOBOX, box->GetId());
    event.SetEventObject(box);
    event.SetInt(index);
    event.SetString(box->GetString(static_cast<unsigned>(index)));
    box->GetEventHandler()->ProcessEvent(event);
}

/// Ends `dialog`'s modal loop with wxID_ABORT after 4 s, so a key that does nothing fails a test
/// instead of hanging it.
struct Watchdog : wxEvtHandler {
    wxDialog& dialog;
    wxTimer timer;
    explicit Watchdog(wxDialog& d) : dialog(d), timer(this) {
        Bind(wxEVT_TIMER, [this](wxTimerEvent&) {
            if (dialog.IsModal())
                dialog.EndModal(wxID_ABORT);
        });
        timer.Start(4000, true);
    }
};
int show_modal(wxDialog& dialog) {
    Watchdog watchdog(dialog);
    return dialog.ShowModal();
}

Key* key(CameraDialog& dialog, wxString const& name) {
    return named<Key>(dialog, name);
}
/// The OK or Cancel key.
Key* button(CameraDialog& dialog, wxWindowID id) {
    return dynamic_cast<Key*>(dialog.FindWindow(id));
}

Vision::CameraSettings pixel_camera(double w, double h) {
    Vision::CameraSettings c {};
    c.input = Vision::SensorInput::pixel;
    c.pixel_width_um = w;
    c.pixel_height_um = h;
    return c;
}

Vision::CameraSettings sensor_camera() {
    Vision::CameraSettings c {};
    c.input = Vision::SensorInput::sensor;
    return c;
}

std::string label_of(CameraDialog& dialog, wxString const& name) {
    auto* label = named<wxStaticText>(dialog, name);
    return label ? utf8(label->GetLabel()) : std::string("<missing>");
}

bool contains(std::string const& text, std::string const& part) {
    return text.find(part) != std::string::npos;
}

/// The colour of a round key's border at the middle of its left side, as drawn.
wxColour left_border(Key* key) {
    wxImage const image = key->Render(1.0).ConvertToImage();
    int const y = image.GetHeight() / 2;
    return wxColour(image.GetRed(1, y), image.GetGreen(1, y), image.GetBlue(1, y));
}
bool is_green(wxColour const& c) {
    return c.Green() > 200 && c.Red() < 120 && c.Blue() < 180;
}

/// A key press as the toolkit sends it first: the hook event, to the window that has the focus.
void send_key(wxWindow* target, int code, wxEventType type = wxEVT_CHAR_HOOK) {
    wxKeyEvent event(type);
    event.m_keyCode = code;
    event.SetId(target->GetId());
    event.SetEventObject(target);
    target->GetEventHandler()->ProcessEvent(event);
}

void send_focus_lost(wxWindow* target) {
    wxFocusEvent event(wxEVT_KILL_FOCUS, target->GetId());
    event.SetEventObject(target);
    target->GetEventHandler()->ProcessEvent(event);
}

void wheel(wxWindow* target, int rotation) {
    wxMouseEvent event(wxEVT_MOUSEWHEEL);
    event.m_wheelRotation = rotation;
    event.m_wheelDelta = 120;
    event.SetEventObject(target);
    target->GetEventHandler()->ProcessEvent(event);
}

/// Types `text` into `box` one character at a time, as the keyboard would.
void type(wxComboBox* box, std::string const& text) {
    for (char c : text)
        box->SetValue(box->GetValue() + wxString(1, c));
}

std::string hex(wxColour const& c) {
    return utf8(c.GetAsString(wxC2S_HTML_SYNTAX));
}

void collect_all(wxWindow* window, std::vector<wxWindow*>& out) {
    for (wxWindow* child : window->GetChildren()) {
        out.push_back(child);
        collect_all(child, out);
    }
}

}  // namespace

TEST(WxSetUpDialog, TitleAndGrid) {
    CameraDialog dialog(nullptr, Vision::CameraSettings {}, {}, false, "en");
    EXPECT_EQ(dialog.GetTitle(), "Set Up");
    for (char const* name : {"width", "height", "link", "distance", "focal", "covered", "scale"}) {
        EXPECT_NE(wxWindow::FindWindowByName(name, &dialog), nullptr) << name;
    }
    int const row = grid_row(dialog, "width");
    EXPECT_GE(row, 0);
    EXPECT_EQ(grid_row(dialog, "height"), row);
    EXPECT_EQ(grid_row(dialog, "link"), row);
    EXPECT_EQ(grid_row(dialog, "distance"), row + 1);
    EXPECT_EQ(grid_row(dialog, "focal"), row + 1);
    EXPECT_EQ(grid_row(dialog, "covered"), row + 2);
    EXPECT_EQ(grid_row(dialog, "scale"), row + 2);
    EXPECT_TRUE(key(dialog, "link")->icon().IsOk());
}

// Qt's dialog (Breeze): 10 px margins, 6 px between the rows, the columns and the parts, and the
// four columns of labels and fields equally wide when there is room, as at the dialog's own size,
// in every language
TEST(WxSetUpDialog, SpacedAsQts) {
    Restore restore;
    for (std::string const& code : language_codes()) {
        apply_language(code);
        CameraDialog dialog(nullptr, Vision::CameraSettings {}, {}, false, code);
        int const margin = dialog.FromDIP(10), spacing = dialog.FromDIP(6);
        auto* grid = dynamic_cast<wxGridBagSizer*>(
            dialog.GetSizer()->GetItem(static_cast<size_t>(0))->GetSizer());
        ASSERT_NE(grid, nullptr);
        auto at = [&](int row, int col) {
            return grid->FindItemAtPosition(wxGBPosition(row, col))->GetWindow();
        };
        // every label and field of the two columns pairs as wide as the others
        int const column = at(1, 1)->GetSize().x;
        for (int row : {1, 2}) {
            for (int col : {0, 1, 3, 4})
                EXPECT_EQ(at(row, col)->GetSize().x, column) << code << " " << row << "," << col;
        }
        EXPECT_EQ(at(0, 0)->GetSize().x, column) << code;
        // the sizer's own positions, left to right whatever the direction
        auto left = [&](int row, int col) {
            return grid->FindItemAtPosition(wxGBPosition(row, col))->GetPosition().x;
        };
        EXPECT_EQ(left(1, 0), margin) << code;
        EXPECT_EQ(left(1, 1), margin + column + spacing) << code;
        EXPECT_EQ(left(1, 2), margin + 2 * (column + spacing)) << code;
        EXPECT_EQ(left(1, 3), left(1, 2) + at(1, 2)->GetSize().x + spacing) << code;
        EXPECT_EQ(left(1, 4), left(1, 3) + column + spacing) << code;
        EXPECT_EQ(dialog.GetClientSize().x - (left(1, 4) + column), margin) << code;
        wxWindow* covered = named<wxWindow>(dialog, "covered");
        Key *flag = key(dialog, "language_en"), *ok = button(dialog, wxID_OK);
        EXPECT_EQ(flag->GetPosition().y, covered->GetRect().GetBottom() + 1 + spacing) << code;
        EXPECT_EQ(ok->GetPosition().y, flag->GetRect().GetBottom() + 1 + spacing) << code;
        EXPECT_EQ(dialog.GetClientSize().y - ok->GetRect().GetBottom() - 1, margin) << code;
    }
}

TEST(WxSetUpDialog, LanguageThemeAndHelpKeysAreLive) {
    CameraDialog dialog(nullptr, Vision::CameraSettings {}, {}, false, "en");
    std::vector<wxString> names;
    for (char const* code : {"en", "de", "es", "it", "fr", "nl", "ar"})
        names.push_back(wxString("language_") + code);
    names.push_back("theme");
    names.push_back("help");
    // one row, in this order, right under the grid; the stretch keeps it there
    // when the dialog grows, above OK / Cancel
    wxSizer* column = dialog.GetSizer();
    ASSERT_NE(column, nullptr);
    ASSERT_EQ(column->GetItemCount(), 5u);  // the grid, a gap, the row, the stretch, OK / Cancel
    EXPECT_TRUE(column->GetItem(static_cast<size_t>(1))->IsSpacer());
    EXPECT_TRUE(column->GetItem(static_cast<size_t>(3))->IsSpacer());
    EXPECT_GT(column->GetItem(static_cast<size_t>(3))->GetProportion(), 0);
    EXPECT_NE(dynamic_cast<wxGridBagSizer*>(column->GetItem(static_cast<size_t>(0))->GetSizer()),
              nullptr);
    wxSizer* row = column->GetItem(static_cast<size_t>(2))->GetSizer();
    ASSERT_NE(row, nullptr);
    auto index_of = [&](wxWindow* window) {
        int at = 0;
        for (auto* item : row->GetChildren()) {
            if (item->GetWindow() == window)
                return at;
            ++at;
        }
        return -1;
    };
    int previous = -1;
    for (wxString const& name : names) {
        Key* k = key(dialog, name);
        ASSERT_NE(k, nullptr) << utf8(name);
        int const at = index_of(k);
        EXPECT_GT(at, previous) << utf8(name);
        previous = at;
        EXPECT_TRUE(k->IsEnabled()) << utf8(name);
        EXPECT_TRUE(k->icon().IsOk()) << utf8(name);
        EXPECT_FALSE(k->GetToolTipText().empty()) << utf8(name);
        EXPECT_FALSE(k->GetToolTipText().Contains("not available yet")) << utf8(name);
        EXPECT_EQ(k->GetSize(), wxSize(34, 34)) << utf8(name);
        EXPECT_TRUE(k->IsRound()) << utf8(name);
    }
}

// Qt's help key shows "?" in Arabic too; GTK's mirrored context for a right-to-left key drew it as
// "؟"
TEST(WxSetUpDialog, HelpKeyShowsAQuestionMarkInArabic) {
    Restore restore;
    apply_language("ar");
    CameraDialog dialog(nullptr, Vision::CameraSettings {}, {}, false, "ar");
    Key* help = key(dialog, "help");
    ASSERT_NE(help, nullptr);
    ASSERT_EQ(help->GetLayoutDirection(), wxLayout_RightToLeft);
    dialog.Show();
    wxRect const face(wxPoint(0, 0), help->GetSize());
    Ink const shown = ink_on_screen(*help, face);
    if (!shown.read)
        GTEST_SKIP() << "the screen cannot be read here";
    ASSERT_GT(shown.count, 0);
    wxImage const drawn = help->Render(help->GetContentScaleFactor()).ConvertToImage();
    Ink const question =
        ink_of(drawn, wxRect(drawn.GetSize()));  // the icon's "?", as the key draws it
    EXPECT_LT(ink_difference(shown, question), 0.15);
    dialog.Hide();
}

TEST(WxSetUpDialog, HelpNamesTheManualInTheDialogsLanguage) {
    CameraDialog dialog(nullptr, Vision::CameraSettings {}, {}, false, "en");
    Key* help = key(dialog, "help");
    ASSERT_NE(help, nullptr);
    EXPECT_EQ(utf8(help->GetToolTipText()), Message("setup.help").render("en"));
    key(dialog, "language_fr")->Click();
    EXPECT_EQ(utf8(help->GetToolTipText()), Message("setup.help").render("fr"));
    EXPECT_NE(Message("setup.help").render("en"), Message("setup.help").render("fr"));
}

#ifdef __linux__
// A stand-in browser first on the PATH writes the arguments it was started with.
TEST(WxSetUpDialog, HelpOpensTheManualInTheDialogsLanguage) {
    TempDir bin;
    std::filesystem::path const record = bin.path / "arguments.txt";
    std::filesystem::path const browser = bin.path / "chromium";
    {
        std::ofstream script(browser);
        script << "#!/bin/sh\nprintf '%s\\n' \"$@\" > '" << record.string() << ".part' && mv '"
               << record.string() << ".part' '" << record.string() << "'\n";
    }
    std::filesystem::permissions(browser, std::filesystem::perms::owner_all);
    std::string const old_path = std::getenv("PATH") ? std::getenv("PATH") : "";
    setenv("PATH", (bin.path.string() + ":" + old_path).c_str(), 1);
    {
        CameraDialog dialog(nullptr, Vision::CameraSettings {}, {}, false, "en");
        key(dialog, "language_fr")->Click();
        key(dialog, "help")->Click();
    }
    setenv("PATH", old_path.c_str(), 1);
    ASSERT_TRUE(pump_until([&] { return std::filesystem::exists(record); }, 10s))
        << "the browser was not started";
    std::ifstream in(record);
    std::vector<std::string> args;
    for (std::string line; std::getline(in, line);) {
        if (!line.empty())
            args.push_back(line);
    }
    std::string const expected =
        std::string("--app=file://") + INDICATOR_MANUAL_DIR + "/fr/index.html";
    EXPECT_NE(std::find(args.begin(), args.end(), expected), args.end()) << expected;
    EXPECT_TRUE(std::any_of(args.begin(), args.end(), [](std::string const& a) {
        return a.rfind("--user-data-dir=", 0) == 0;
    }));
}
#endif

TEST(WxSetUpDialog, FlagsAreExclusive) {
    CameraDialog dialog(nullptr, Vision::CameraSettings {}, {}, false, "en");
    auto const codes = language_codes();
    ASSERT_EQ(codes.size(), 7u);
    auto check = [&](std::string const& pressed) {
        for (auto const& code : codes) {
            Key* flag = key(dialog, "language_" + wx(code));
            ASSERT_NE(flag, nullptr) << code;
            EXPECT_TRUE(flag->IsEnabled()) << code;
            EXPECT_EQ(flag->IsChecked(), code == pressed) << code;
        }
    };
    check("en");
    EXPECT_EQ(dialog.language(), "en");
    key(dialog, "language_fr")->Click();
    check("fr");
    EXPECT_EQ(dialog.language(), "fr");
}

TEST(WxSetUpDialog, PressedFlagHasGreenBorder) {
    CameraDialog dialog(nullptr, Vision::CameraSettings {}, {}, false, "en");
    Key* en = key(dialog, "language_en");
    Key* fr = key(dialog, "language_fr");
    EXPECT_TRUE(is_green(left_border(en))) << hex(left_border(en));
    EXPECT_FALSE(is_green(left_border(fr))) << hex(left_border(fr));
    fr->Click();
    EXPECT_TRUE(is_green(left_border(fr))) << hex(left_border(fr));
    EXPECT_FALSE(is_green(left_border(en))) << hex(left_border(en));
}

TEST(WxSetUpDialog, ThemeKeyIsOnForLightPanels) {
    Restore restore;
    CameraDialog dark(nullptr, Vision::CameraSettings {}, {}, false, "en");
    Key* theme = key(dark, "theme");
    ASSERT_NE(theme, nullptr);
    EXPECT_FALSE(dark.light());
    EXPECT_EQ(utf8(theme->GetToolTipText()), "Light or dark theme");
    auto pixels = [&] {
        wxImage image = theme->icon().GetBitmap(wxSize(20, 20)).ConvertToImage();
        return std::string(reinterpret_cast<char const*>(image.GetAlpha()), 400);
    };
    std::string const moon = pixels();
    theme->Click();
    EXPECT_TRUE(dark.light());
    EXPECT_TRUE(is_green(left_border(theme)));
    EXPECT_NE(pixels(), moon);  // the sun
    theme->Click();
    EXPECT_FALSE(dark.light());
    EXPECT_FALSE(is_green(left_border(theme)));
    EXPECT_EQ(pixels(), moon);
    CameraDialog light(nullptr, Vision::CameraSettings {}, {}, false, "en", true);
    EXPECT_TRUE(light.light());
    EXPECT_TRUE(key(light, "theme")->IsChecked());
    EXPECT_NE(
        std::string(
            reinterpret_cast<char const*>(
                key(light, "theme")->icon().GetBitmap(wxSize(20, 20)).ConvertToImage().GetAlpha()),
            400),
        moon);
    // the flags stay exclusive among themselves: the theme key is not one of them
    EXPECT_TRUE(key(light, "language_en")->IsChecked());
}

TEST(WxSetUpDialog, DialogPanelsFollowTheTheme) {
    Restore restore;
    Palette const dark = palette(false), light = palette(true);
    // Set Up: opened light, and the Theme key previews the other one in it
    CameraDialog dialog(nullptr, Vision::CameraSettings {}, {}, false, "en", true);
    auto* entry = named<NumberField>(dialog, "distance")->entry();
    auto* width = named<wxComboBox>(dialog, "width");
    auto* covered = named<wxStaticText>(dialog, "covered");
    ASSERT_NE(width, nullptr);
    auto expect_theme = [&](Palette const& p, bool is_light) {
        EXPECT_EQ(hex(dialog.GetBackgroundColour()), hex(p.panel));
        EXPECT_EQ(hex(entry->GetBackgroundColour()), hex(p.field));
        EXPECT_EQ(hex(entry->GetForegroundColour()), hex(p.field_text));
        EXPECT_EQ(hex(width->GetBackgroundColour()), hex(p.field));
        EXPECT_EQ(hex(named<wxStaticText>(dialog, "scale")->GetBackgroundColour()), hex(p.panel));
        EXPECT_EQ(hex(radio(dialog, "Pixel size")->GetForegroundColour()), hex(p.text));
        // the result lines keep their own green
        EXPECT_EQ(hex(covered->GetForegroundColour()), hex(wxColour(result_colour(is_light))));
        EXPECT_EQ(hex(named<wxStaticText>(dialog, "scale")->GetForegroundColour()),
                  hex(wxColour(result_colour(is_light))));
    };
    expect_theme(light, true);
    // a window of its own, light: the preview in the dialog does not restyle it
    wxDialog other(nullptr, wxID_ANY, "other");
    auto* field = new wxTextCtrl(&other, wxID_ANY, "x");
    apply_theme(&other, true);
    key(dialog, "theme")->Click();
    expect_theme(dark, false);
    EXPECT_FALSE(light_theme());  // previewed only
    EXPECT_EQ(hex(other.GetBackgroundColour()), hex(light.panel));
    EXPECT_EQ(hex(field->GetBackgroundColour()), hex(light.field));
    key(dialog, "theme")->Click();
    expect_theme(light, true);
}

TEST(WxSetUpDialog, NumbersKeepEnglishFormat) {
    Restore restore;
    // a locale that writes the decimal comma, so that a locale-dependent format would show
    char const* const old = std::setlocale(LC_NUMERIC, nullptr);
    std::string const saved = old ? old : "C";
    ASSERT_NE(std::setlocale(LC_NUMERIC, "de_DE.utf8"), nullptr) << "de_DE.utf8 is not installed";
    char probe[16];
    std::snprintf(probe, sizeof probe, "%.1f", 1.5);
    ASSERT_STREQ(probe, "1,5");
    for (char const* code : {"de", "ar"}) {
        apply_language(code);
        Vision::CameraSettings camera;
        camera.focal_length_mm = 4.25;
        camera.distance_mm = 1500;
        CameraDialog dialog(nullptr, camera, wxSize(1600, 1200), false, code);
        EXPECT_EQ(utf8(named<NumberField>(dialog, "focal")->text()), "4.25 mm") << code;
        EXPECT_EQ(utf8(named<NumberField>(dialog, "distance")->text()), "1500 mm") << code;
        EXPECT_TRUE(contains(label_of(dialog, "scale"), "mm/px"));
        EXPECT_FALSE(contains(label_of(dialog, "scale"), ",")) << label_of(dialog, "scale");
    }
    std::setlocale(LC_NUMERIC, saved.c_str());
}

#ifdef INDICATOR_GTK3
// Qt shows the numbers ("15.04 mm") at the fields' left edge in Arabic too
TEST(WxSetUpDialog, NumbersStayAtTheLeftInArabic) {
    Restore restore;
    for (char const* code : {"en", "ar"}) {
        apply_language(code);
        CameraDialog dialog(nullptr, sensor_camera(), {}, false, code);
        dialog.Show();
        for (wxString const name : {"width", "height", "distance", "focal"}) {
            wxWindow* field = named<wxWindow>(dialog, name);
            if (auto* number = dynamic_cast<NumberField*>(field))
                field = number->entry();
            GtkWidget* entry = field->GetHandle();
            if (!GTK_IS_ENTRY(entry))
                entry = gtk_bin_get_child(GTK_BIN(entry));
            ASSERT_TRUE(GTK_IS_ENTRY(entry)) << utf8(name);
            ASSERT_TRUE(
                pump_until([&] { return gtk_widget_get_allocated_width(entry) > 1; }, 3000ms));
            // where the entry draws its text: nearer its left edge than its right one
            gint x = 0, y = 0, text_w = 0;
            gtk_entry_get_layout_offsets(GTK_ENTRY(entry), &x, &y);
            pango_layout_get_pixel_size(gtk_entry_get_layout(GTK_ENTRY(entry)), &text_w, nullptr);
            int const right = gtk_widget_get_allocated_width(entry) - x - text_w;
            EXPECT_LT(x, right) << code << " " << utf8(name);
        }
        dialog.Hide();
    }
}
#endif

TEST(WxSetUpDialog, PressedFlagStaysPressed) {
    CameraDialog dialog(nullptr, Vision::CameraSettings {}, {}, false, "en");
    key(dialog, "language_en")->Click();
    EXPECT_TRUE(key(dialog, "language_en")->IsChecked());
    EXPECT_EQ(dialog.language(), "en");
    EXPECT_EQ(dialog.GetTitle(), "Set Up");
}

TEST(WxSetUpDialog, FlagPreviewsOnlyTheDialog) {
    Restore restore;
    CameraDialog dialog(nullptr, Vision::CameraSettings {}, {}, false, "en");
    key(dialog, "language_ar")->Click();
    EXPECT_EQ(dialog.GetLayoutDirection(), wxLayout_RightToLeft);
    EXPECT_EQ(utf8(dialog.GetTitle()), Message("setup.title").render("ar"));
    EXPECT_NE(dialog.GetTitle(), "Set Up");
    EXPECT_EQ(utf8(key(dialog, "language_ar")->GetToolTipText()), language_name("ar"));
    EXPECT_EQ(wxTheApp->GetLayoutDirection(), wxLayout_LeftToRight);
    EXPECT_EQ(current_language(), "en");
    key(dialog, "language_de")->Click();
    EXPECT_EQ(dialog.GetLayoutDirection(), wxLayout_LeftToRight);
    EXPECT_EQ(utf8(dialog.GetTitle()), Message("setup.title").render("de"));
    wxString const start = wx(Message("setup.covered").with("value", "").render("de")).Left(5);
    EXPECT_TRUE(named<wxStaticText>(dialog, "covered")->GetLabel().StartsWith(start));
    EXPECT_NE(start, wx(Message("setup.covered").with("value", "").render("en")).Left(5));

    // Cancel leaves nothing behind; OK hands back what the dialog shows
    CameraDialog modal(nullptr, Vision::CameraSettings {}, {}, false, "en");
    drive_modal([&](wxDialog&) {
        key(modal, "language_fr")->Click();
        key(modal, "theme")->Click();
        button(modal, wxID_CANCEL)->Click();
    });
    EXPECT_EQ(show_modal(modal), wxID_CANCEL);
    EXPECT_EQ(current_language(), "en");
    EXPECT_FALSE(light_theme());
    drive_modal([&](wxDialog&) { button(modal, wxID_OK)->Click(); });
    EXPECT_EQ(show_modal(modal), wxID_OK);
    EXPECT_EQ(modal.language(), "fr");
    EXPECT_TRUE(modal.light());
    EXPECT_EQ(current_language(), "en");  // the caller applies them
    EXPECT_FALSE(light_theme());
}

TEST(WxSetUpDialog, SensorModeResults) {
    CameraDialog dialog(nullptr, sensor_camera(), wxSize(1600, 1200), false, "en");
    EXPECT_TRUE(contains(label_of(dialog, "covered"), "1733 × 1300 mm"))
        << label_of(dialog, "covered");
    EXPECT_TRUE(contains(label_of(dialog, "scale"), "1.083 mm/px")) << label_of(dialog, "scale");
    CameraDialog none(nullptr, sensor_camera(), {}, false, "en");
    EXPECT_TRUE(contains(label_of(none, "scale"), "needs a photo"));
    EXPECT_TRUE(contains(label_of(none, "covered"), "1733 × 1300 mm"));
}

TEST(WxSetUpDialog, PixelModeResults) {
    Vision::CameraSettings c = pixel_camera(3.76, 4.20);
    c.distance_mm = 2000;
    c.focal_length_mm = 16;
    CameraDialog dialog(nullptr, c, wxSize(1000, 600), false, "en");
    EXPECT_TRUE(contains(label_of(dialog, "scale"), "0.470 × 0.525 mm/px"))
        << label_of(dialog, "scale");
    EXPECT_TRUE(contains(label_of(dialog, "covered"), "470 × 315 mm"))
        << label_of(dialog, "covered");
    CameraDialog none(nullptr, c, {}, false, "en");
    EXPECT_TRUE(contains(label_of(none, "scale"), "0.470 × 0.525 mm/px"));
    EXPECT_TRUE(contains(label_of(none, "covered"), "needs a photo"));
}

TEST(WxSetUpDialog, LinkKeepsHeightEqual) {
    CameraDialog dialog(nullptr, pixel_camera(3.76, 3.76), {}, false, "en");
    Key* link = key(dialog, "link");
    auto* width = named<wxComboBox>(dialog, "width");
    auto* height = named<wxComboBox>(dialog, "height");
    EXPECT_TRUE(link->IsChecked());
    EXPECT_FALSE(height->IsEnabled());
    width->SetValue("2.40");
    EXPECT_EQ(dialog.camera().pixel_height_um, 2.4);
    EXPECT_EQ(dialog.camera().pixel_width_um, 2.4);
    link->Click();  // off
    EXPECT_FALSE(link->IsChecked());
    EXPECT_TRUE(height->IsEnabled());
    width->SetValue("3.00");
    EXPECT_EQ(dialog.camera().pixel_width_um, 3.0);
    EXPECT_EQ(dialog.camera().pixel_height_um, 2.4);
    CameraDialog unequal(nullptr, pixel_camera(3.76, 4.2), {}, false, "en");
    EXPECT_FALSE(key(unequal, "link")->IsChecked());
}

TEST(WxSetUpDialog, ModesKeepTheirPairs) {
    CameraDialog dialog(nullptr, sensor_camera(), {}, false, "en");
    auto* width = named<wxComboBox>(dialog, "width");
    choose(radio(dialog, "Pixel size"));
    width->SetValue("2.00");
    choose(radio(dialog, "Sensor size"));
    EXPECT_TRUE(contains(utf8(width->GetValue()), "15.04")) << utf8(width->GetValue());
    Vision::CameraSettings const r = dialog.camera();
    EXPECT_EQ(r.input, Vision::SensorInput::sensor);
    EXPECT_EQ(r.sensor_width_mm, 15.04);
    EXPECT_EQ(r.sensor_height_mm, 11.28);
    EXPECT_EQ(r.pixel_width_um, 2.0);
    EXPECT_EQ(r.pixel_height_um, 2.0);
}

TEST(WxSetUpDialog, NamedSensorFillsBoth) {
    Vision::CameraSettings c = sensor_camera();
    c.sensor_width_mm = c.sensor_height_mm = 10;
    CameraDialog dialog(nullptr, c, {}, false, "en");
    auto* width = named<wxComboBox>(dialog, "width");
    EXPECT_TRUE(key(dialog, "link")->IsChecked());
    int const i = width->FindString("23.50 (APS-C)");
    ASSERT_GE(i, 0);
    pick(width, i);  // the user's choice
    EXPECT_EQ(dialog.camera().sensor_width_mm, 23.5);
    EXPECT_EQ(dialog.camera().sensor_height_mm, 15.6);
    EXPECT_FALSE(key(dialog, "link")->IsChecked());
    EXPECT_TRUE(contains(utf8(named<wxComboBox>(dialog, "height")->GetValue()), "15.60"));
}

TEST(WxSetUpDialog, OutOfRangeIsRejected) {
    CameraDialog dialog(nullptr, pixel_camera(3.76, 3.76), {}, false, "en");
    auto* width = named<wxComboBox>(dialog, "width");
    width->SetValue("150");
    EXPECT_EQ(dialog.camera().pixel_width_um, 3.76);
    width->SetValue("0.05");
    EXPECT_EQ(dialog.camera().pixel_width_um, 3.76);
    width->SetValue(wx("4.5 µm"));
    EXPECT_EQ(dialog.camera().pixel_width_um, 4.5);
}

TEST(WxSetUpDialog, RotatedPhotoSwapsPixelSides) {
    Vision::CameraSettings c = pixel_camera(3.76, 4.20);
    c.distance_mm = 2000;
    c.focal_length_mm = 16;
    CameraDialog dialog(nullptr, c, wxSize(600, 1000), true, "en");
    EXPECT_TRUE(contains(label_of(dialog, "scale"), "0.525 × 0.470 mm/px"))
        << label_of(dialog, "scale");
    EXPECT_TRUE(contains(label_of(dialog, "covered"), "315 × 470 mm"))
        << label_of(dialog, "covered");
}

TEST(WxSetUpDialog, SensorModeRotatedSwapsCovered) {
    Vision::CameraSettings c = sensor_camera();
    c.sensor_width_mm = 15.04;
    c.sensor_height_mm = 11.28;
    c.distance_mm = 1500;
    c.focal_length_mm = 13.02;
    CameraDialog dialog(nullptr, c, wxSize(1200, 1600), true, "en");
    EXPECT_TRUE(contains(label_of(dialog, "covered"), "1300 × 1733 mm"))
        << label_of(dialog, "covered");
}

TEST(WxSetUpDialog, ReChoosingTheNamedEntryFillsTheHeight) {
    Vision::CameraSettings c = sensor_camera();
    c.sensor_width_mm = c.sensor_height_mm = 10;
    CameraDialog dialog(nullptr, c, {}, false, "en");
    auto* width = named<wxComboBox>(dialog, "width");
    int const i = width->FindString("23.50 (APS-C)");
    ASSERT_GE(i, 0);
    width->SetSelection(i);
    width->SetValue("20");
    EXPECT_EQ(dialog.camera().sensor_width_mm, 20);
    pick(width, i);  // the same entry again
    EXPECT_EQ(dialog.camera().sensor_width_mm, 23.5);
    EXPECT_EQ(dialog.camera().sensor_height_mm, 15.6);
}

TEST(WxSetUpDialog, TypedOutOfRangeIsRestored) {
    CameraDialog dialog(nullptr, pixel_camera(3.76, 3.76), {}, false, "en");
    auto* width = named<wxComboBox>(dialog, "width");
    for (char const* typed : {"1", "15", "150"})
        width->SetValue(typed);  // one key at a time
    EXPECT_EQ(width->GetValue(), "150");
    EXPECT_EQ(dialog.camera().pixel_width_um, 15.0);
    send_focus_lost(width);  // the box is left: put back
    EXPECT_TRUE(contains(utf8(width->GetValue()), "15.00")) << utf8(width->GetValue());
    width->SetValue("150");
    EXPECT_EQ(width->GetValue(), "150");
    button(dialog, wxID_OK)->Click();  // restores the box before accepting
    EXPECT_NE(width->GetValue(), "150");
    EXPECT_EQ(dialog.camera().pixel_width_um, 15.0);
    EXPECT_TRUE(contains(utf8(width->GetValue()), "15.00")) << utf8(width->GetValue());
}

TEST(WxSetUpDialog, TextThatCannotBeASizeCannotBeTyped) {
    CameraDialog dialog(nullptr, pixel_camera(3.76, 3.76), {}, false, "en");
    for (char const* name : {"width", "height"}) {
        auto* box = named<wxComboBox>(dialog, name);
        box->SetValue("");
        type(box, "abc1");  // letters are the unit's: "abc" can be typed ...
        EXPECT_EQ(box->GetValue(), "abc") << name;  // ... but not a digit after them
        box->SetValue("");
        type(box, "#");
        EXPECT_EQ(box->GetValue(), "") << name;
        type(box, "!5");
        EXPECT_EQ(box->GetValue(), "5") << name;
        box->SetValue("");
        box->SetValue("%^&");  // pasted
        EXPECT_EQ(box->GetValue(), "") << name;
        type(box, "5.6 mm");
        EXPECT_EQ(box->GetValue(), "5.6 mm") << name;
        type(box, " (x)");
        EXPECT_EQ(box->GetValue(), "5.6 mm (x)") << name;
    }
    auto* width = named<wxComboBox>(dialog, "width");
    width->SetValue("");
    key(dialog, "link")->Click();  // off: the height box is free
    type(width, "5.6 mm");
    EXPECT_EQ(dialog.camera().pixel_width_um, 5.6);
}

TEST(WxSetUpDialog, EnterAnswersOkAndEscapeCancels) {
    Restore restore;
    CameraDialog dialog(nullptr, pixel_camera(3.76, 3.76), {}, false, "en");
    auto* width = named<wxComboBox>(dialog, "width");
    wxWindow* targets[] = {named<NumberField>(dialog, "distance")->entry(), width,
                           dialog.FindWindow(wxID_OK), &dialog};
    for (wxWindow* target : targets) {
        drive_modal([&](wxDialog&) { send_key(target, WXK_RETURN); });
        EXPECT_EQ(show_modal(dialog), wxID_OK) << target->GetClassInfo()->GetClassName();
        drive_modal([&](wxDialog&) { send_key(target, WXK_ESCAPE); });
        EXPECT_EQ(show_modal(dialog), wxID_CANCEL) << target->GetClassInfo()->GetClassName();
    }
    // Enter is OK: a half-typed box is put right on the way out
    for (char const* typed : {"1", "15", "150"})
        width->SetValue(typed);
    drive_modal([&](wxDialog&) { send_key(width, WXK_NUMPAD_ENTER); });
    EXPECT_EQ(show_modal(dialog), wxID_OK);
    EXPECT_TRUE(contains(utf8(width->GetValue()), "15.00")) << utf8(width->GetValue());
    // another key is left alone
    send_key(width, 'A');
    EXPECT_FALSE(dialog.IsModal());
}

TEST(WxSetUpDialog, AFocusedKeyTakesEnterItself) {
    Restore restore;
    CameraDialog dialog(nullptr, Vision::CameraSettings {}, {}, false, "en");
    int oks = 0;
    dialog.Bind(wxEVT_BUTTON, [&](wxCommandEvent& event) {
        if (event.GetId() == wxID_OK)
            ++oks;
        event.Skip();
    });
    send_key(key(dialog, "language_fr"), WXK_RETURN);
    EXPECT_EQ(dialog.language(), "fr");
    send_key(key(dialog, "theme"), WXK_RETURN);
    EXPECT_TRUE(dialog.light());
    EXPECT_EQ(oks, 0);
    // Cancel, with the focus on it: it is Cancel that acts
    drive_modal([&](wxDialog&) { send_key(button(dialog, wxID_CANCEL), WXK_RETURN); });
    EXPECT_EQ(show_modal(dialog), wxID_CANCEL);
    EXPECT_EQ(oks, 0);
    // a field still sends Enter to OK
    drive_modal(
        [&](wxDialog&) { send_key(named<NumberField>(dialog, "focal")->entry(), WXK_RETURN); });
    EXPECT_EQ(show_modal(dialog), wxID_OK);
    EXPECT_EQ(oks, 1);
#ifdef __linux__
    // the help key opens the manual: a stand-in browser first on the PATH
    TempDir bin;
    std::filesystem::path const record = bin.path / "arguments.txt";
    std::filesystem::path const browser = bin.path / "chromium";
    {
        std::ofstream script(browser);
        script << "#!/bin/sh\nprintf '%s\\n' \"$@\" > '" << record.string() << ".part' && mv '"
               << record.string() << ".part' '" << record.string() << "'\n";
    }
    std::filesystem::permissions(browser, std::filesystem::perms::owner_all);
    std::string const old_path = std::getenv("PATH") ? std::getenv("PATH") : "";
    setenv("PATH", (bin.path.string() + ":" + old_path).c_str(), 1);
    send_key(key(dialog, "help"), WXK_RETURN);
    setenv("PATH", old_path.c_str(), 1);
    EXPECT_TRUE(pump_until([&] { return std::filesystem::exists(record); }, 10s))
        << "the help key did not act";
    EXPECT_EQ(oks, 1);
#endif
}

TEST(WxSetUpDialog, AnOpenListTakesEscapeAndEnter) {
    CameraDialog dialog(nullptr, pixel_camera(3.76, 3.76), {}, false, "en");
    auto* width = named<wxComboBox>(dialog, "width");
    auto list_event = [&](wxEventType type) {
        wxCommandEvent event(type, width->GetId());
        event.SetEventObject(width);
        width->GetEventHandler()->ProcessEvent(event);
    };
    bool open_after_escape = false, open_after_enter = false;
    drive_modal([&](wxDialog& d) {
        list_event(wxEVT_COMBOBOX_DROPDOWN);
        send_key(width, WXK_ESCAPE);
        open_after_escape = d.IsModal();
        send_key(width, WXK_RETURN);
        open_after_enter = d.IsModal();
        list_event(wxEVT_COMBOBOX_CLOSEUP);
        send_key(width, WXK_ESCAPE);
    });
    EXPECT_EQ(show_modal(dialog), wxID_CANCEL);
    EXPECT_TRUE(open_after_escape);  // the list took it
    EXPECT_TRUE(open_after_enter);
}

TEST(WxSetUpDialog, AListLeftOpenNeverLeavesEnterAndEscapeDead) {
    CameraDialog dialog(nullptr, pixel_camera(3.76, 3.76), {}, false, "en");
    auto* width = named<wxComboBox>(dialog, "width");
    auto drop = [&] {
        wxCommandEvent event(wxEVT_COMBOBOX_DROPDOWN, width->GetId());
        event.SetEventObject(width);
        width->GetEventHandler()->ProcessEvent(event);
    };
    // a dropdown with no close-up, then the dialog is shown: Escape cancels
    drop();
    drive_modal([&](wxDialog&) { send_key(width, WXK_ESCAPE); });
    EXPECT_EQ(show_modal(dialog), wxID_CANCEL);
    // the same, and again after the dialog ended while a list was open
    drop();
    drive_modal([&](wxDialog&) {
        drop();
        send_key(width, WXK_ESCAPE);  // taken by the list
        EXPECT_TRUE(dialog.IsModal());
        dialog.EndModal(wxID_CANCEL);
    });
    EXPECT_EQ(show_modal(dialog), wxID_CANCEL);
    drive_modal([&](wxDialog&) { send_key(width, WXK_ESCAPE); });
    EXPECT_EQ(show_modal(dialog), wxID_CANCEL);
}

TEST(WxSetUpDialog, LeavingTheBoxKeepsItsListOpen) {
    // GTK may move the focus into an open list's popup: the list still takes Escape and Enter until
    // it closes
    CameraDialog dialog(nullptr, pixel_camera(3.76, 3.76), {}, false, "en");
    auto* width = named<wxComboBox>(dialog, "width");
    auto list_event = [&](wxEventType type) {
        wxCommandEvent event(type, width->GetId());
        event.SetEventObject(width);
        width->GetEventHandler()->ProcessEvent(event);
    };
    bool open_after_escape = false, open_after_enter = false;
    drive_modal([&](wxDialog& d) {
        list_event(wxEVT_COMBOBOX_DROPDOWN);
        send_focus_lost(width);
        send_key(width, WXK_ESCAPE);
        open_after_escape = d.IsModal();
        send_key(width, WXK_RETURN);
        open_after_enter = d.IsModal();
        list_event(wxEVT_COMBOBOX_CLOSEUP);
        send_key(width, WXK_ESCAPE);
    });
    EXPECT_EQ(show_modal(dialog), wxID_CANCEL);
    EXPECT_TRUE(open_after_escape);  // the list took it
    EXPECT_TRUE(open_after_enter);
    // a list open and the box left when the dialog ends: shown again, Escape cancels
    drive_modal([&](wxDialog& d) {
        list_event(wxEVT_COMBOBOX_DROPDOWN);
        send_focus_lost(width);
        d.EndModal(wxID_CLOSE);
    });
    EXPECT_EQ(show_modal(dialog), wxID_CLOSE);
    drive_modal([&](wxDialog&) { send_key(width, WXK_ESCAPE); });
    EXPECT_EQ(show_modal(dialog), wxID_CANCEL);
}

TEST(WxSetUpDialog, RoundKeyHoveredWhileOnLooksAsOn) {
    CameraDialog dialog(nullptr, Vision::CameraSettings {}, {}, false, "en");
    // Qt: ":checked" is a 2 px green border and ":hover" only sets the border colour to that same
    // green
    Key* en = key(dialog, "language_en");
    ASSERT_TRUE(en->IsChecked());
    auto pixels = [&] {
        wxImage image = en->Render(1.0).ConvertToImage();
        return std::string(reinterpret_cast<char const*>(image.GetData()),
                           static_cast<size_t>(image.GetWidth() * image.GetHeight() * 3));
    };
    std::string const on = pixels();
    wxMouseEvent enter(wxEVT_ENTER_WINDOW);
    enter.SetEventObject(en);
    en->GetEventHandler()->ProcessEvent(enter);
    EXPECT_TRUE(is_green(left_border(en)));
    EXPECT_EQ(pixels(), on);
    wxMouseEvent leave(wxEVT_LEAVE_WINDOW);
    leave.SetEventObject(en);
    en->GetEventHandler()->ProcessEvent(leave);
    EXPECT_EQ(pixels(), on);
}

TEST(WxSetUpDialog, RoundKeyRingsOnHover) {
    CameraDialog dialog(nullptr, Vision::CameraSettings {}, {}, false, "en");
    Key* fr = key(dialog, "language_fr");
    auto mouse = [&](Key* k, wxEventType type) {
        wxMouseEvent event(type);
        event.SetEventObject(k);
        k->GetEventHandler()->ProcessEvent(event);
    };
    auto edge = [&](Key* k) {
        wxImage const shot = k->Render(1.0).ConvertToImage();
        int const y = shot.GetHeight() / 2;
        return wxColour(shot.GetRed(0, y), shot.GetGreen(0, y), shot.GetBlue(0, y));
    };
    // the ring is one anti-aliased pixel wide: greenish, not the full green of a lit key
    auto greenish = [&](Key* k) {
        wxColour const c = edge(k);
        return c.Green() > 120 && c.Green() > c.Red() + 60;
    };
    EXPECT_FALSE(greenish(fr));
    mouse(fr, wxEVT_ENTER_WINDOW);
    EXPECT_TRUE(greenish(fr)) << hex(edge(fr));
    // a ring, not a lit key: the face inside is still the raised key's
    wxImage const image = fr->Render(1.0).ConvertToImage();
    EXPECT_FALSE(
        is_green(wxColour(image.GetRed(4, 10), image.GetGreen(4, 10), image.GetBlue(4, 10))));
    mouse(fr, wxEVT_LEAVE_WINDOW);
    EXPECT_FALSE(greenish(fr));
    // the square keys do not ring
    Key* ok = button(dialog, wxID_OK);
    mouse(ok, wxEVT_ENTER_WINDOW);
    wxImage const square = ok->Render(1.0).ConvertToImage();
    EXPECT_FALSE(is_green(wxColour(square.GetRed(0, square.GetHeight() / 2),
                                   square.GetGreen(0, square.GetHeight() / 2),
                                   square.GetBlue(0, square.GetHeight() / 2))));
}

// Qt's OK and Cancel carry the desktop icon theme's dialog-ok check mark and dialog-cancel "no"
// sign, 16 px, in the theme's dark grey (#232629, Breeze), before their text (after it in Arabic),
// in either panel theme
TEST(WxSetUpDialog, OkAndCancelCarryTheirIcons) {
    Restore restore;
    for (std::string const code : {"en", "ar"}) {
        for (bool light : {false, true}) {
            CameraDialog dialog(nullptr, Vision::CameraSettings {}, {}, false, code, light);
            for (Key* k : {button(dialog, wxID_OK), button(dialog, wxID_CANCEL)}) {
                ASSERT_TRUE(k->icon().IsOk()) << code;
                EXPECT_EQ(k->icon().GetDefaultSize(), wxSize(16, 16));
                // the icon's drawn pixels are the dark grey
                wxImage const icon = k->icon().GetBitmap(wxSize(16, 16)).ConvertToImage();
                int drawn = 0;
                for (int y = 0; y < 16; ++y) {
                    for (int x = 0; x < 16; ++x) {
                        if (icon.HasAlpha() && icon.GetAlpha(x, y) < 200)
                            continue;
                        ++drawn;
                        // within the rounding of the alpha the edges are drawn with
                        EXPECT_NEAR(icon.GetRed(x, y), 0x23, 2);
                        EXPECT_NEAR(icon.GetGreen(x, y), 0x26, 2);
                        EXPECT_NEAR(icon.GetBlue(x, y), 0x29, 2);
                    }
                }
                EXPECT_GT(drawn, 10) << code;
                // the text's green starts after the icon and its gap, or right away in Arabic
                wxImage const image = k->Render(1).ConvertToImage();
                int first = -1;
                for (int x = 0; x < image.GetWidth() && first < 0; ++x) {
                    for (int y = 0; y < image.GetHeight(); ++y) {
                        if (image.GetGreen(x, y) - image.GetRed(x, y) > 60) {
                            first = x;
                            break;
                        }
                    }
                }
                int const text = k->GetTextExtent(k->GetLabelText()).x;
                int const start = 1 + (k->GetSize().x - 2 - (16 + 6 + text)) / 2;
                EXPECT_NEAR(first, code == "ar" ? start : start + 16 + 6, 2)
                    << code << " " << utf8(k->GetLabelText());
            }
        }
    }
}

// "Abbrechen" with its icon is wider than the keys' least width: the key grows to show it whole
TEST(WxSetUpDialog, OkAndCancelAreWideEnoughForTheirText) {
    for (std::string const& code : language_codes()) {
        CameraDialog dialog(nullptr, Vision::CameraSettings {}, {}, false, code);
        for (Key* k : {button(dialog, wxID_OK), button(dialog, wxID_CANCEL)}) {
            EXPECT_GE(k->GetSize().x, k->GetBestSize().x) << code << " " << utf8(k->GetLabelText());
            EXPECT_GE(k->GetSize().x, dialog.FromDIP(64)) << code;
        }
    }
}

TEST(WxSetUpDialog, TypedNamedTextKeepsHeight) {
    CameraDialog dialog(nullptr, sensor_camera(), {}, false, "en");
    ASSERT_FALSE(key(dialog, "link")->IsChecked());  // 15.04 x 11.28: the heights are free
    named<wxComboBox>(dialog, "height")->SetValue("9.00");
    named<wxComboBox>(dialog, "width")->SetValue("15.04 mm");
    EXPECT_EQ(dialog.camera().sensor_height_mm, 9.0);
}

TEST(WxSetUpDialog, DialogStyle) {
    Restore restore;
    CameraDialog dialog(nullptr, Vision::CameraSettings {}, {}, false, "en");
    Palette const p = palette(false);
    EXPECT_EQ(hex(dialog.GetBackgroundColour()), hex(p.panel));  // black like the window
    EXPECT_EQ(hex(dialog.GetBackgroundColour()), "#000000");
    // the panel's keys: the keys' green text on their dark face
    Key* ok = button(dialog, wxID_OK);
    ASSERT_NE(ok, nullptr);
    EXPECT_EQ(hex(p.key_text), "#47FF86");
    EXPECT_EQ(utf8(ok->GetLabel()), "OK");
    wxImage const image = ok->Render(1.0).ConvertToImage();
    // within a few levels: where no glyph covers a whole pixel, antialiasing mixes in the face
    auto near = [](unsigned char value, int target) { return std::abs(value - target) <= 8; };
    bool green = false;
    for (int y = 0; y < image.GetHeight() && !green; ++y) {
        for (int x = 0; x < image.GetWidth() && !green; ++x) {
            green = near(image.GetRed(x, y), 0x47) && near(image.GetGreen(x, y), 0xFF) &&
                    near(image.GetBlue(x, y), 0x86);
        }
    }
    EXPECT_TRUE(green) << "no key-green text pixel";
    EXPECT_EQ(hex(dialog.GetBackgroundColour()),
              hex(palette(light_theme()).panel));  // the theme in use
}

TEST(WxSetUpDialog, EveryChildFollowsTheTheme) {
    Restore restore;
    CameraDialog dialog(nullptr, Vision::CameraSettings {}, {}, false, "en");
    Key* theme = key(dialog, "theme");
    theme->Click();
    std::vector<wxWindow*> all;
    collect_all(&dialog, all);
    ASSERT_GE(all.size(), 25u);
    Palette const light = palette(true);
    for (wxWindow* window : all) {
        wxColour const c = window->GetBackgroundColour();
        EXPECT_TRUE(c == light.panel || c == light.field)
            << window->GetClassInfo()->GetClassName() << " " << hex(c);
    }
    theme->Click();
    all.clear();
    collect_all(&dialog, all);
    Palette const dark = palette(false);
    for (wxWindow* window : all) {
        wxColour const c = window->GetBackgroundColour();
        EXPECT_TRUE(c == dark.panel || c == dark.field)
            << window->GetClassInfo()->GetClassName() << " " << hex(c);
    }
    EXPECT_TRUE(dialog.GetBackgroundColour() == dark.panel);
    EXPECT_FALSE(light_theme());
}

namespace {
/// A NumberField 1..100, two decimals, in a frame, counting its change events.
struct FieldFixture {
    wxFrame* frame = new wxFrame(nullptr, wxID_ANY, "n");
    NumberField* field = new NumberField(frame, 1, 100, 2, " mm", 10);
    int changes = 0;
    FieldFixture() {
        field->Bind(EVT_NUMBER_CHANGED, [this](wxCommandEvent&) { ++changes; });
    }
    ~FieldFixture() { frame->Destroy(); }
};
}  // namespace

TEST(WxNumberField, UpAndDownClampAtTheRangeEnds) {
    FieldFixture f;
    send_key(f.field->entry(), WXK_DOWN, wxEVT_KEY_DOWN);
    EXPECT_EQ(f.field->value(), 9);
    f.field->set_value(1);
    f.changes = 0;
    send_key(f.field->entry(), WXK_DOWN, wxEVT_KEY_DOWN);
    EXPECT_EQ(f.field->value(), 1);
    EXPECT_EQ(f.changes, 0);
    f.field->set_value(99.5);
    send_key(f.field->entry(), WXK_UP, wxEVT_KEY_DOWN);
    EXPECT_EQ(f.field->value(), 100);
    send_key(f.field->entry(), WXK_UP, wxEVT_KEY_DOWN);
    EXPECT_EQ(f.field->value(), 100);
    EXPECT_EQ(utf8(f.field->text()), "100.00 mm");
    EXPECT_EQ(f.changes, 1);
}

TEST(WxNumberField, PageKeysStepTenAndTheWheelOne) {
    FieldFixture f;
    send_key(f.field->entry(), WXK_PAGEUP, wxEVT_KEY_DOWN);
    EXPECT_EQ(f.field->value(), 20);
    send_key(f.field->entry(), WXK_PAGEDOWN, wxEVT_KEY_DOWN);
    send_key(f.field->entry(), WXK_PAGEDOWN, wxEVT_KEY_DOWN);
    EXPECT_EQ(f.field->value(), 1);  // 20 - 20 = 0, clamped
    wheel(f.field->entry(), 120);
    EXPECT_EQ(f.field->value(), 2);
    wheel(f.field->entry(), 240);  // two notches
    EXPECT_EQ(f.field->value(), 4);
    wheel(f.field->entry(), -120);
    EXPECT_EQ(f.field->value(), 3);
    f.field->set_value(100);
    wheel(f.field->entry(), 120);
    EXPECT_EQ(f.field->value(), 100);
}

TEST(WxNumberField, TypedOutOfRangeIsRestoredWhenLeft) {
    FieldFixture f;
    f.field->entry()->SetValue("150 mm");
    EXPECT_EQ(f.field->value(), 10);
    EXPECT_EQ(f.changes, 0);
    f.field->entry()->SetValue("abc");
    EXPECT_EQ(f.field->value(), 10);
    send_focus_lost(f.field->entry());
    EXPECT_EQ(utf8(f.field->text()), "10.00 mm");
    f.field->entry()->SetValue("0.5");
    EXPECT_EQ(f.field->value(), 10);
}

TEST(WxNumberField, ChangeEventOncePerChange) {
    FieldFixture f;
    f.field->entry()->SetValue("25");
    EXPECT_EQ(f.changes, 1);
    EXPECT_EQ(f.field->value(), 25);
    f.field->set_value(30);  // by the program: no event
    EXPECT_EQ(f.changes, 1);
    send_key(f.field->entry(), WXK_PAGEUP, wxEVT_KEY_DOWN);
    EXPECT_EQ(f.changes, 2);
    wheel(f.field->entry(), 120);
    EXPECT_EQ(f.changes, 3);
    f.field->entry()->SetValue("junk");
    EXPECT_EQ(f.changes, 3);
}

TEST(WxNumberField, EnglishGroupSeparators) {
    NumberField* field;
    auto* frame = new wxFrame(nullptr, wxID_ANY, "n");
    field = new NumberField(frame, 1, 100000, 0, " mm", 7);
    auto typed = [&](char const* text) {
        field->set_value(7);
        field->entry()->SetValue(text);
        return field->value();
    };
    EXPECT_EQ(typed("1,500"), 1500);
    EXPECT_EQ(typed("12,345 mm"), 12345);
    EXPECT_EQ(typed("1,234,5"), 7);  // misplaced: not a number
    EXPECT_EQ(typed("1,5"), 7);
    EXPECT_EQ(typed("1.5"), 1.5);
    EXPECT_EQ(typed("1,500.5"), 1500.5);
    frame->Destroy();
}

}  // namespace test_SetUpDialog
