#include <gtest/gtest.h>

#include <wx/filectrl.h>
#include <wx/generic/filectrlg.h>
#include <wx/image.h>
#include <wx/mstream.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>

#include "LegacyInspection.h"
#include "TempDir.h"
#include "TestImage.h"
#include "model/ImageSize.h"
#include "model/IndicatorSettings.h"
#include "model/Inspection.h"
#include "model/Language.h"
#include "model/CameraMessage.h"
#include "model/Measure.h"
#include "model/Utf8Path.h"
#include "ui/CameraDialog.h"
#include "ui/ControlPanel.h"
#include "ui/Localise.h"
#include "ui/MainWindow.h"
#include "ui/PhotoFile.h"
#include "ui/PhotoView.h"
#include "ui/ProgressBar.h"
#include "ui/Text.h"
#include "ui/Theme.h"
#include "ui/WxConfigStore.h"
#include "ui/WxTestApp.h"

#include <Tools/Zip.h>
#include <Vision/Objects.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace test_Window {

namespace {

using namespace std::chrono_literals;

std::filesystem::path const calf = std::filesystem::path(INDICATOR_SAMPLES_DIR) / "leather" / "calf_black.jpg";

bool model_available() { return Vision::ai_available() && std::filesystem::exists(INDICATOR_DEFAULT_MODEL); }

bool contains(std::string const& text, std::string const& part) { return text.find(part) != std::string::npos; }
bool starts_with(std::string const& text, std::string const& start) { return text.rfind(start, 0) == 0; }
bool ends_with(std::string const& text, std::string const& end) {
    return text.size() >= end.size() && text.compare(text.size() - end.size(), end.size(), end) == 0;
}

/// Back to English and the dark theme when a test ends, whatever it did.
struct Restore {
    ~Restore() {
        apply_language("en");
        set_light_theme(false);
    }
};

/// A main window with settings of its own (a file in a fresh folder, never the user's), destroyed when the
/// test ends. The window is not shown unless the test shows it.
struct Window {
    Restore restore;
    TempDir dir;
    std::shared_ptr<WxConfigStore> store = std::make_shared<WxConfigStore>(dir.path / "indicator.ini");
    MainWindow* window = new MainWindow(store);

    Window() = default;
    Window(Window const&) = delete;
    Window& operator=(Window const&) = delete;
    ~Window() {
        MainWindow* const gone = window;
        gone->Destroy();
        pump_until([gone] { return !wxTopLevelWindows.Find(gone); }, 30s);
    }
    MainWindow* operator->() const { return window; }
    MainWindow& operator*() const { return *window; }
    IndicatorSettings settings() const { return load_indicator_settings(*store); }
    /// Shown at `size` (client area), its layout done.
    void show(wxSize size) {
        window->SetClientSize(size);
        window->Show();
        pump_until([&] { return window->IsShownOnScreen() && window->GetClientSize() == size; }, 3s);
        window->Layout();
        pump_until([] { return false; }, 100ms);
    }
    void resize(wxSize size) {
        window->SetClientSize(size);
        pump_until([&] { return window->GetClientSize() == size; }, 3s);
        window->Layout();
        pump_until([] { return false; }, 100ms);
    }
};

/// Waits for the window's next EVT_ANALYSED; false on timeout.
bool wait_analysed(MainWindow& window, std::chrono::milliseconds timeout = 300s) {
    struct Arrival {
        bool arrived = false;
        void on(wxCommandEvent& event) {
            arrived = true;
            event.Skip();
        }
    } arrival;
    window.Bind(EVT_ANALYSED, &Arrival::on, &arrival);
    bool const arrived = pump_until([&] { return arrival.arrived; }, timeout);
    window.Unbind(EVT_ANALYSED, &Arrival::on, &arrival);
    return arrived;
}

/// The panel's mask box as an image.
wxImage mask_box(MainWindow& window) { return window.panel().mask().render_image(); }

/// The photo boxes above the panel: the original and the processed photo.
std::vector<PhotoView*> photo_boxes(MainWindow& window) {
    std::vector<PhotoView*> all, boxes;
    collect_descendants(&window, all);
    for (auto* box : all) {
        if (box != &window.panel().mask()) boxes.push_back(box);
    }
    return boxes;
}

/// The mask box shows an object: some opaque black pixel (the rest is transparent).
bool has_object(wxImage const& image) {
    if (!image.IsOk()) return false;
    for (int y = 0; y < image.GetHeight(); ++y) {
        for (int x = 0; x < image.GetWidth(); ++x) {
            bool const opaque = !image.HasAlpha() || image.GetAlpha(x, y) == 255;
            if (opaque && image.GetRed(x, y) == 0 && image.GetGreen(x, y) == 0 && image.GetBlue(x, y) == 0) return true;
        }
    }
    return false;
}

/// The four keys in the order Import, Open, Save, Set up.
std::vector<Key*> keys_of(ControlPanel& panel) {
    std::vector<Key*> keys;
    for (wxWindowID id : {ID_IMPORT, ID_OPEN, ID_SAVE, ID_SETUP}) keys.push_back(dynamic_cast<Key*>(panel.FindWindow(id)));
    return keys;
}

/// The Set up key pressed: the dialog opens (driven by drive_modal) and closes before this returns.
void press_set_up(MainWindow& window) { keys_of(window.panel())[3]->Click(); }

/// `child`'s rectangle in `top`'s coordinates, from the positions of the windows between them.
wxRect rect_in(wxWindow* top, wxWindow* child) {
    wxPoint at;
    for (wxWindow* w = child; w && w != top; w = w->GetParent()) at += w->GetPosition();
    return wxRect(at, child->GetSize());
}

template <class T>
T* named(wxWindow& dialog, wxString const& name) {
    return dynamic_cast<T*>(wxWindow::FindWindowByName(name, &dialog));
}

/// The OK (`ok`) or Cancel key of a dialog pressed.
void answer(wxDialog& dialog, bool ok) { dynamic_cast<Key*>(dialog.FindWindow(ok ? wxID_OK : wxID_CANCEL))->Click(); }

/// A `w` x `h` JPEG in one brown, as written by wx (no EXIF), as bytes.
std::string jpeg_bytes(int w, int h) {
    ensure_image_handlers();
    wxImage image(w, h);
    image.SetRGB(wxRect(0, 0, w, h), 120, 90, 60);
    wxMemoryOutputStream out;
    if (!image.SaveFile(out, wxBITMAP_TYPE_JPEG)) return {};
    std::string jpeg(out.GetLength(), '\0');
    out.CopyTo(jpeg.data(), jpeg.size());
    return jpeg;
}

/// A `w` x `h` JPEG with an APP1 Exif segment holding `tags`. False when it cannot be written.
bool write_jpeg_with_exif(std::filesystem::path const& path, int w, int h, std::vector<ExifTag> const& tags) {
    std::string const jpeg = jpeg_bytes(w, h);
    return !jpeg.empty() && write_bytes(path, jpeg_with_exif(jpeg, tags));
}

/// The inspection zip `folder`/`name` of `photo` with the saved result `saved`, the lens and the minimum area.
std::filesystem::path saved_inspection(std::filesystem::path const& folder, char const* name,
                                       std::filesystem::path const& photo, Vision::ObjectsAnalysis const& saved,
                                       Vision::LensCamera lens = {}, double min_area_mm2 = 100) {
    InspectionFiles files;
    files.photo_path = photo;
    files.camera.lens = lens;
    files.min_area_mm2 = min_area_mm2;
    auto entries = inspection_entries(files, saved);
    EXPECT_FALSE(entries.isError()) << entries.error().english();
    if (entries.isError()) return {};
    auto const zip = folder / name;
    EXPECT_FALSE(Tools::write_zip(utf8(zip), entries()).isError());
    return zip;
}

/// The calf photo's result as the Qt tests make it: a 200 x 100 px rectangle, `sx` x `sy` mm/px.
Vision::ObjectsAnalysis calf_rectangle(double sx, double sy) {
    Vision::ObjectsAnalysis saved;
    saved.frame = {1334, 1602, sx, sy};  // the calf photo's size
    saved.mask = {1334, 1602, std::vector<std::uint8_t>(1334 * 1602, 0)};
    for (int row = 100; row < 200; ++row) {
        for (int col = 100; col < 300; ++col) saved.mask.data[static_cast<std::size_t>(row * 1334 + col)] = 255;
    }
    saved.area_mm2 = 200 * 100 * sx * sy;
    auto const mm = [](double v) { return static_cast<std::int64_t>(v * Pot::units_per_mm); };
    // the Qt tests' outlines: the same rectangle at 0.5 mm/px, a taller one at 0.5 x 0.6
    if (sx == sy) {
        saved.objects = {Pot::Polygon64 {{{mm(50), mm(75)}, {mm(150), mm(75)}, {mm(150), mm(125)}, {mm(50), mm(125)}}, {}}};
    } else {
        saved.objects = {Pot::Polygon64 {{{mm(50), mm(60)}, {mm(150), mm(60)}, {mm(150), mm(120)}, {mm(50), mm(120)}}, {}}};
    }
    return saved;
}

/// Whether the adapter's raw fallback appears at any level of the message.
bool has_raw(Message const& message) {
    if (message.key() == "common.raw") return true;
    for (auto const& inner : message.nested()) {
        if (has_raw(inner)) return true;
    }
    return false;
}

/// The wordmark's colour: its first opaque pixel.
wxColour wordmark_colour(MainWindow& window) {
    std::vector<wxStaticBitmap*> logos;
    collect_descendants(&window.panel(), logos);
    for (auto* logo : logos) {
        wxImage const image = logo->GetBitmap().ConvertToImage();
        for (int y = 0; y < image.GetHeight(); ++y) {
            for (int x = 0; x < image.GetWidth(); ++x) {
                if (!image.HasAlpha() || image.GetAlpha(x, y) == 255) {
                    return wxColour(image.GetRed(x, y), image.GetGreen(x, y), image.GetBlue(x, y));
                }
            }
        }
    }
    return {};
}

}  // namespace

