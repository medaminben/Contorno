#include <gtest/gtest.h>

#include <wx/bitmap.h>
#include <wx/choice.h>
#include <wx/frame.h>
#include <wx/image.h>
#include <wx/log.h>
#include <wx/dialog.h>
#include <wx/listbox.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/textctrl.h>
#include <wx/tglbtn.h>

#include "model/Language.h"
#include "model/Utf8Path.h"
#include "ui/Fonts.h"
#include "ui/Text.h"
#include "ui/Theme.h"
#include "ui/WxConfigStore.h"
#include "ui/WxTestApp.h"
#include "TempDir.h"

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>

#ifdef INDICATOR_GTK3
#include <wx/fontutil.h>

#include <cairo.h>
#include <gtk/gtk.h>
#include <pango/pangocairo.h>
#endif

namespace test_Theme {

namespace {

/// A colour a light theme could produce: a light fill or light text.
bool is_light(wxColour const& c) {
    return c.Red() + c.Green() + c.Blue() > 3 * 160;
}

void collect_all(wxWindow* window, std::vector<wxWindow*>& out) {
    for (wxWindow* child : window->GetChildren()) {
        out.push_back(child);
        collect_all(child, out);
    }
}

}  // namespace

TEST(WxTheme, EveryChildFollowsTheTheme) {
    auto* frame = new wxFrame(nullptr, wxID_ANY, "Theme");
    auto* panel = new wxPanel(frame);
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    wxArrayString choices;
    choices.Add("one");
    choices.Add("two");
    sizer->Add(new wxTextCtrl(panel, wxID_ANY, "text"));
    sizer->Add(new wxChoice(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, choices));
    sizer->Add(new wxListBox(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, choices));
    sizer->Add(new wxSpinCtrl(panel));
    sizer->Add(new Key(panel, wxID_ANY, "Key"));
    panel->SetSizer(sizer);

    apply_theme(frame, true);
    apply_theme(frame, false);

    std::vector<wxWindow*> all;
    collect_all(frame, all);
    ASSERT_GE(all.size(), 6u);
    Palette const dark = palette(false);
    for (wxWindow* window : all) {
        wxColour const background = window->GetBackgroundColour();
        EXPECT_TRUE(background == dark.panel || background == dark.field)
            << window->GetClassInfo()->GetClassName();
        EXPECT_FALSE(is_light(background)) << window->GetClassInfo()->GetClassName();
    }
    EXPECT_TRUE(frame->GetBackgroundColour() == dark.panel);
    frame->Destroy();
}

TEST(WxTheme, PalettesMatchTheQtStylesheets) {
    EXPECT_EQ(palette(false).panel, wxColour("#000000"));
    EXPECT_EQ(palette(false).field, wxColour("#050805"));
    EXPECT_EQ(palette(true).panel, wxColour("#E4E7E3"));
    EXPECT_EQ(palette(true).selection_text, wxColour("#47FF86"));
    EXPECT_EQ(result_colour(true), "#127A36");
    EXPECT_EQ(result_colour(false), "#47FF86");
}

TEST(WxKey, DrawsAtContentScale) {
    auto* frame = new wxFrame(nullptr, wxID_ANY, "Key");
    auto* key = new Key(frame, wxID_ANY, "Measure", {}, 6, 18);
    wxSize const size = key->GetBestSize();
    EXPECT_GT(size.x, 36);
    key->SetSize(size);

    wxBitmap const bitmap = key->Render(2.0);
    ASSERT_TRUE(bitmap.IsOk());
    EXPECT_EQ(bitmap.GetWidth(), 2 * size.x);
    EXPECT_EQ(bitmap.GetHeight(), 2 * size.y);

    wxImage const image = bitmap.ConvertToImage();
    int const x = image.GetWidth() / 2;
    // the raised key's top edge: the light #9aa29c row, two device rows at scale 2
    for (int y : {0, 1}) {
        EXPECT_NEAR(image.GetRed(x, y), 0x9a, 12) << "row " << y;
        EXPECT_NEAR(image.GetGreen(x, y), 0xa2, 12) << "row " << y;
        EXPECT_NEAR(image.GetBlue(x, y), 0x9c, 12) << "row " << y;
    }
    frame->Destroy();
}

TEST(WxKey, ClickEmitsButtonAndToggleEmitsToggle) {
    auto* frame = new wxFrame(nullptr, wxID_ANY, "Key");
    auto* key = new Key(frame, wxID_ANY, "Go");
    int buttons = 0, toggles = 0;
    frame->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) { ++buttons; });
    frame->Bind(wxEVT_TOGGLEBUTTON, [&](wxCommandEvent&) { ++toggles; });
    key->Click();
    EXPECT_EQ(buttons, 1);
    key->SetCheckable(true);
    key->Click();
    EXPECT_EQ(toggles, 1);
    EXPECT_TRUE(key->IsChecked());
    key->Click();
    EXPECT_FALSE(key->IsChecked());
    EXPECT_EQ(buttons, 1);
    frame->Destroy();
}

