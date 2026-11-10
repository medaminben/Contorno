#include "model/DataFolder.h"
#include "model/IndicatorSettings.h"
#include "model/Language.h"
#include "model/Measure.h"
#include "model/SettingsStore.h"
#include "model/Utf8Path.h"
#include "ui/CameraDialog.h"
#include "ui/Catalogue.h"
#include "ui/Localise.h"
#include "ui/MainWindow.h"
#include "ui/PhotoFile.h"
#include "ui/Text.h"
#include "ui/WxConfigStore.h"

#include <Tools/Dxf.h>
#include <Tools/Zip.h>
#include <Vision/Objects.h>

#include <wx/app.h>
#include <wx/combobox.h>
#include <wx/dialog.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/image.h>
#include <wx/statbmp.h>
#include <wx/stdpaths.h>
#include <wx/utils.h>

#ifdef __WXMSW__
#include <dwmapi.h>
#include <uxtheme.h>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#endif

#ifdef INDICATOR_GTK3
#include <gtk/gtk.h>
#else
#include <wx/dcmemory.h>
#include <wx/dcscreen.h>
#endif

#include <cctype>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace {

/// ctest's SKIP_RETURN_CODE: the AI outline is unavailable on this build.
constexpr int skipped = 77;

/// Runs the event loop until `done` or `timeout`; false on timeout.
bool pump_until(std::function<bool()> const& done, std::chrono::milliseconds timeout) {
    auto const end = std::chrono::steady_clock::now() + timeout;
    while (!done()) {
        if (std::chrono::steady_clock::now() >= end)
            return false;
        wxTheApp->Yield(true);
        wxMilliSleep(5);
    }
    return true;
}

/// Waits until no analysis runs or waits; false on timeout or a failed analysis.
bool wait_idle(MainWindow& window) {
    struct Outcome {
        bool ok = false;
        void on(wxCommandEvent& event) {
            ok = event.GetInt() != 0;
            event.Skip();
        }
    } outcome {window.analysis().has_value()};
    window.Bind(EVT_ANALYSED, &Outcome::on, &outcome);
    // each wait: the failing step prints before ctest's TIMEOUT (300 s)
    bool const idle = pump_until([&] { return !window.busy(); }, std::chrono::seconds(120));
    window.Unbind(EVT_ANALYSED, &Outcome::on, &outcome);
    return idle && outcome.ok;
}

int fail(char const* step, std::string const& why) {
    std::fprintf(stderr, "self-test: %s: %s\n", step, why.c_str());
    return 1;
}

int fail(char const* step, MainWindow const& window) {
    return fail(step, window.busy() ? std::string("no result within 120 s") : window.last_error());
}

/// The readout shows the area of the result shown.
bool readout_matches(MainWindow& window) {
    auto const& a = window.analysis();
    return a && utf8(window.panel().display().text()) == format_area(a->area_mm2).digits;
}

/// Outer rings and holes of all objects.
std::size_t ring_count(Vision::ObjectsAnalysis const& a) {
    std::size_t rings = 0;
    for (auto const& o : a.objects)
        rings += 1 + o.holes.size();
    return rings;
}

/// The window as it is on the screen; invalid when it cannot be read.
wxImage screenshot(wxWindow& window) {
#ifdef INDICATOR_GTK3
    GdkWindow* const gdk = gtk_widget_get_window(window.GetHandle());
    if (!gdk)
        return {};
    GdkPixbuf* const pixbuf = gdk_pixbuf_get_from_window(gdk, 0, 0, gdk_window_get_width(gdk),
                                                         gdk_window_get_height(gdk));
    if (!pixbuf)
        return {};
    int const w = gdk_pixbuf_get_width(pixbuf), h = gdk_pixbuf_get_height(pixbuf);
    int const channels = gdk_pixbuf_get_n_channels(pixbuf),
              stride = gdk_pixbuf_get_rowstride(pixbuf);
    guchar const* const pixels = gdk_pixbuf_get_pixels(pixbuf);
    wxImage image(w, h, false);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            guchar const* p = pixels + y * stride + x * channels;
            image.SetRGB(x, y, p[0], p[1], p[2]);
        }
    }
    g_object_unref(pixbuf);
    return image;