TEST(WxWindow, HasNoMenuBar) {
    Window window;
    EXPECT_EQ(window->GetMenuBar(), nullptr);
}

TEST(WxWindow, TwoSquareImageBoxes) {
    Window window;
    window.show(wxSize(1500, 1000));
    auto const boxes = photo_boxes(*window);
    ASSERT_EQ(boxes.size(), 2u);
    wxWindow* const row = boxes[0]->GetParent();
    int const row_width = row->GetSize().x;
    ASSERT_GT(row_width, 1000);
    for (int i = 0; i < 2; ++i) {
        EXPECT_EQ(boxes[i]->GetSize().x, boxes[i]->GetSize().y) << i;
        EXPECT_NEAR(boxes[i]->GetSize().x, (row_width - 3 * 8) / 2, 1) << i;  // half of the row, less three gaps
        EXPECT_EQ(boxes[i]->GetPosition().y, boxes[0]->GetPosition().y) << i;  // side by side
        if (i > 0) {
            EXPECT_GT(boxes[i]->GetPosition().x, boxes[i - 1]->GetPosition().x + boxes[i - 1]->GetSize().x - 1) << i;
        }
    }
    // centred: the gaps before, between and after the boxes are equal
    int const before = boxes[0]->GetPosition().x;
    int const between = boxes[1]->GetPosition().x - (boxes[0]->GetPosition().x + boxes[0]->GetSize().x);
    int const after = row_width - (boxes[1]->GetPosition().x + boxes[1]->GetSize().x);
    EXPECT_NEAR(before, between, 1);
    EXPECT_NEAR(after, between, 1);
    // a high window: the squares stay at the top, the room below them empty
    window.resize(wxSize(900, 1000));
    EXPECT_EQ(boxes[0]->GetSize().x, boxes[0]->GetSize().y);
    EXPECT_EQ(boxes[0]->GetPosition().y, 0);
    EXPECT_LT(boxes[0]->GetPosition().y + boxes[0]->GetSize().y, row->GetSize().y);
    // a low window: the squares shrink to its height and stay square
    window.resize(wxSize(1500, 500));
    EXPECT_EQ(boxes[0]->GetSize().x, boxes[0]->GetSize().y);
    EXPECT_LE(boxes[0]->GetSize().y, row->GetSize().y);
    EXPECT_LT(boxes[0]->GetSize().x, (row->GetSize().x - 3 * 8) / 2 - 1);  // smaller than half the row
}

TEST(WxWindow, LogInThePanelProgressFillsStatusBar) {
    Window window;
    window.show(wxSize(1500, 1000));
    auto* log = find_descendant<MessageLog>(window.window);
    ASSERT_NE(log, nullptr);
    EXPECT_EQ(utf8(log->last_message()), window->message());
    // a log: each message a line of its own, the newest last
    std::size_t const lines = log->line_count();
    window->Hide();  // a visible window shows a failed import in a dialog too
    EXPECT_FALSE(window->import_photo("/nonexistent/photo.jpg"));
    EXPECT_EQ(log->line_count(), lines + 1);
    EXPECT_TRUE(ends_with(utf8(log->line_text(log->line_count() - 1)), window->message()));
    EXPECT_TRUE(starts_with(window->message(), "Cannot read /nonexistent/photo.jpg")) << window->message();
    window->Show();
    int row = 0;
    log->GetTextExtent("Ag", nullptr, &row, nullptr, nullptr, &log->text_font());
    EXPECT_GE(log->GetSize().y, 3 * row);  // several lines shown
    // the newest line glides into view: at the bottom once the scroll ends
    for (int i = 0; i < 12; ++i) window->panel().log().add(Message("common.raw").with("text", "line " + std::to_string(i)));
    pump_until([] { return false; }, 1000ms);
    EXPECT_GT(log->scroll_range(), 0);
    EXPECT_NEAR(log->scroll_position(), log->scroll_range(), 0.5);
    EXPECT_EQ(log->GetParent(), &window->panel());
    // left of the readout, right of the mask
    auto& mask = window->panel().mask();
    EXPECT_GT(log->GetPosition().x, mask.GetPosition().x + mask.GetSize().x - 1);
    EXPECT_LT(log->GetPosition().x + log->GetSize().x, window->panel().display().GetPosition().x + 1);
    auto* bar = find_descendant<ProgressBar>(window.window);
    ASSERT_NE(bar, nullptr);
    EXPECT_GT(bar->GetSize().x, bar->GetParent()->GetSize().x - 40);
    EXPECT_GT(bar->GetParent()->GetSize().x, window->GetClientSize().x - 2);  // the strip is the window's width
    EXPECT_GT(bar->GetPosition().y + bar->GetParent()->GetPosition().y, window->panel().GetPosition().y);  // under the panel
}