TEST(WxKey, FastSecondClickCountsOnGtk) {
    auto* frame = new wxFrame(nullptr, wxID_ANY, "Key");
    auto* key = new Key(frame, wxID_ANY, "Go", {}, 6, 18);
    key->SetSize(key->GetBestSize());
    frame->Show();
    int buttons = 0, toggles = 0;
    frame->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) { ++buttons; });
    frame->Bind(wxEVT_TOGGLEBUTTON, [&](wxCommandEvent&) { ++toggles; });
    // GTK: press, release, then the second press as a double click, release
    auto click_sequence = [&] {
        for (wxEventType type :
             {wxEVT_LEFT_DOWN, wxEVT_LEFT_UP, wxEVT_LEFT_DCLICK, wxEVT_LEFT_UP}) {
            wxMouseEvent e(type);
            e.SetEventObject(key);
            e.SetPosition(wxPoint(5, 5));
            key->GetEventHandler()->ProcessEvent(e);
        }
    };
    click_sequence();
    EXPECT_EQ(buttons, 2);
    key->SetCheckable(true);
    click_sequence();
    EXPECT_EQ(toggles, 2);
    EXPECT_FALSE(key->IsChecked());
    frame->Destroy();
}

TEST(WxKey, KeyboardReachesAndPressesKeys) {
    auto* dialog = new wxDialog(nullptr, wxID_ANY, "Keys");
    auto* first = new Key(dialog, wxID_OK, "OK", {}, 6, 18);
    auto* second = new Key(dialog, wxID_CANCEL, "Cancel", {}, 6, 18);
    auto* sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(first);
    sizer->Add(second);
    dialog->SetSizerAndFit(sizer);
    // Tab reaches a window that accepts focus from the keyboard, is shown and enabled; asked of the
    // windows themselves, as the real focus belongs to the window manager (parallel tests share the
    // display)
    dialog->Show();
    for (Key* key : {first, second}) {
        EXPECT_TRUE(key->AcceptsFocusFromKeyboard());
        EXPECT_TRUE(key->CanAcceptFocusFromKeyboard());
    }
    second->Enable(false);
    EXPECT_FALSE(second->CanAcceptFocusFromKeyboard());
    second->Enable(true);

    int ok = 0, cancel = 0;
    dialog->Bind(wxEVT_BUTTON, [&](wxCommandEvent& e) { (e.GetId() == wxID_OK ? ok : cancel)++; });
    auto press = [&](int code, wxEventType type) {
        wxKeyEvent e(type);
        e.SetEventObject(second);
        e.m_keyCode = code;
        second->GetEventHandler()->ProcessEvent(e);
    };
    // Enter on the focused Cancel key presses Cancel, not the dialog's default key
    press(WXK_RETURN, wxEVT_CHAR_HOOK);
    EXPECT_EQ(cancel, 1);
    EXPECT_EQ(ok, 0);
    press(WXK_SPACE, wxEVT_KEY_DOWN);
    press(WXK_SPACE, wxEVT_KEY_UP);
    EXPECT_EQ(cancel, 2);
    dialog->Destroy();
}

TEST(WxKey, DisabledIconIsTheQtDimGreen) {
    auto* frame = new wxFrame(nullptr, wxID_ANY, "Key");
    auto* key = new Key(frame, wxID_ANY, "", tinted_icon("up"), 4, 8);
    key->SetSize(key->GetBestSize());
    key->Enable(false);
    wxImage const image = key->Render(1.0).ConvertToImage();
    bool dim = false, green = false;
    for (int y = 0; y < image.GetHeight(); ++y) {
        for (int x = 0; x < image.GetWidth(); ++x) {
            if (image.GetRed(x, y) == 0x3a && image.GetGreen(x, y) == 0x4a &&
                image.GetBlue(x, y) == 0x3f)
                dim = true;
            if (image.GetGreen(x, y) > 200 && image.GetRed(x, y) < 120)
                green = true;
        }
    }
    EXPECT_TRUE(dim);
    EXPECT_FALSE(green);
    frame->Destroy();
}