#else
    // read from the screen, the title bar included
    wxScreenDC screen;
    wxRect const rect = window.GetScreenRect();
    wxBitmap bitmap(rect.GetSize());
    wxMemoryDC memory(bitmap);
    memory.Blit(0, 0, rect.width, rect.height, &screen, rect.x, rect.y);
    memory.SelectObject(wxNullBitmap);
    return bitmap.ConvertToImage();
#endif
}

// DIAGNOSTIC (temporary): which way of building a field paints it dark on Windows, and whether
// DWM gives the window a dark title bar. Saved as self_test_probe.png.
void probe_dark_fields(wxWindow& parent, std::filesystem::path const& out) {
    wxColour const back(0x05, 0x08, 0x05), text(0xFF, 0xFF, 0xFF);
    auto* probe = new wxDialog(&parent, wxID_ANY, "Probe", wxPoint(0, 0), wxDefaultSize,
                               wxDEFAULT_DIALOG_STYLE);
    probe->SetBackgroundColour(*wxBLACK);
    auto* grid = new wxFlexGridSizer(2, 6, 12);
    auto row = [&](wxString const& name, wxWindow* field) {
        auto* label = new wxStaticText(probe, wxID_ANY, name);
        label->SetForegroundColour(text);
        grid->Add(label, 0, wxALIGN_CENTER_VERTICAL);
        grid->Add(field, 0, wxEXPAND);
        field->SetBackgroundColour(back);
        field->SetForegroundColour(text);
        return field;
    };
    row("1 text", new wxTextCtrl(probe, wxID_ANY, "one"));
    row("2 text rich2",
        new wxTextCtrl(probe, wxID_ANY, "two", wxDefaultPosition, wxDefaultSize, wxTE_RICH2));
    row("3 text simple border", new wxTextCtrl(probe, wxID_ANY, "three", wxDefaultPosition,
                                               wxDefaultSize, wxBORDER_SIMPLE));
    auto* panel = new wxPanel(probe);
    auto* inner = new wxTextCtrl(panel, wxID_ANY, "four");
    inner->SetBackgroundColour(back);
    inner->SetForegroundColour(text);
    panel->SetSizer(new wxBoxSizer(wxHORIZONTAL));
    panel->GetSizer()->Add(inner, 1);
    row("4 text in panel", panel);
    row("5 combo", new wxComboBox(probe, wxID_ANY, "five"));
    wxWindow* six = row("6 text no theme", new wxTextCtrl(probe, wxID_ANY, "six"));
    wxWindow* seven = row("7 text DarkMode_Explorer", new wxTextCtrl(probe, wxID_ANY, "seven"));
    wxWindow* eight = row("8 combo DarkMode_CFD", new wxComboBox(probe, wxID_ANY, "eight"));
    wxWindow* nine = row("9 text DarkMode_CFD", new wxTextCtrl(probe, wxID_ANY, "nine"));
#ifdef __WXMSW__
    SetWindowTheme(static_cast<HWND>(six->GetHandle()), L"", L"");
    SetWindowTheme(static_cast<HWND>(seven->GetHandle()), L"DarkMode_Explorer", nullptr);
    SetWindowTheme(static_cast<HWND>(eight->GetHandle()), L"DarkMode_CFD", nullptr);
    SetWindowTheme(static_cast<HWND>(nine->GetHandle()), L"DarkMode_CFD", nullptr);
    BOOL const dark = TRUE;
    DwmSetWindowAttribute(static_cast<HWND>(probe->GetHandle()), 20, &dark, sizeof dark);
#else
    (void)six, (void)seven, (void)eight, (void)nine;
#endif
    auto* column = new wxBoxSizer(wxVERTICAL);
    column->Add(grid, 0, wxALL, 12);
    probe->SetSizerAndFit(column);
    probe->Show();
    pump_until([] { return false; }, std::chrono::seconds(1));
    wxImage const shot = screenshot(*probe);
    if (shot.IsOk())
        shot.SaveFile(wx(utf8(out / "self_test_probe.png")), wxBITMAP_TYPE_PNG);
    probe->Destroy();
}