TEST(WxWindow, FullBarFadesToBlackAfterASecond) {
    Window window;
    window->set_model_candidates_for_test("", "");
    auto* bar = find_descendant<ProgressBar>(window.window);
    ASSERT_NE(bar, nullptr);
    EXPECT_TRUE(bar->empty());  // empty at first
    // no model: the analysis stops at once, the bar full. Its clock starts inside the import.
    ASSERT_TRUE(window->import_photo(calf));
    EXPECT_TRUE(bar->full());
    auto const start = std::chrono::steady_clock::now();  // already full: samples follow the bar from here
    bar->SetSize(400, 20);
    auto lit = [&] { return bar->render(1).ConvertToImage().GetGreen(200, 10); };
    int const full = lit();
    EXPECT_GT(full, 200);
    auto elapsed = [&] { return std::chrono::steady_clock::now() - start; };
    auto wait_until = [&](int ms) { pump_until([&] { return elapsed() >= std::chrono::milliseconds(ms); }, 5s); };
    wait_until(800);
    bool const full_now = bar->full();
    bool const empty_now = bar->empty();
    double const level_now = bar->level();
    int const lit_now = lit();
    // still fully lit within the second; checked only when the loop came back in time (a busy machine may oversleep)
    if (elapsed() < std::chrono::milliseconds(ProgressBar::full_ms)) {
        EXPECT_TRUE(full_now);
        EXPECT_FALSE(empty_now);
        EXPECT_DOUBLE_EQ(level_now, 1);
        EXPECT_EQ(lit_now, full);
    }
    wait_until(1400);  // then the green dims, the bar still full
    bool const dim_full = bar->full();
    bool const dim_empty = bar->empty();
    double const dim_level = bar->level();
    int const dimmed = lit();
    // dimmed but still full; a wake after that part ended must not fail
    if (elapsed() < std::chrono::milliseconds(ProgressBar::full_ms + ProgressBar::fade_ms)) {
        EXPECT_TRUE(dim_full);
        EXPECT_FALSE(dim_empty);
        EXPECT_LT(dim_level, 1.0);
        EXPECT_GT(dim_level, 0.0);
        EXPECT_LT(dimmed, full - 30);
        // mid-fade green, with the same 200 ms slack as the lit sample; later in the fade it is simply darker
        if (elapsed() < std::chrono::milliseconds(ProgressBar::full_ms + ProgressBar::fade_ms / 2 + 200)) {
            EXPECT_GT(dimmed, 20);
        }
    }
    wait_until(2200);
    EXPECT_TRUE(bar->empty());  // black: empty, and a late wake still finds it empty
    EXPECT_FALSE(bar->full());
    EXPECT_LT(lit(), 20);
}

TEST(WxWindow, NoCaptions) {
    Window window;
    std::vector<wxStaticText*> labels;
    collect_descendants(window.window, labels);
    for (auto const* label : labels) {
        for (char const* caption : {"Raw photo", "Mask", "Processed photo"}) EXPECT_NE(label->GetLabel(), caption);
    }
}

TEST(WxWindow, PanelControls) {
    Window window;
    window.show(wxSize(1500, 1000));
    auto& panel = window->panel();
    // four square keys: Import over Open, Save over "Set up"
    auto const keys = keys_of(panel);
    std::vector<std::string> labels;
    for (Key* key : keys) labels.push_back(utf8(key->GetLabelText()));
    ASSERT_EQ(labels, (std::vector<std::string> {"Import", "Open", "Save", "Set up"}));
    for (Key const* key : keys) EXPECT_EQ(key->GetSize().x, key->GetSize().y) << utf8(key->GetLabelText());
    Key* const import = keys[0];
    Key* const open = keys[1];
    Key* const save = keys[2];
    Key* const setup = keys[3];
    EXPECT_EQ(import->GetPosition().x, open->GetPosition().x);
    EXPECT_LT(import->GetPosition().y, open->GetPosition().y);
    EXPECT_EQ(save->GetPosition().x, setup->GetPosition().x);
    EXPECT_LT(save->GetPosition().y, setup->GetPosition().y);
    EXPECT_GT(save->GetPosition().x, import->GetPosition().x);
    EXPECT_EQ(import->GetPosition().y, save->GetPosition().y);

    // the mask: square, as high as the readout, right of the keys
    auto const& mask = panel.mask();
    EXPECT_EQ(mask.GetParent(), &panel);
    EXPECT_EQ(mask.GetSize().x, mask.GetSize().y);
    EXPECT_EQ(mask.GetSize().y, panel.display().GetSize().y);
    EXPECT_GT(mask.GetPosition().x, save->GetPosition().x);

    std::vector<SegmentDisplay*> displays;
    collect_descendants(&panel, displays);
    ASSERT_EQ(displays.size(), 1u);
    EXPECT_EQ(displays[0]->text(), "-----");

    // the wordmark in white on the black window: no dark opaque pixel
    wxImage logo;
    std::vector<wxStaticBitmap*> bitmaps;
    collect_descendants(&panel, bitmaps);
    for (auto* b : bitmaps) {
        if (b->GetBitmap().IsOk()) logo = b->GetBitmap().ConvertToImage();
    }
    ASSERT_TRUE(logo.IsOk());
    int white = 0;
    for (int y = 0; y < logo.GetHeight(); ++y) {
        for (int x = 0; x < logo.GetWidth(); ++x) {
            if (logo.HasAlpha() && logo.GetAlpha(x, y) < 128) continue;
            int const grey = (logo.GetRed(x, y) + logo.GetGreen(x, y) + logo.GetBlue(x, y)) / 3;
            EXPECT_GT(grey, 200) << x << ", " << y;
            if (grey <= 200) return;  // one report is enough
            ++white;
        }
    }
    EXPECT_GT(white, 0);
}

