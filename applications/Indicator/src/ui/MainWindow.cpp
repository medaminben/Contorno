#include "ui/MainWindow.h"

#include "model/CameraMessage.h"
#include "model/ImageSize.h"
#include "model/Measure.h"
#include "model/ModelPath.h"
#include "model/Utf8Path.h"
#include "ui/AppIcon.h"
#include "ui/CameraDialog.h"
#include "ui/Localise.h"
#include "ui/NumberField.h"
#include "ui/PhotoView.h"
#include "ui/MessageLog.h"
#include "ui/Text.h"
#include "ui/Theme.h"
#include "ui/WxConfigStore.h"

#include <wx/app.h>
#include <wx/display.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stdpaths.h>
#include <wx/utils.h>

#include <algorithm>
#include <system_error>
#include <utility>

wxDEFINE_EVENT(EVT_ANALYSED, wxCommandEvent);

namespace {

wxColour const contour_colour(0x28, 0xDC, 0x28);
int const box_gap = 8;  ///< before, between and after the photo boxes
int const margin = 9;   ///< around the photos and the panel
int const spacing = 6;  ///< between the photos and the panel

/// The photo boxes in one row: squares of equal size, as wide as their share
/// of the row (less the gaps), smaller when the row is too low. The gaps
/// before, between and after them are equal (at least `gap`). They sit at the
/// row's top: the height left over stays empty, between them and the control
/// panel.
class SquareRow : public wxPanel {
public:
    SquareRow(wxWindow* parent, int gap) : gap_(gap) {
        wxPanel::Create(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
        Bind(wxEVT_SIZE, [this](wxSizeEvent&) { place(); });
    }
    /// Takes `box` (a child of this row) as the next square.
    void add(wxWindow* box) {
        boxes_.push_back(box);
        SetMinSize(row_size(120));
    }
    /// The squares' side when the row is `width` wide (and high enough).
    int side_for(int width) const {
        int const n = static_cast<int>(boxes_.size());
        return n == 0 ? 0 : (width - (n + 1) * gap_) / n;
    }

protected:
    wxSize DoGetBestSize() const override { return row_size(460); }

private:
    wxSize row_size(int side) const {
        int const n = static_cast<int>(boxes_.size());
        return {n * side + (n + 1) * gap_, side};
    }
    void place() {
        wxSize const size = GetClientSize();
        int const side = std::max(0, std::min(side_for(size.x), size.y));
        int const n = static_cast<int>(boxes_.size());
        int const spare = size.x - n * side;
        for (int i = 0; i < n; ++i) {
            // the i+1-th of n+1 equal gaps, rounded where it ends
            int const x = i * side + spare * (i + 1) / (n + 1);
            boxes_[static_cast<std::size_t>(i)]->SetSize(x, 0, side, side);
        }
    }

    std::vector<wxWindow*> boxes_;
    int gap_;
};

/// `window` stays left to right when apply_language turns the windows for Arabic.
void keep_left_to_right(wxWindow* window) {
    window->SetLayoutDirection(wxLayout_LeftToRight);
    window->Bind(EVT_LANGUAGE_CHANGED, [window](wxCommandEvent& event) {
        window->SetLayoutDirection(wxLayout_LeftToRight);
        event.Skip();
    });
}

/// The mask as the panel shows it: the object black, the rest transparent
/// (`width` x `height`; all transparent for an empty mask or one of another size).
wxImage mask_shape(Vision::Mask const& mask, int width, int height) {
    wxImage image(width, height, true);  // black
    image.SetAlpha();
    unsigned char* alpha = image.GetAlpha();
    std::fill(alpha, alpha + static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0);
    if (mask.width != width || mask.height != height ||
        mask.data.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
        return image;
    }
    for (std::size_t i = 0; i < mask.data.size(); ++i) {
        if (mask.data[i] != 0)
            alpha[i] = 255;
    }
    return image;
}

/// Whether the Set Up dialog gave back the camera it was given.
bool same_camera(Vision::CameraSettings const& a, Vision::CameraSettings const& b) {
    return a.distance_mm == b.distance_mm && a.focal_length_mm == b.focal_length_mm &&
           a.input == b.input && a.pixel_width_um == b.pixel_width_um &&
           a.pixel_height_um == b.pixel_height_um && a.sensor_width_mm == b.sensor_width_mm &&
           a.sensor_height_mm == b.sensor_height_mm;
}

/// "1 object", "3 objects", "No object found".
Message count_message(Vision::ObjectsAnalysis const& a) {
    return a.objects.empty() ? Message("message.objects.none")
           : a.objects.size() == 1
               ? Message("message.objects.one")
               : Message("message.objects.many").with("count", std::to_string(a.objects.size()));
}

/// "0.500 mm/px" or "0.500 × 0.600 mm/px".
std::string scale_with_unit(Vision::PixelFrame const& frame) {
    return scale_text({frame.mm_per_px_x, frame.mm_per_px_y}) + " mm/px";
}

/// Where the file dialogs start before a folder is remembered: the samples in
/// the user's data folder, else those of the sources when they are there,
/// else the home folder.
std::string first_folder() {
    std::error_code ignored;
    auto const samples = user_data_folder() / "samples";
    if (std::filesystem::is_directory(samples, ignored))
        return utf8(samples);
#ifdef INDICATOR_SAMPLES_DIR
    if (std::filesystem::is_directory(path_from_utf8(INDICATOR_SAMPLES_DIR), ignored))
        return INDICATOR_SAMPLES_DIR;
#endif
    return utf8(wxGetHomeDir());
}

}  // namespace

MainWindow::MainWindow(std::shared_ptr<SettingsStore> store, long style)
    : wxFrame(nullptr, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, style),
      store_(std::move(store)),
      model_default_(path_from_utf8(INDICATOR_DEFAULT_MODEL)) {
    if (!store_)
        store_ = std::make_shared<WxConfigStore>(default_settings_file());
    set_title({});
    settings_ = load_indicator_settings(*store_);
    // the model keeps the text stored; a folder that is gone starts the dialogs in the first one
    std::error_code ignored;
    if (settings_.last_folder.empty() ||
        !std::filesystem::is_directory(path_from_utf8(settings_.last_folder), ignored)) {
        settings_.last_folder = first_folder();
    }
    camera_ = Vision::resolve_camera(settings_.camera, {}, 0, 0);
    model_configured_ = path_from_utf8(settings_.model_path);
    min_area_mm2_ = settings_.min_area_mm2;
    follow_system_scheme(this);

    body_ = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    auto* row = new SquareRow(body_, box_gap);
    row_ = row;
    raw_ = new PhotoView(row);
    processed_ = new PhotoView(row);
    row->add(raw_);
    row->add(processed_);
    raw_->link_to(processed_);  // the panel's small mask keeps its own fit
    panel_ = new ControlPanel(body_);
    mask_ = &panel_->mask();
    log_ = &panel_->log();
    status_ = new wxPanel(body_, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    progress_ = new ProgressBar(status_);
    auto* strip = new wxBoxSizer(wxHORIZONTAL);
    strip->Add(progress_, 1, wxEXPAND | wxALL, 3);  // the whole strip
    status_->SetSizer(strip);

    auto* column = new wxBoxSizer(wxVERTICAL);
    column->Add(row, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, margin);
    column->AddSpacer(spacing);
    column->Add(panel_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, margin);
    column->Add(status_, 0, wxEXPAND);
    body_->SetSizer(column);
    for (wxWindow* window :
         {static_cast<wxWindow*>(this), body_, row_, static_cast<wxWindow*>(raw_),
          static_cast<wxWindow*>(processed_), status_, static_cast<wxWindow*>(progress_)}) {
        keep_left_to_right(window);  // the window never mirrors, whatever the language
    }
    apply_theme(settings_.light);
    set_message(Message("message.import_first_photo"));

    Bind(EVT_LANGUAGE_CHANGED, [this](wxCommandEvent& event) {
        set_title(title_file_);
        event.Skip();
    });
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { choose_photo(); }, ID_IMPORT);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { choose_inspection(); }, ID_OPEN);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { choose_export(); }, ID_SAVE);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { open_camera(); }, ID_SETUP);
    analyser_ = std::make_unique<Analyser>([](auto task) { wxTheApp->CallAfter(std::move(task)); },
                                           [this](AnalysisDone const& done) { show_result(done); });

    // A restored window: the squares fill the width, the height follows (both within the screen).
    // A window created with wxMAXIMIZE keeps that state. SetClientSize calls gtk_window_resize,
    // which clears the maximized flag on GTK before the window is mapped.
    SetMinClientSize(column->CalcMin());
    if ((GetWindowStyleFlag() & wxMAXIMIZE) == 0) {
        int const shown_on = wxDisplay::GetFromWindow(this);
        wxRect const area =
            wxDisplay(shown_on == wxNOT_FOUND ? 0u : static_cast<unsigned>(shown_on))
                .GetClientArea();
        wxSize const room(area.width * 9 / 10, area.height * 9 / 10);
        int const below = spacing + panel_->GetBestSize().y + margin + status_->GetBestSize().y;
        auto height_for = [&](int w) {
            return margin + std::max(0, row->side_for(w - 2 * margin)) + below;
        };
        int width = std::min(1500, room.x);
        while (width > 600 && height_for(width) > room.y)
            width -= 20;
        SetClientSize(width, height_for(width));
    }
}