/// `contorno-indicator --self-test <photo> <dir>`: import the photo and check the
/// result and the readout; import while an analysis runs; export, check the
/// zip and open it again without an analysis; save screenshots of the window, the
/// maximised window and the Set Up dialog in the dark theme in <dir>.
/// Exits with 77 (skipped) when the AI outline is unavailable.
int self_test(MainWindow& window, std::vector<std::string> const& args) {
    if (args.size() < 4) {
        std::fprintf(stderr, "usage: contorno-indicator --self-test <photo> <dir>\n");
        return 2;
    }
    auto const photo = path_from_utf8(args[2]);
    auto const out = path_from_utf8(args[3]);
    std::error_code ignored;
    std::filesystem::create_directories(out, ignored);
    if (window.GetIcons().IsEmpty())
        return fail("icon", "the window has no icon");
    bool wordmark = false;
    for (wxWindow* child : window.panel().GetChildren()) {
        if (auto* logo = dynamic_cast<wxStaticBitmap*>(child))
            wordmark = wordmark || logo->GetBitmap().IsOk();
    }
    if (!wordmark)
        return fail("wordmark", "the panel shows no wordmark");

    if (!Vision::ai_available()) {
        std::fprintf(stderr, "self-test: skipped: Vision was built without LibTorch\n");
        return skipped;
    }
    auto& display = window.panel().display();
    if (!window.import_photo(photo))
        return fail("import", window);
    if (!window.busy() && (window.message_key() == "message.ai.no_libtorch" ||
                           window.message_key() == "message.ai.no_model")) {
        std::fprintf(stderr, "self-test: skipped: %s\n", window.message().c_str());
        return skipped;
    }
    if (!wait_idle(window))
        return fail("import", window);
    auto const& analysis = window.analysis();
    if (!analysis || analysis->objects.empty() || !(analysis->area_mm2 > 0)) {
        return fail("result", "no object or no area found");
    }
    if (!readout_matches(window))
        return fail("readout", utf8(display.text()) + " for the result's area");

    // import while busy: the readout never shows the result of the photo before
    if (!window.import_photo(photo))
        return fail("import while busy", window);
    if (!window.busy())
        return fail("import while busy", "no analysis started");
    if (!window.import_photo(photo))
        return fail("import while busy", window);
    if (!(display.text() == "-----" || display.dimmed())) {
        return fail("import while busy", "the readout shows " + utf8(display.text()));
    }
    if (!wait_idle(window))
        return fail("import while busy", window);
    if (!readout_matches(window))
        return fail("import while busy", utf8(display.text()));

    // export: the entries of Inspection.EntryNames, one DXF shape per ring
    auto const shown = *window.analysis();
    auto const zip = out / "self_test.zip";
    std::filesystem::remove(zip, ignored);
    if (!window.export_inspection(zip))
        return fail("export", window);
    auto entries = Tools::read_zip(utf8(zip));
    if (entries.isError())
        return fail("export", entries.error().text());
    std::vector<std::string> names;
    for (auto const& e : entries())
        names.push_back(e.name);
    std::string suffix = utf8(photo.extension());
    for (char& c : suffix)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    std::vector<std::string> const expected {"original" + suffix, "mask.png",    "camera.json",
                                             "parameters.json",   "objects.dxf", "result.json"};
    if (names != expected) {
        std::string list;
        for (auto const& name : names)
            list += (list.empty() ? "" : ", ") + name;
        return fail("export", "entries " + list);
    }
    auto rings = Tools::parse_dxf(entries()[4].data);
    if (rings.isError())
        return fail("objects.dxf", rings.error().text());
    if (rings().size() != ring_count(shown)) {
        return fail("objects.dxf", std::to_string(rings().size()) + " shapes for " +
                                       std::to_string(ring_count(shown)) + " rings");
    }

    // open: the saved result, no analysis
    if (!window.open_inspection(zip))
        return fail("open", window);
    if (window.busy())
        return fail("open", "an analysis started");
    auto const& reopened = window.analysis();
    if (!reopened || reopened->area_mm2 != shown.area_mm2 ||
        reopened->objects.size() != shown.objects.size()) {
        return fail("open", "not the exported result");
    }
    for (std::size_t i = 0; i < shown.objects.size(); ++i) {
        if (reopened->objects[i].outer != shown.objects[i].outer ||
            reopened->objects[i].holes != shown.objects[i].holes) {
            return fail("open", "object " + std::to_string(i) + " differs");
        }
    }
    if (!readout_matches(window))
        return fail("open readout", utf8(display.text()));

    // the last message laid out, the log's scroll and the readout's count ended
    pump_until([] { return false; }, std::chrono::seconds(1));
    ensure_image_handlers();
    wxImage const shot = screenshot(window);
    if (!shot.IsOk() || !shot.SaveFile(wx(utf8(out / "self_test_window.png")), wxBITMAP_TYPE_PNG)) {
        return fail("screenshot", utf8(out));
    }

    // maximised, as the program starts: the panel whole under the photos
    window.Maximize();
    pump_until([] { return false; }, std::chrono::seconds(1));
    wxImage const maximised = screenshot(window);
    if (!maximised.IsOk() ||
        !maximised.SaveFile(wx(utf8(out / "self_test_maximised.png")), wxBITMAP_TYPE_PNG)) {
        return fail("maximised screenshot", utf8(out));
    }

    // the Set Up dialog in the dark theme: its title bar and fields
    auto* dialog = new CameraDialog(&window, {}, wxSize(), false, "en", false);
    dialog->Show();
    pump_until([] { return false; }, std::chrono::seconds(1));
    wxImage const dark = screenshot(*dialog);
    probe_dark_fields(window, out);  // DIAGNOSTIC (temporary)
    dialog->Destroy();
    if (!dark.IsOk() ||
        !dark.SaveFile(wx(utf8(out / "self_test_setup_dark.png")), wxBITMAP_TYPE_PNG))
        return fail("dialog screenshot", utf8(out));
    std::fprintf(stderr, "self-test: ok: %zu object(s), %s %s\n", shown.objects.size(),
                 utf8(display.text()).c_str(), utf8(display.unit()).c_str());
    return 0;
}

}  // namespace