TEST(WxWindow, UnavailableSaysWhy) {
    Window window;
    window->set_model_candidates_for_test("/nonexistent/configured.pt", "/nonexistent/default.pt");
    ASSERT_TRUE(window->import_photo(calf));
    EXPECT_TRUE(starts_with(window->message(), "AI outline unavailable")) << window->message();
    EXPECT_EQ(window->panel().display().text(), "-----");
    EXPECT_FALSE(window->busy());
    if (Vision::ai_available()) {
        EXPECT_TRUE(contains(window->message(), "/nonexistent/configured.pt"));
        EXPECT_TRUE(contains(window->message(), "/nonexistent/default.pt"));
        EXPECT_TRUE(ends_with(window->message(), " — build the isnet_model target.")) << window->message();
    }
}

// with the model: the readout shows the area of the outline drawn on the right
TEST(WxWindow, ResultShowsArea) {
    if (!model_available()) GTEST_SKIP();
    Window window;
    ASSERT_TRUE(window->import_photo(calf));
    EXPECT_TRUE(window->busy());
    EXPECT_EQ(window->panel().display().text(), "-----");
    ASSERT_TRUE(wait_analysed(*window));
    ASSERT_TRUE(window->analysis()) << window->last_error();
    auto const& a = *window->analysis();
    EXPECT_EQ(a.objects.size(), 1u);
    auto const area = format_area(a.area_mm2);
    EXPECT_EQ(utf8(window->panel().display().text()), area.digits);
    EXPECT_EQ(utf8(window->panel().display().unit()), area.unit);
    EXPECT_FALSE(window->panel().display().dimmed());
    EXPECT_EQ(window->analysed_min_area_mm2(), window.settings().min_area_mm2);
    EXPECT_TRUE(starts_with(window->message(), "1 object · ")) << window->message();
    // the outline is drawn on the right photo and on the mask
    EXPECT_EQ(photo_boxes(*window)[1]->polygon_layers(), 1u);
    EXPECT_EQ(window->panel().mask().polygon_layers(), 1u);
}

// when the object found from the background replaced the model's region, a line after the result says so
TEST(WxWindow, OutlineFromTheBackgroundIsReported) {
    if (!model_available()) GTEST_SKIP();
    Window window;
    ASSERT_TRUE(window->import_photo(std::filesystem::path(INDICATOR_SAMPLES_DIR) / "leather" / "calf_spotted.jpg"));
    ASSERT_TRUE(wait_analysed(*window));
    ASSERT_TRUE(window->analysis()) << window->last_error();
    ASSERT_TRUE(window->analysis()->from_background);
    EXPECT_EQ(window->message(), Message("message.from_background").render()) << window->message();
    EXPECT_TRUE(contains(utf8(window->panel().log().text()), "1 object · "));
}

// a new photo: the readout keeps the area shown, dimmed, and counts from it to the new one
TEST(WxWindow, ImportCountsFromTheAreaShown) {
    if (!model_available()) GTEST_SKIP();
    Window window;
    ASSERT_TRUE(window->import_photo(calf));
    ASSERT_TRUE(wait_analysed(*window));
    ASSERT_TRUE(window->analysis()) << window->last_error();
    auto& display = window->panel().display();
    wxString const before = display.text();

    ASSERT_TRUE(window->import_photo(std::filesystem::path(INDICATOR_SAMPLES_DIR) / "leather" / "indian_lamb.jpg"));
    EXPECT_EQ(display.text(), before);  // not dashes: the count starts there
    EXPECT_TRUE(display.dimmed());
    ASSERT_TRUE(wait_analysed(*window));
    ASSERT_TRUE(window->analysis()) << window->last_error();
    EXPECT_EQ(utf8(display.text()), format_area(window->analysis()->area_mm2).digits);
    EXPECT_FALSE(display.dimmed());
    ASSERT_TRUE(display.counting());
    EXPECT_NE(display.shown_text(), "-----");
}

TEST(WxWindow, MaskImageIsGrey) {
    Vision::Mask mask {3, 2, {0, 255, 0, 255, 0, 0}};
    wxImage const image = mask_image(mask);
    ASSERT_TRUE(image.IsOk());
    ASSERT_EQ(image.GetSize(), wxSize(3, 2));
    EXPECT_FALSE(image.HasAlpha());
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 3; ++x) {
            EXPECT_EQ(image.GetRed(x, y), image.GetGreen(x, y));  // grey
            EXPECT_EQ(image.GetRed(x, y), image.GetBlue(x, y));
        }
    }
    EXPECT_EQ(image.GetRed(0, 0), 0);
    EXPECT_EQ(image.GetRed(1, 0), 255);
    EXPECT_EQ(image.GetRed(0, 1), 255);
    EXPECT_EQ(image.GetRed(2, 1), 0);
}

// the mask box and the readout always come from the same result: empty and dashes without one
TEST(WxWindow, ErrorShowsBlackMask) {
    if (!model_available()) GTEST_SKIP();
    Window window;
    ASSERT_TRUE(window->import_photo(calf));
    ASSERT_TRUE(wait_analysed(*window));
    ASSERT_TRUE(window->analysis()) << window->last_error();
    ASSERT_TRUE(has_object(mask_box(*window)));

    // the analysis fails: a file that is not a model
    window->set_model_candidates_for_test(calf, "");
    ASSERT_TRUE(window->import_photo(calf));
    ASSERT_TRUE(window->busy());
    ASSERT_TRUE(wait_analysed(*window));
    EXPECT_FALSE(window->analysis());
    EXPECT_FALSE(window->last_error().empty());
    wxImage const empty = mask_box(*window);
    EXPECT_EQ(empty.GetSize(), wxSize(1334, 1602));  // the photo's size, all transparent
    EXPECT_FALSE(has_object(empty));
    EXPECT_EQ(window->panel().display().text(), "-----");

    // a result again, then the AI is unavailable
    window->set_model_candidates_for_test("", INDICATOR_DEFAULT_MODEL);
    ASSERT_TRUE(window->import_photo(calf));
    ASSERT_TRUE(wait_analysed(*window));
    ASSERT_TRUE(window->analysis());
    window->set_model_candidates_for_test("/nonexistent/configured.pt", "/nonexistent/default.pt");
    ASSERT_TRUE(window->import_photo(calf));
    EXPECT_FALSE(window->busy());
    EXPECT_FALSE(window->analysis());
    EXPECT_FALSE(has_object(mask_box(*window)));
    EXPECT_EQ(window->panel().display().text(), "-----");
}

// the photo's own camera data comes first, and the log says so
TEST(WxWindow, ImportUsesPhotoMetadata) {
    TempDir dir;
    auto const photo = dir.path / "exif.jpg";
    ASSERT_TRUE(write_jpeg_with_exif(photo, 400, 300, {{0x920A, 5, 42, 10}, {0xA20E, 5, 1000000, 1400}, {0xA210, 3, 4}}));
    Window window;
    window->set_model_candidates_for_test("", "");
    ASSERT_TRUE(window->import_photo(photo));
    EXPECT_EQ(window->camera().lens.focal_length_mm, 4.2);
    EXPECT_NEAR(window->camera().lens.pixel_width_um, 1.4, 1e-9);
    EXPECT_EQ(window->camera().focal_source, Vision::CameraSource::photo);
    EXPECT_TRUE(contains(utf8(window->panel().log().text()), "from the photo"));
}