MainWindow::~MainWindow() {
    analyser_.reset();  // waits for a run in progress, which may read an opened inspection's photo
    for (auto const& dir : inspection_dirs_) {
        std::error_code ignored;
        std::filesystem::remove_all(dir, ignored);
    }
}

void MainWindow::set_model_candidates_for_test(std::filesystem::path configured,
                                               std::filesystem::path compiled_default) {
    model_configured_ = std::move(configured);
    model_default_ = std::move(compiled_default);
}

std::string MainWindow::message() const {
    return utf8(log_->last_message());
}

std::string MainWindow::message_key() const {
    return log_->last_text().key();
}

void MainWindow::set_title(std::string const& file) {
    title_file_ = file;
    SetTitle(wx(file.empty() ? Message("window.title").render()
                             : Message("window.title_file").with("file", file).render()));
}

void MainWindow::set_message(Message const& message) {
    log_->add(message);
}

void MainWindow::fail(Message const& message, bool dialog) {
    error_ = message;
    set_message(message);
    if (dialog && IsShown())
        warn(this, wx(Message("window.title").render()), wx(message.render()));
}

void MainWindow::emit_analysed(bool ok) {
    wxCommandEvent event(EVT_ANALYSED, GetId());
    event.SetInt(ok ? 1 : 0);
    event.SetEventObject(this);
    ProcessWindowEvent(event);
}

