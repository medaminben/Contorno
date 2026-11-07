#ifndef INDICATOR_MAINWINDOW_H
#define INDICATOR_MAINWINDOW_H
#include "model/Analyser.h"
#include "model/IndicatorSettings.h"
#include "model/Message.h"
#include "ui/ControlPanel.h"
#include "ui/ProgressBar.h"

#include <Vision/Objects.h>
#include <Vision/Photo.h>

#include <wx/frame.h>
#include <wx/image.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

/// Sent to the window when the shown result changed: an analysis ended
/// (`GetInt()` 1) or failed (0).
wxDECLARE_EVENT(EVT_ANALYSED, wxCommandEvent);

/// Two square boxes side by side, each half of the window's width, zooming
/// together, no captions: the original photo and the photo with the
/// object's outline. Under them the control panel, with the mask and the
/// message log; the strip at the bottom is the busy indicator. A change of the
/// camera analyses the photo again; the minimum area is the settings'. The
/// window never mirrors, whatever the language.
class MainWindow : public wxFrame {
public:
    /// The settings are read from and written to `store`; none: the user's
    /// file, WxConfigStore(default_settings_file()). `style` is the frame's
    /// (wxMAXIMIZE: the app opens on the whole screen, and the constructor
    /// does not pin a client size that would leave it restored).
    explicit MainWindow(std::shared_ptr<SettingsStore> store = nullptr, long style = wxDEFAULT_FRAME_STYLE);
    ~MainWindow() override;

    /// Shows the photo and starts its analysis with the remembered camera.
    bool import_photo(std::filesystem::path const& path);
    /// Opens an inspection zip: its photo and camera (which apply until the
    /// next import), its minimum area and its saved result, without an
    /// analysis. An older hide inspection is analysed again with the AI.
    bool open_inspection(std::filesystem::path const& path);
    /// Writes the inspection zip of the result shown; refused while busy().
    bool export_inspection(std::filesystem::path const& path);
    ControlPanel& panel() { return *panel_; }
    /// An analysis of what is shown runs: its result is still to come. One
    /// meaning for the busy indicator, export and the self-test; an analysis
    /// made obsolete by opening another photo or inspection is not counted,
    /// though the analyser may still be finishing it.
    bool busy() const { return expected_id_ != 0; }
    /// The analysis shown, and the minimum area it was made with.
    std::optional<Vision::ObjectsAnalysis> const& analysis() const { return analysis_; }
    double analysed_min_area_mm2() const { return analysed_min_area_mm2_; }
    /// The camera the photo is analysed with.
    Vision::PhotoCamera const& camera() const { return camera_; }
    /// The last message of the log in the panel (UTF-8).
    std::string message() const;
    /// The key of the last message (what a test or the self-test checks, whatever the language).
    std::string message_key() const;
    /// The last failure as shown now ("" when none).
    std::string last_error() const { return error_.render(); }

    /// Tests: the configured and compiled-in model paths tried instead of
    /// the settings' and INDICATOR_DEFAULT_MODEL.
    void set_model_candidates_for_test(std::filesystem::path configured, std::filesystem::path compiled_default);

private:
    void choose_photo();
    void choose_inspection();
    void choose_export();
    /// The folder of `path` (a file chosen in a dialog) is where the next dialog starts.
    void remember_folder(std::filesystem::path const& path);
    bool load_photo(std::filesystem::path const& path);
    void open_camera();
    /// Requests an analysis, or says why the AI outline is unavailable.
    void analyse();
    void show_result(AnalysisDone const& done);
    /// An opened inspection's result, made with `min_area_mm2`, shown and logged.
    void show_saved_result(Vision::ObjectsAnalysis const& analysis, double min_area_mm2);
    /// The analysis in the middle and right boxes (outlines on the mask and
    /// on the photo) and its area in the readout.
    void show_analysis(Vision::ObjectsAnalysis const& analysis);
    /// No result: the mask box empty (transparent, photo size), the photo
    /// without outlines on the right; the readout dashes, or with
    /// `keep_readout` (a new photo, its area still to come) the value shown
    /// dimmed, so that the result counts from it.
    void show_no_result(bool keep_view, bool keep_readout = false);
    /// Busy while analysing; stopped, the bar is full for a second, then its
    /// green dims to black.
    void set_running(bool running);
    /// The panels (the window behind the photos, the control panel and the
    /// busy indicator's strip) light grey with a black wordmark, or black with a white one.
    void apply_theme(bool light);
    /// The window title with the shown file's name (none: the title alone), in the current language.
    void set_title(std::string const& file);
    void set_message(Message const& message);
    /// Logs `message` as the failure; with `dialog` (and a visible window) a
    /// warning shows it as rendered at that moment.
    void fail(Message const& message, bool dialog);
    void emit_analysed(bool ok);
    /// A new folder for an opened inspection's files, kept for the session; empty when it cannot be made.
    std::filesystem::path new_inspection_dir(std::string& reason);

    std::shared_ptr<SettingsStore> store_;
    IndicatorSettings settings_;
    /// The camera the photo is analysed with: the photo's metadata and the
    /// settings', or that of an opened inspection until the next import.
    Vision::PhotoCamera camera_ {};
    Vision::PhotoMetadata metadata_ {};  ///< of the photo shown, to resolve the camera again
    std::filesystem::path model_configured_;
    std::filesystem::path model_default_;
    std::filesystem::path photo_path_;
    std::string title_file_;  ///< the file named in the title ("" before any)
    std::filesystem::path export_stem_;  ///< suggested export path without ".zip"
    std::optional<Message> note_;  ///< ends the next result's message (e.g. an older inspection)
    /// Folders of the opened inspections, kept for the session (a running
    /// analysis may still read an earlier one) and removed with the window.
    std::vector<std::filesystem::path> inspection_dirs_;
    wxImage photo_;
    wxWindow* body_;  ///< everything in the frame
    wxWindow* row_;   ///< the photo boxes, side by side
    PhotoView* raw_;
    PhotoView* processed_;  ///< the photo, the object's outline green
    ControlPanel* panel_;
    PhotoView* mask_;  ///< the panel's: the object black, the rest transparent, its outline green
    MessageLog* log_;  ///< the panel's
    wxWindow* status_;       ///< the strip under the panels
    ProgressBar* progress_;
    std::uint64_t expected_id_ {0};  ///< the request whose result is shown next (0: none coming)
    /// The minimum area of the analyses: the settings', or that of an opened
    /// inspection until the next import.
    double min_area_mm2_ {0};
    double requested_min_area_mm2_ {0};
    double analysed_min_area_mm2_ {0};
    std::optional<Vision::ObjectsAnalysis> analysis_;
    Message error_;  ///< the last failure; no key when none
    /// Last: destroyed first, it waits for a run in progress before anything else goes.
    std::unique_ptr<Analyser> analyser_;
};

#endif  // INDICATOR_MAINWINDOW_H