TEST(WxWindow, ImportWithoutMetadataUsesSettings) {
    TempDir dir;
    auto const photo = dir.path / "plain.jpg";
    std::string const jpeg = jpeg_bytes(400, 300);  // the sensor's aspect ratio
    ASSERT_FALSE(jpeg.empty());
    ASSERT_TRUE(write_bytes(photo, jpeg));
    Window window;
    window->set_model_candidates_for_test("", "");
    ASSERT_TRUE(window->import_photo(photo));
    EXPECT_EQ(window->camera().focal_source, Vision::CameraSource::settings);
    EXPECT_EQ(window->camera().pixel_source, Vision::CameraSource::settings);
    EXPECT_TRUE(contains(utf8(window->panel().log().text()), "no camera data in the photo")) << utf8(window->panel().log().text());
}

namespace {
/// Accepts the Set Up dialog once it is open, the distance one millimetre
/// longer with `change` (a change of the camera), else as it was.
void accept_camera_dialog_later(bool change = true) {
    drive_modal(
        [change](wxDialog& dialog) {
            if (change) {
                auto* distance = named<NumberField>(dialog, "distance");
                distance->set_value(distance->value() + 1);
            }
            answer(dialog, true);
        },
        5000ms, "contorno-setup");
}
}  // namespace

// OK without a change: no log line, no analysis, nothing saved
TEST(WxWindow, CameraDialogUnchangedDoesNothing) {
    Window window;
    window->set_model_candidates_for_test("", "");
    ASSERT_TRUE(window->import_photo(calf));  // no model: the analysis fails at once, logged
    std::size_t const lines = window->panel().log().line_count();
    wxString const text = window->panel().log().text();
    accept_camera_dialog_later(false);
    press_set_up(*window);
    EXPECT_EQ(window->panel().log().line_count(), lines);
    EXPECT_EQ(window->panel().log().text(), text);
    EXPECT_FALSE(window.store->value("camera/distance_mm"));  // nothing saved
}

// no photo shown: nothing was resolved, the log only confirms
TEST(WxWindow, CameraDialogWithoutPhotoSaysSaved) {
    Window window;
    window->set_model_candidates_for_test("", "");
    double const distance = window.settings().camera.distance_mm;
    accept_camera_dialog_later();
    press_set_up(*window);
    EXPECT_TRUE(contains(utf8(window->panel().log().text()), "Camera settings saved."))
        << utf8(window->panel().log().text());
    EXPECT_EQ(window.settings().camera.distance_mm, distance + 1);
}

// a photo shown: the accepted settings are resolved with its metadata again
TEST(WxWindow, CameraDialogReResolvesWithPhoto) {
    TempDir dir;
    auto const photo = dir.path / "exif.jpg";
    ASSERT_TRUE(write_jpeg_with_exif(photo, 400, 300, {{0x920A, 5, 42, 10}}));
    Window window;
    window->set_model_candidates_for_test("", "");
    ASSERT_TRUE(window->import_photo(photo));
    accept_camera_dialog_later();
    press_set_up(*window);
    EXPECT_EQ(window->camera().lens.focal_length_mm, 4.2);
    EXPECT_EQ(window->camera().focal_source, Vision::CameraSource::photo);
    std::string const log = utf8(window->panel().log().text());
    EXPECT_FALSE(contains(log, "Camera settings saved."));
    std::size_t count = 0;
    for (auto at = log.find("Camera: focal length 4.20 mm"); at != std::string::npos;
         at = log.find("Camera: focal length 4.20 mm", at + 1)) {
        ++count;
    }
    EXPECT_EQ(count, 2u) << log;  // the import, then the dialog
}

// a saved inspection keeps its camera, whatever its photo's EXIF says
TEST(WxWindow, OpenedInspectionKeepsSavedCamera) {
    TempDir dir;
    auto const photo = dir.path / "exif.jpg";
    ASSERT_TRUE(write_jpeg_with_exif(photo, 400, 300, {{0x920A, 5, 42, 10}}));
    Vision::ObjectsAnalysis saved;
    saved.frame = {400, 300, 0.5, 0.5};
    saved.mask = {400, 300, std::vector<std::uint8_t>(400 * 300, 0)};
    saved.area_mm2 = 0;
    auto const zip = saved_inspection(dir.path, "kept.zip", photo, saved, Vision::LensCamera {3000, 13.02, 3.76});
    Window window;
    window->set_model_candidates_for_test("", "");
    ASSERT_TRUE(window->open_inspection(zip)) << window->last_error();
    EXPECT_EQ(window->camera().lens.focal_length_mm, 13.02);
    EXPECT_EQ(window->camera().lens.distance_mm, 3000);
}

// an old hide inspection: its camera applies until the next import, which a failed import is not
TEST(WxWindow, FailedImportKeepsState) {
    TempDir dir;
    Vision::CameraConfig const old_camera {3000, 60, 4000};  // twice the default distance
    auto const entries = legacy_inspection_entries(calf, old_camera);
    ASSERT_FALSE(entries.empty());
    auto const zip = dir.path / "old.zip";
    ASSERT_FALSE(Tools::write_zip(utf8(zip), entries).isError());

    Window window;
    window->set_model_candidates_for_test("", model_available() ? INDICATOR_DEFAULT_MODEL : "");
    ASSERT_TRUE(window->open_inspection(zip));
    if (model_available()) {
        ASSERT_TRUE(wait_analysed(*window));
    }
    double const scale = Vision::lens_scale(window->camera().lens).x;
    auto const legacy_scale = Vision::mm_per_px(old_camera, shown_size(calf)->width);  // scaled by the photo's width
    ASSERT_FALSE(legacy_scale.isError());
    EXPECT_NEAR(scale, legacy_scale(), 1e-9);
    wxString const readout = window->panel().display().text();
    bool const had_result = window->analysis().has_value();
    wxString const title = window->GetTitle();

    window->Hide();  // a visible window shows the failure in a dialog too
    EXPECT_FALSE(window->import_photo(dir.path / "missing.jpg"));
    EXPECT_EQ(Vision::lens_scale(window->camera().lens).x, scale);
    EXPECT_EQ(window->panel().display().text(), readout);
    EXPECT_EQ(window->analysis().has_value(), had_result);
    EXPECT_EQ(window->GetTitle(), title);
    EXPECT_EQ(window->busy(), false);
}