#ifdef INDICATOR_GTK3
namespace {

/// `text` in `font`, in the keys' green on black, as a Pango layout draws it: shaped, in its own
/// direction.
wxImage pango_text(wxFont const& font, wxString const& text) {
    int const w = 400, h = 100;
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
    cairo_t* cr = cairo_create(surface);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_paint(cr);
    PangoLayout* layout = pango_cairo_create_layout(cr);
    pango_layout_set_font_description(layout, font.GetNativeFontInfo()->description);
    std::string const utf8_text = text.utf8_string();
    pango_layout_set_text(layout, utf8_text.data(), static_cast<int>(utf8_text.size()));
    cairo_set_source_rgb(cr, 0x47 / 255.0, 1, 0x86 / 255.0);
    cairo_move_to(cr, 10, 10);
    pango_cairo_show_layout(cr, layout);
    g_object_unref(layout);
    cairo_destroy(cr);
    cairo_surface_flush(surface);
    wxImage image(w, h, false);
    unsigned char const* data = cairo_image_surface_get_data(surface);
    int const stride = cairo_image_surface_get_stride(surface);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            auto const pixel = *reinterpret_cast<std::uint32_t const*>(data + y * stride + 4 * x);
            image.SetRGB(x, y, (pixel >> 16) & 0xff, (pixel >> 8) & 0xff, pixel & 0xff);
        }
    }
    cairo_surface_destroy(surface);
    return image;
}

}  // namespace