void MainWindow::show_no_result(bool keep_view, bool keep_readout) {
    analysis_.reset();
    Vision::PixelFrame const frame {photo_.GetWidth(), photo_.GetHeight(), 0};
    processed_->set_photo(photo_, frame, keep_view);
    mask_->set_photo(mask_shape({}, photo_.GetWidth(), photo_.GetHeight()), frame, keep_view);
    if (keep_readout) {
        panel_->display().set_dimmed(true);  // stale until the result; dashes stay dashes
    } else {
        panel_->display().set_dashes();
    }
}

void MainWindow::open_camera() {
    wxSize const photo = photo_.IsOk() ? photo_.GetSize() : wxSize();
    CameraDialog dialog(this, settings_.camera, photo,
                        !photo_path_.empty() && shown_rotated(photo_path_), settings_.language,
                        settings_.light);
    if (dialog.ShowModal() != wxID_OK)
        return;
    bool const new_camera = !same_camera(dialog.camera(), settings_.camera);
    bool const new_language = dialog.language() != settings_.language;
    bool const new_theme = dialog.light() != settings_.light;
    if (!new_camera && !new_language && !new_theme)
        return;  // OK without a change: nothing happens
    settings_.camera = dialog.camera();
    settings_.language = dialog.language();
    settings_.light = dialog.light();
    save_indicator_settings(*store_, settings_);
    if (new_theme)
        apply_theme(settings_.light);
    if (new_language)
        apply_language(settings_.language);  // before the log line, which is then in it
    if (!new_camera)
        return;  // the language or the theme: the result stands
    camera_ = Vision::resolve_camera(
        settings_.camera, photo_path_.empty() ? Vision::PhotoMetadata {} : metadata_,
        photo_.IsOk() ? photo_.GetWidth() : 0, photo_.IsOk() ? photo_.GetHeight() : 0);
    set_message(photo_path_.empty() ? Message("message.camera_saved")
                                    : camera_message(camera_, shown_rotated(photo_path_)));
    analyse();
}