// an objects inspection: its saved result is shown, nothing is analysed
TEST(WxWindow, OpenShowsSavedResult) {
    TempDir dir;
    Vision::ObjectsAnalysis const saved = calf_rectangle(0.5, 0.5);
    Vision::LensCamera const lens {3000, 13.02, 3.76};
    auto const zip = saved_inspection(dir.path, "objects.zip", calf, saved, lens, 5000);

    Window window;
    window->set_model_candidates_for_test("", model_available() ? INDICATOR_DEFAULT_MODEL : "");
    ASSERT_TRUE(window->open_inspection(zip)) << window->last_error();
    EXPECT_FALSE(window->busy());
    ASSERT_TRUE(window->analysis());
    EXPECT_EQ(window->analysis()->area_mm2, saved.area_mm2);
    ASSERT_EQ(window->analysis()->objects.size(), 1u);
    EXPECT_EQ(window->analysis()->objects[0].outer, saved.objects[0].outer);
    EXPECT_EQ(utf8(window->panel().display().text()), format_area(saved.area_mm2).digits);
    EXPECT_FALSE(window->panel().display().dimmed());
    EXPECT_EQ(window->analysed_min_area_mm2(), 5000);
    EXPECT_EQ(Vision::lens_scale(window->camera().lens).x, Vision::lens_scale(lens).x);
    EXPECT_TRUE(has_object(mask_box(*window)));
    EXPECT_FALSE(starts_with(window->message(), "AI outline unavailable")) << window->message();
    EXPECT_EQ(utf8(window->GetTitle()), Message("window.title_file").with("file", "objects.zip").render());

    // and exported again as it was opened
    auto const again = dir.path / "again.zip";
    ASSERT_TRUE(window->export_inspection(again)) << window->last_error();
    auto read = Tools::read_zip(utf8(again));
    ASSERT_FALSE(read.isError()) << read.error();
    EXPECT_EQ(read().size(), 6u);
}

TEST(WxWindow, ResultMessageShowsBothScales) {
    TempDir dir;
    auto const zip = saved_inspection(dir.path, "nonsquare.zip", calf, calf_rectangle(0.5, 0.6),
                                      Vision::LensCamera {3000, 13.02, 3.76, 4.20}, 5000);
    Window window;
    window->set_model_candidates_for_test("", model_available() ? INDICATOR_DEFAULT_MODEL : "");
    ASSERT_TRUE(window->open_inspection(zip)) << window->last_error();
    EXPECT_TRUE(contains(window->message(), "0.500 × 0.600 mm/px")) << window->message();
}

// an inspection opened while the analysis of the photo before still runs:
// that analysis is obsolete, so nothing shown is busy and export works
TEST(WxWindow, OpenWhileAnalysingIsNotBusy) {
    if (!model_available()) GTEST_SKIP();
    TempDir dir;
    auto const zip = saved_inspection(dir.path, "objects.zip", calf, calf_rectangle(0.5, 0.5));

    Window window;
    window->set_model_candidates_for_test("", INDICATOR_DEFAULT_MODEL);
    ASSERT_TRUE(window->import_photo(calf));
    ASSERT_TRUE(window->busy());  // its result comes through the event loop, not yet run
    ASSERT_TRUE(window->open_inspection(zip)) << window->last_error();
    EXPECT_FALSE(window->busy());
    auto* bar = find_descendant<ProgressBar>(window.window);
    ASSERT_NE(bar, nullptr);
    EXPECT_FALSE(bar->running()) << "the busy indicator runs";
    EXPECT_TRUE(window->export_inspection(dir.path / "again.zip")) << window->last_error();
}

// a phone photo stored on its side: the saved frame is the size as shown
TEST(WxWindow, OpenRotatedPhoto) {
    TempDir dir;
    auto const photo = dir.path / "rotated.jpg";
    ASSERT_TRUE(write_rotated_jpeg(photo));
    auto const shown = read_photo(photo);
    ASSERT_TRUE(shown.image.IsOk()) << shown.error;
    ASSERT_EQ(shown.image.GetSize(), wxSize(20, 40));  // the test photo is rotated as meant

    Vision::ObjectsAnalysis saved;
    saved.frame = {20, 40, 1, 1};
    saved.mask = {20, 40, std::vector<std::uint8_t>(20 * 40, 0)};
    for (int row = 10; row < 30; ++row) {
        for (int col = 5; col < 15; ++col) saved.mask.data[static_cast<std::size_t>(row * 20 + col)] = 255;
    }
    saved.area_mm2 = 200;
    auto const mm = [](double v) { return static_cast<std::int64_t>(v * Pot::units_per_mm); };
    saved.objects = {Pot::Polygon64 {{{mm(5), mm(10)}, {mm(15), mm(10)}, {mm(15), mm(30)}, {mm(5), mm(30)}}, {}}};
    auto const zip = saved_inspection(dir.path, "rotated.zip", photo, saved);

    Window window;
    ASSERT_TRUE(window->open_inspection(zip)) << window->last_error();
    ASSERT_TRUE(window->analysis());
    EXPECT_EQ(window->analysis()->frame.width, 20);
    EXPECT_EQ(utf8(window->panel().display().text()), format_area(200).digits);
    EXPECT_EQ(photo_boxes(*window)[0]->render_image().GetSize(), wxSize(20, 40));  // shown upright
    // exported again and opened again
    auto const again = dir.path / "again.zip";
    ASSERT_TRUE(window->export_inspection(again)) << window->last_error();
    ASSERT_TRUE(window->open_inspection(again)) << window->last_error();
    EXPECT_EQ(window->analysis()->objects[0].outer, saved.objects[0].outer);
}

// an old hide inspection while the AI is unavailable: the photo, the reason, dashes
TEST(WxWindow, LegacyUnavailableShowsPhoto) {
    TempDir dir;
    auto const entries = legacy_inspection_entries(calf, Vision::CameraConfig {1500, 60, 4000});
    ASSERT_FALSE(entries.empty());
    auto const zip = dir.path / "old.zip";
    ASSERT_FALSE(Tools::write_zip(utf8(zip), entries).isError());

    Window window;
    window->set_model_candidates_for_test("/nonexistent/configured.pt", "/nonexistent/default.pt");
    ASSERT_TRUE(window->open_inspection(zip)) << window->last_error();
    EXPECT_FALSE(window->busy());
    EXPECT_TRUE(starts_with(window->message(), "AI outline unavailable")) << window->message();
    EXPECT_EQ(window->panel().display().text(), "-----");
    EXPECT_FALSE(window->analysis());
    wxImage const left = photo_boxes(*window)[0]->render_image();
    EXPECT_EQ(left.GetSize(), wxSize(1334, 1602));  // the photo is shown
    EXPECT_FALSE(has_object(mask_box(*window)));
}

TEST(WxWindow, OldLinesFollowTheLanguage) {
    Window window;
    window->set_model_candidates_for_test("/nonexistent/configured.pt", "/nonexistent/default.pt");
    ASSERT_TRUE(window->import_photo(calf));  // the camera line is logged without a model
    auto text = [&] { return utf8(window->panel().log().text()); };
    std::string const photo_size = "1334 x 1602 px";
    Message const imported = Message("message.imported").with("name", "calf_black.jpg").with("size", photo_size);
    Message const camera = camera_message(window->camera(), false);
    ASSERT_TRUE(contains(text(), imported.english())) << text();
    ASSERT_TRUE(contains(text(), camera.english())) << text();
    apply_language("fr");
    std::string const all = text();
    EXPECT_TRUE(contains(all, imported.render("fr"))) << all;
    EXPECT_TRUE(contains(all, camera.render("fr"))) << all;
    EXPECT_NE(imported.render("fr"), imported.english());
    EXPECT_NE(camera.render("fr"), camera.english());
    EXPECT_TRUE(contains(all, "calf_black.jpg"));
    EXPECT_TRUE(contains(all, photo_size));
    EXPECT_TRUE(contains(all, "mm/px"));
}