// A key in an Arabic window is right to left; its label must still come out joined and in reading
// order, as Pango lays it out, on the screen (GTK hands a right-to-left window a mirrored context
// to paint in).
TEST(WxKey, ArabicLabelOnScreenIsShapedAsPangoLaysItOut) {
    wxString const label = wx(lookup("ar", "panel.import"));
    auto* frame = new wxFrame(nullptr, wxID_ANY, "Key");
    auto* key = new Key(frame, wxID_ANY, label, {}, 6, 18);
    key->SetFont(pixel_font(key->GetFont(), 24));
    key->SetLayoutDirection(wxLayout_RightToLeft);
    key->SetSize(key->GetBestSize());
    frame->SetClientSize(key->GetSize() + wxSize(20, 20));
    frame->Show();
    wxRect const inside(6, 6, key->GetSize().x - 12,
                        key->GetSize().y - 18);  // inside the focus ring
    Ink const drawn = ink_on_screen(*key, inside);
    if (!drawn.read)
        GTEST_SKIP() << "the screen cannot be read here";
    ASSERT_GT(drawn.count, 0);
    wxImage const reference = pango_text(key->GetFont(), label);
    Ink const expected = ink_of(reference, wxRect(reference.GetSize()));
    ASSERT_GT(expected.count, 50);
    EXPECT_NEAR(drawn.w, expected.w, 2);
    EXPECT_NEAR(drawn.h, expected.h, 2);
    EXPECT_LT(ink_difference(drawn, expected), 0.15);
    frame->Destroy();
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"  // the border colour has no other getter
TEST(WxTheme, FieldFrameAndSelectionReachGtk) {
    auto border_of = [](wxWindow* field, GtkStateFlags state) {
        GdkRGBA border;
        gtk_style_context_get_border_color(gtk_widget_get_style_context(field->GetHandle()), state,
                                           &border);
        return wxColour(std::lround(border.red * 255), std::lround(border.green * 255),
                        std::lround(border.blue * 255));
    };
    auto expect_theme = [&](wxWindow* field, bool light, char const* what) {
        EXPECT_EQ(border_of(field, GTK_STATE_FLAG_NORMAL), palette(light).frame) << what;
        EXPECT_EQ(border_of(field, GTK_STATE_FLAG_FOCUSED), palette(light).focus) << what;
    };
    auto* dark_frame = new wxFrame(nullptr, wxID_ANY, "Dark");
    auto* light_frame = new wxFrame(nullptr, wxID_ANY, "Light");
    auto* dark_field = new wxTextCtrl(dark_frame, wxID_ANY, "text");
    auto* light_field = new wxTextCtrl(light_frame, wxID_ANY, "text");
    apply_theme(dark_frame, false);
    apply_theme(light_frame, true);
    expect_theme(dark_field, false, "dark frame after the light one was themed");
    expect_theme(light_field, true, "light frame");
    // theming one window again leaves the other alone
    apply_theme(light_frame, false);
    expect_theme(light_field, false, "light frame re-themed dark");
    expect_theme(dark_field, false, "dark frame");
    apply_theme(light_frame, true);
    apply_theme(dark_frame, true);
    expect_theme(dark_field, true, "dark frame re-themed light");
    expect_theme(light_field, true, "light frame");
    apply_theme(dark_frame, false);
    expect_theme(light_field, true, "light frame unchanged by the other");
    dark_frame->Destroy();
    light_frame->Destroy();
}
#pragma GCC diagnostic pop
#endif

namespace {
/// Counts what the wx log is asked to show; the default target would open a dialog from the idle
/// handler.
class CountingLog : public wxLog {
public:
    int shown = 0;

protected:
    void DoLogRecord(wxLogLevel level, wxString const&, wxLogRecordInfo const&) override {
        if (level <= wxLOG_Warning)
            ++shown;
    }
};
}  // namespace

TEST(WxSettings, UnwritableConfigIsSilent) {
    TempDir dir;
    std::filesystem::path const folder = dir.path / "locked";
    std::filesystem::create_directories(folder);
    ASSERT_EQ(chmod(folder.c_str(), 0555), 0);
    bool const writable = access(folder.c_str(), W_OK) == 0;  // root ignores the mode
    auto* log = new CountingLog;
    wxLog* const previous = wxLog::SetActiveTarget(log);
    bool dialog_shown = false;
    std::optional<std::string> read_back;
    {
        WxConfigStore store(folder / "indicator.ini");
        EXPECT_NO_THROW(store.set_value("camera/distance_mm", "1500"));
        EXPECT_NO_THROW(read_back = store.value("camera/distance_mm"));
        EXPECT_FALSE(store.value("missing/key").has_value());
        // what a log dialog or a delayed message would do runs in the idle handler
        pump_until([] { return false; }, std::chrono::milliseconds(200));
        wxLog::FlushActive();
        for (wxWindow* window : wxTopLevelWindows)
            dialog_shown = dialog_shown || window->IsShown();
    }
    wxLog::SetActiveTarget(previous);
    int const messages = log->shown;
    delete log;
    chmod(folder.c_str(), 0755);
    EXPECT_FALSE(dialog_shown);
    EXPECT_EQ(messages, 0) << "the failed write reached the log";
    EXPECT_EQ(read_back, std::optional<std::string>("1500"));  // kept in memory
    if (writable)
        GTEST_SKIP() << "the directory is writable (running as root)";
}

TEST(WxSettings, ValuesRoundTripThroughTheIniFile) {
    TempDir dir;
    auto const ini = dir.path / "sub" / "indicator.ini";
    {
        WxConfigStore store(ini);
        store.set_value("camera/distance_mm", "1500");
        store.set_value("window/title", "Gerät ü");
    }
    WxConfigStore again(ini);
    EXPECT_EQ(again.value("camera/distance_mm"), "1500");
    EXPECT_EQ(again.value("window/title"), "Gerät ü");
    EXPECT_FALSE(again.value("camera/other").has_value());
    EXPECT_EQ(default_settings_file().filename(), "indicator.ini");
    EXPECT_EQ(default_settings_file().parent_path().filename(), "Indicator");
}

#if defined(__UNIX__) && !defined(__APPLE__)
// Ruling 24: the settings live under $XDG_CONFIG_HOME (~/.config), not in a folder of the home
// folder itself
TEST(WxSettings, FileIsInTheXdgConfigFolder) {
    TempDir config;
    char const* const before = std::getenv("XDG_CONFIG_HOME");
    std::string const saved = before ? before : "";
    setenv("XDG_CONFIG_HOME", utf8(config.path).c_str(), 1);
    std::filesystem::path const file = default_settings_file();
    if (before)
        setenv("XDG_CONFIG_HOME", saved.c_str(), 1);
    else
        unsetenv("XDG_CONFIG_HOME");
    EXPECT_EQ(file, config.path / "Contorno" / "Indicator" / "indicator.ini");
}
#endif

}  // namespace test_Theme