void MainWindow::analyse() {
    if (photo_path_.empty())
        return;
    // Checked before every request: no fallback to the colour method.
    std::optional<Message> unavailable;
    std::filesystem::path model;
    if (!Vision::ai_available()) {
        unavailable = Message("message.ai.no_libtorch");
    } else {
        std::vector<std::filesystem::path> looked_in;
        auto const app_dir =
            path_from_utf8(utf8(wxStandardPaths::Get().GetExecutablePath())).parent_path();
        model = find_model(model_configured_, app_dir, model_default_, &looked_in);
        if (model.empty()) {
            std::string places;
            for (auto const& place : looked_in)
                places += (places.empty() ? "" : ", ") + utf8(place);
            unavailable = Message("message.ai.no_model").with("places", places);
        }
    }
    if (unavailable) {
        expected_id_ = 0;  // a result still coming is no longer the one shown
        set_running(false);
        show_no_result(true);
        note_.reset();
        fail(*unavailable, false);
        emit_analysed(false);
        return;
    }
    requested_min_area_mm2_ = min_area_mm2_;
    expected_id_ = analyser_->request(photo_path_, camera_.lens, requested_min_area_mm2_, model);
    set_running(true);
    panel_->display().set_dimmed(true);  // the shown value is stale until the result
    if (!note_)
        set_message(Message("message.analysing"));
}

void MainWindow::show_result(AnalysisDone const& done) {
    if (done.id != expected_id_) {
        // stale: another photo or inspection was opened meanwhile
        set_running(busy());
        return;
    }
    expected_id_ = 0;  // shown next: nothing more is coming
    set_running(false);
    auto failed = [&](Message const& message) {
        show_no_result(true);
        note_.reset();
        fail(message, false);
        emit_analysed(false);
    };
    if (!done.analysis)
        return failed(done.error);
    auto const& a = *done.analysis;
    if (a.frame.width != photo_.GetWidth() || a.frame.height != photo_.GetHeight()) {
        return failed(Message("message.size_differs_analysed"));
    }
    analysed_min_area_mm2_ = requested_min_area_mm2_;
    error_ = {};
    show_analysis(a);
    Message message = Message(note_ ? "message.result_note" : "message.result")
                          .with("count", count_message(a))
                          .with("scale", scale_with_unit(a.frame))
                          .with("seconds", fixed_text(done.seconds, 1) + " s");
    if (note_)
        message.with("note", *note_);
    note_.reset();  // said once, with the first result
    set_message(message);
    if (a.from_background)
        set_message(Message("message.from_background"));
    emit_analysed(true);
}

void MainWindow::show_saved_result(Vision::ObjectsAnalysis const& analysis, double min_area_mm2) {
    analysed_min_area_mm2_ = min_area_mm2;
    error_ = {};
    show_analysis(analysis);
    set_message(Message("message.saved_result")
                    .with("count", count_message(analysis))
                    .with("scale", scale_with_unit(analysis.frame)));
}

void MainWindow::show_analysis(Vision::ObjectsAnalysis const& analysis) {
    analysis_ = analysis;
    auto const& a = *analysis_;
    wxPen const pen(contour_colour, 2);            // as many device pixels wide at every zoom
    processed_->set_photo(photo_, a.frame, true);  // keeps the linked zoom
    processed_->add_polygons(a.objects, pen, *wxTRANSPARENT_BRUSH);
    mask_->set_photo(mask_shape(a.mask, a.frame.width, a.frame.height), a.frame, true);
    mask_->add_polygons(a.objects, pen, *wxTRANSPARENT_BRUSH);
    auto const area = format_area(a.area_mm2);
    panel_->display().set_value(wx(area.digits), wx(area.unit));
}

void MainWindow::set_running(bool running) {
    // busy indicator while analysing; full once the result is shown, for a second
    progress_->set_running(running);
}

void MainWindow::apply_theme(bool light) {
    // behind the photos and the panel, and under the busy indicator; the
    // photo boxes, keys, log and readout keep their own colours
    wxColour const colour = palette(light).panel;  // #E4E7E3 or black
    for (wxWindow* window : {static_cast<wxWindow*>(this), body_, row_, status_}) {
        window->SetBackgroundColour(colour);
        window->Refresh();
    }
    panel_->SetBackgroundColour(colour);
    panel_->Refresh();
    progress_->Refresh();    // its corners
    set_light_theme(light);  // the dialogs opened from now on
    apply_title_bar(this, light);
}