TEST(WxWindow, OpenErrorIsTranslatedAtEveryLevel) {
    TempDir dir;
    auto const zip = dir.path / "not_a_zip.zip";
    ASSERT_TRUE(write_bytes(zip, "this is plain text, not an archive\n"));
    Window window;
    apply_language("de");
    EXPECT_FALSE(window->open_inspection(zip));
    auto const message = window->panel().log().last_text();
    EXPECT_FALSE(has_raw(message)) << message.key();
    EXPECT_TRUE(contains(message.render("de"), "not_a_zip.zip")) << message.render("de");
    EXPECT_NE(message.render("de"), message.english());
}

TEST(WxWindow, ArabicKeepsTheWindowInPlace) {
    Window window;
    window.show(wxSize(1200, 800));
    auto geometries = [&] {
        std::vector<wxRect> out;
        std::vector<PhotoView*> views;
        collect_descendants(window.window, views);
        for (auto* view : views) out.push_back(rect_in(window.window, view));
        std::vector<wxStaticBitmap*> labels;
        collect_descendants(&window->panel(), labels);
        for (auto* label : labels) out.push_back(rect_in(window.window, label));
        for (auto* key : keys_of(window->panel())) out.push_back(rect_in(window.window, key));
        out.push_back(rect_in(window.window, &window->panel().log()));
        out.push_back(rect_in(window.window, &window->panel().display()));
        out.push_back(rect_in(window.window, find_descendant<ProgressBar>(window.window)));
        return out;
    };
    auto const before = geometries();
    ASSERT_EQ(before.size(), 11u);
    apply_language("ar");
    pump_until([] { return false; }, 100ms);
    EXPECT_EQ(geometries(), before);
    EXPECT_EQ(window->GetLayoutDirection(), wxLayout_LeftToRight);
    EXPECT_EQ(window->panel().GetLayoutDirection(), wxLayout_LeftToRight);
    std::vector<wxWindow*> containers;
    for (wxWindow* w = find_descendant<ProgressBar>(window.window); w && w != window.window; w = w->GetParent()) containers.push_back(w);
    for (wxWindow* w = photo_boxes(*window)[0]; w && w != window.window; w = w->GetParent()) containers.push_back(w);
    for (wxWindow* w : containers) EXPECT_EQ(w->GetLayoutDirection(), wxLayout_LeftToRight) << w->GetClassInfo()->GetClassName();
    auto const keys = keys_of(window->panel());
    char const* names[] = {"panel.import", "panel.open", "panel.save", "panel.setup"};
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(keys[i]->GetLayoutDirection(), wxLayout_RightToLeft);
        EXPECT_EQ(utf8(keys[i]->GetLabelText()), Message(names[i]).render("ar"));
    }
}

TEST(WxWindow, TitleFollowsTheLanguage) {
    Window window;
    window->set_model_candidates_for_test("/nonexistent/configured.pt", "/nonexistent/default.pt");
    EXPECT_EQ(utf8(window->GetTitle()), Message("window.title").render("en"));
    ASSERT_TRUE(window->import_photo(calf));
    for (auto const& code : {"en", "it", "ar"}) {
        apply_language(code);
        std::string const title = utf8(window->GetTitle());
        EXPECT_EQ(title, Message("window.title_file").with("file", "calf_black.jpg").render(code));
        EXPECT_TRUE(contains(title, "Contorno Indicator")) << code;
        EXPECT_TRUE(contains(title, "calf_black.jpg")) << code;
    }
}

namespace {
/// Picks the language `code` in the open Set Up dialog, then OK or Cancel.
void pick_language_later(std::string code, bool ok) {
    drive_modal(
        [code, ok](wxDialog& dialog) {
            named<Key>(dialog, wx("language_" + code))->Click();
            answer(dialog, ok);
        },
        5000ms, "contorno-setup");
}
}  // namespace

TEST(WxWindow, SetUpOkAppliesLanguage) {
    Window window;
    window->set_model_candidates_for_test("", "");
    pick_language_later("de", true);
    press_set_up(*window);
    EXPECT_EQ(window.settings().language, "de");
    EXPECT_EQ(current_language(), "de");
    // "Import" is the same word in German; the Open key (second) is not
    EXPECT_EQ(utf8(keys_of(window->panel())[1]->GetLabelText()), Message("panel.open").render("de"));
    EXPECT_NE(Message("panel.open").render("de"), Message("panel.open").render("en"));
    // the language alone: the old lines in it, no new line and no new analysis
    EXPECT_TRUE(contains(utf8(window->panel().log().text()), Message("message.import_first_photo").render("de")))
        << utf8(window->panel().log().text());
    EXPECT_EQ(window->panel().log().line_count(), 1u);
}

TEST(WxWindow, SetUpCancelLeavesNoTrace) {
    Window window;
    window->set_model_candidates_for_test("", "");
    pick_language_later("ar", false);
    press_set_up(*window);
    EXPECT_EQ(current_language(), "en");
    EXPECT_EQ(wxTheApp->GetLayoutDirection(), wxLayout_LeftToRight);
    EXPECT_EQ(utf8(keys_of(window->panel())[0]->GetLabelText()), Message("panel.import").render("en"));
    EXPECT_EQ(window.settings().language, "en");
    bool checked = false;
    drive_modal(
        [&](wxDialog& dialog) {
            checked = true;
            EXPECT_TRUE(named<Key>(dialog, "language_en")->IsChecked());
            EXPECT_FALSE(named<Key>(dialog, "language_ar")->IsChecked());
            answer(dialog, false);
        },
        5000ms, "contorno-setup");
    press_set_up(*window);
    EXPECT_TRUE(checked);
}

namespace {
/// Toggles the Theme key in the open Set Up dialog, then OK or Cancel.
void toggle_theme_later(bool ok) {
    drive_modal(
        [ok](wxDialog& dialog) {
            named<Key>(dialog, "theme")->Click();
            answer(dialog, ok);
        },
        5000ms, "contorno-setup");
}
/// The colour behind the photos, the panel and the busy indicator: each window between them and the frame.
std::vector<wxColour> panel_colours(MainWindow& window) {
    std::vector<wxColour> out {window.panel().GetBackgroundColour()};
    for (wxWindow* w = window.panel().GetParent(); w; w = w->GetParent()) out.push_back(w->GetBackgroundColour());
    for (wxWindow* w = find_descendant<ProgressBar>(&window)->GetParent(); w != &window; w = w->GetParent()) {
        out.push_back(w->GetBackgroundColour());
    }
    for (wxWindow* w = photo_boxes(window)[0]->GetParent(); w != &window; w = w->GetParent()) out.push_back(w->GetBackgroundColour());
    return out;
}
bool all_are(std::vector<wxColour> const& colours, wxColour const& colour) {
    for (auto const& c : colours) {
        if (c != colour) return false;
    }
    return true;
}
}  // namespace