/// The application object: the main window, a photo given on the command
/// line, `--version`, and `--self-test <photo> <dir>`.
class IndicatorApp : public wxApp {
public:
    /// The name the desktop pairs the window with its desktop entry by
    /// (contorno-indicator.desktop): the window's WM_CLASS on X11, the application id under
    /// Wayland. The settings file does not depend on it.
    IndicatorApp() {
        SetAppName("contorno-indicator");
        SetClassName("contorno-indicator");
#ifdef INDICATOR_GTK3
        g_set_prgname("contorno-indicator");
#endif
    }
    bool OnInit() override {
        std::vector<std::string> args;
        for (int i = 0; i < argc; ++i)
            args.push_back(utf8(argv[i]));
        if (args.size() > 1 && args[1] == "--version") {
            std::printf("Contorno Indicator %s\n", INDICATOR_VERSION);
            return false;
        }
        bool const testing = args.size() > 1 && args[1] == "--self-test";
        // self-tests never touch the user's settings
        std::shared_ptr<SettingsStore> store;
        if (testing) {
            store = std::make_shared<MemorySettingsStore>();
        } else {
            store = std::make_shared<WxConfigStore>(default_settings_file());
            // Linux: the package's samples, manual and articles into ~/Documents/Contorno
            std::string reason;
            auto const app_dir =
                path_from_utf8(utf8(wxStandardPaths::Get().GetExecutablePath())).parent_path();
            if (!fill_data_folder(app_dir, user_data_folder(), reason))
                std::fprintf(stderr, "Indicator: data folder: %s\n", reason.c_str());
        }
        register_indicator_strings();
        install_wx_catalogues();
        apply_language(load_indicator_settings(*store).language);
        auto* window = new MainWindow(
            store, testing ? wxDEFAULT_FRAME_STYLE : wxDEFAULT_FRAME_STYLE | wxMAXIMIZE);
        if (testing) {
            window->Show();
            CallAfter([this, window, args] {
                exit_code_ = self_test(*window, args);
                window->Destroy();
                ExitMainLoop();
            });
            return true;
        }
        // Map first, then maximize. On Wayland, gtk_window_resize from the
        // constructor's client size (or from Show applying that size) clears a
        // maximize requested before the window is mapped, so the window stayed
        // at its restored size.
        window->Show();
        window->Maximize();
        if (args.size() > 1)
            window->import_photo(path_from_utf8(args[1]));
        return true;
    }
    int OnRun() override {
        int const code = wxApp::OnRun();
        return exit_code_ >= 0 ? exit_code_ : code;
    }
    /// wx 3.2 takes the direction from the locale and has no setter: the interface language's.
    wxLayoutDirection GetLayoutDirection() const override { return language_layout_direction(); }

private:
    int exit_code_ = -1;  ///< the self-test's
};

wxIMPLEMENT_APP(IndicatorApp);