TEST(WxWindow, ThemeLightensOnlyThePanels) {
    Window window;
    window->set_model_candidates_for_test("", "");
    window->SetClientSize(1500, 1000);
    window->Layout();
    EXPECT_TRUE(all_are(panel_colours(*window), *wxBLACK));
    EXPECT_EQ(wordmark_colour(*window), *wxWHITE);
    auto box_colour = [&] {
        wxImage const image = photo_boxes(*window)[0]->render(1).ConvertToImage();
        return wxColour(image.GetRed(20, 20), image.GetGreen(20, 20), image.GetBlue(20, 20));
    };
    wxColour const box = box_colour();
    toggle_theme_later(false);  // Cancel: nothing changes
    press_set_up(*window);
    EXPECT_FALSE(window.settings().light);
    EXPECT_TRUE(all_are(panel_colours(*window), *wxBLACK));
    toggle_theme_later(true);
    press_set_up(*window);
    EXPECT_TRUE(window.settings().light);
    auto const light = panel_colours(*window);
    EXPECT_TRUE(all_are(light, light[0]));
    EXPECT_GT(light[0].GetLuminance(), 200 / 255.0) << utf8(light[0].GetAsString(wxC2S_HTML_SYNTAX));
    EXPECT_EQ(wordmark_colour(*window), *wxBLACK);
    // the photo boxes keep their colour
    EXPECT_EQ(box_colour(), box);
    // the dialogs opened now have the light panel
    EXPECT_TRUE(light_theme());
    toggle_theme_later(true);  // and back
    press_set_up(*window);
    EXPECT_FALSE(window.settings().light);
    EXPECT_TRUE(all_are(panel_colours(*window), *wxBLACK));
    EXPECT_EQ(wordmark_colour(*window), *wxWHITE);
    EXPECT_FALSE(light_theme());
}

namespace {
/// The folder the Import dialog starts in, opened by the window's Import key and cancelled.
wxString import_dialog_folder(MainWindow& window) {
    wxString folder;
    drive_modal([&](wxDialog& dialog) {
        if (auto* files = find_descendant<wxGenericFileCtrl>(&dialog)) folder = files->GetShownDirectory();
        dialog.EndModal(wxID_CANCEL);
    });
    keys_of(window.panel())[0]->Click();
    return folder;
}
/// A window over `store`, destroyed when the scope ends.
struct Over {
    MainWindow* window;
    explicit Over(std::shared_ptr<SettingsStore> store) : window(new MainWindow(std::move(store))) {}
    ~Over() {
        MainWindow* const gone = window;
        gone->Destroy();
        pump_until([gone] { return !wxTopLevelWindows.Find(gone); }, 30s);
    }
};
}  // namespace

// Ruling 8: a remembered folder that is gone starts the file dialogs in the samples (built from the sources)
TEST(WxWindow, MissingLastFolderStartsInTheSamples) {
    TempDir dir;
    auto store = std::make_shared<MemorySettingsStore>();
    store->set_value("dialogs/last_folder", utf8(dir.path / "gone"));
    {
        Over gone(store);
        EXPECT_EQ(path_from_utf8(utf8(import_dialog_folder(*gone.window))), std::filesystem::path(INDICATOR_SAMPLES_DIR));
    }
    // a folder that exists is kept, and none stored starts in the samples too
    store->set_value("dialogs/last_folder", utf8(dir.path));
    {
        Over kept(store);
        EXPECT_EQ(path_from_utf8(utf8(import_dialog_folder(*kept.window))), dir.path);
    }
    Over fresh(std::make_shared<MemorySettingsStore>());
    EXPECT_EQ(path_from_utf8(utf8(import_dialog_folder(*fresh.window))), std::filesystem::path(INDICATOR_SAMPLES_DIR));
}

// a photo and an inspection named in German and Arabic letters: imported, analysed, saved and opened again
TEST(WxWindow, NonAsciiPhotoImportsAndSaves) {
    if (!model_available()) GTEST_SKIP();
    TempDir dir;
    auto const photo = dir.path / path_from_utf8("Kälber_جلد.jpg");
    std::filesystem::copy_file(calf, photo);
    Window window;
    ASSERT_TRUE(window->import_photo(photo)) << window->last_error();
    EXPECT_EQ(utf8(window->GetTitle()), Message("window.title_file").with("file", "Kälber_جلد.jpg").render());
    ASSERT_TRUE(wait_analysed(*window));
    ASSERT_TRUE(window->analysis()) << window->last_error();
    double const area = window->analysis()->area_mm2;
    auto const zip = dir.path / path_from_utf8("Kälber_جلد.zip");
    ASSERT_TRUE(window->export_inspection(zip)) << window->last_error();
    ASSERT_TRUE(std::filesystem::exists(zip));
    EXPECT_TRUE(contains(window->message(), "Kälber_جلد.zip")) << window->message();
    ASSERT_TRUE(window->open_inspection(zip)) << window->last_error();
    std::string const title = utf8(window->GetTitle());
    EXPECT_EQ(title, Message("window.title_file").with("file", "Kälber_جلد.zip").render());
    EXPECT_TRUE(contains(title, "Kälber_جلد.zip")) << title;
    ASSERT_TRUE(window->analysis());
    EXPECT_EQ(window->analysis()->area_mm2, area);
    EXPECT_FALSE(window->busy());
}

// the window destroyed while find_objects runs: the destructor waits for the run, its result goes nowhere
TEST(WxWindow, CloseWhileAnalysingIsClean) {
    if (!model_available()) GTEST_SKIP();
    Restore restore;
    TempDir dir;
    auto store = std::make_shared<WxConfigStore>(dir.path / "indicator.ini");
    auto* window = new MainWindow(store);
    window->set_model_candidates_for_test("", INDICATOR_DEFAULT_MODEL);
    auto delivered = std::make_shared<bool>(false);
    window->Bind(EVT_ANALYSED, [delivered](wxCommandEvent&) { *delivered = true; });
    ASSERT_TRUE(window->import_photo(calf));
    ASSERT_TRUE(window->busy());
    auto const start = std::chrono::steady_clock::now();
    window->Destroy();
    ASSERT_TRUE(pump_until([window] { return !wxTopLevelWindows.Find(window); }, 10s));
    auto const destroyed_after = std::chrono::steady_clock::now() - start;
    pump_until([] { return false; }, 500ms);  // the worker's posted result runs now, for no one
    EXPECT_FALSE(*delivered);
    // the analysis was really running: an IS-Net run takes far longer than a refused file
    EXPECT_GT(destroyed_after, 200ms);
}

}  // namespace test_Window
