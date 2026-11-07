// MainWindow: the file actions (import, open, save) and the dialogs that choose the files.
#include "ui/MainWindow.h"

#include "model/CameraMessage.h"
#include "model/ImageSize.h"
#include "model/Inspection.h"
#include "model/LibraryText.h"
#include "model/Utf8Path.h"
#include "ui/FileDialog.h"
#include "ui/PhotoFile.h"
#include "ui/PhotoView.h"
#include "ui/Text.h"

#include <Tools/Zip.h>

#include <wx/utils.h>

#include <random>
#include <system_error>

namespace {

/// A file filter in the current language: only the description is translated.
wxString filter_with(std::string const& description, std::string const& patterns) {
    return wx(description + " (" + patterns + ");;" + Message("dialog.all_files").render() + " (*)");
}

std::filesystem::path chosen(wxString const& path) { return path_from_utf8(utf8(path)); }

}  // namespace

void MainWindow::choose_photo() {
    auto const path = choose_open_file(this, wx(Message("dialog.import").render()), wx(settings_.last_folder),
                                       filter_with(Message("dialog.images").render(), "*.png *.jpg *.jpeg *.bmp *.tif *.tiff"));
    if (path.empty()) return;
    remember_folder(chosen(path));
    import_photo(chosen(path));
}

void MainWindow::remember_folder(std::filesystem::path const& path) {
    std::error_code ignored;
    auto const absolute = std::filesystem::absolute(path, ignored);
    settings_.last_folder = utf8((absolute.empty() ? path : absolute).parent_path());
    save_indicator_settings(*store_, settings_);
}

bool MainWindow::import_photo(std::filesystem::path const& path) {
    if (!load_photo(path)) return false;  // a failed import keeps the photo, camera and result shown
    camera_ = Vision::resolve_camera(settings_.camera, metadata_, photo_.GetWidth(), photo_.GetHeight());
    min_area_mm2_ = settings_.min_area_mm2;
    note_.reset();
    export_stem_ = path.parent_path() / path.stem();
    set_message(Message("message.imported")
                    .with("name", utf8(path.filename()))
                    .with("size", std::to_string(photo_.GetWidth()) + " x " + std::to_string(photo_.GetHeight()) + " px"));
    set_message(camera_message(camera_, shown_rotated(path)));
    analyse();
    return true;
}

void MainWindow::choose_inspection() {
    auto const path = choose_open_file(this, wx(Message("dialog.open").render()), wx(settings_.last_folder),
                                       filter_with(Message("dialog.inspections").render(), "*.zip"));
    if (path.empty()) return;
    remember_folder(chosen(path));
    open_inspection(chosen(path));
}

std::filesystem::path MainWindow::new_inspection_dir(std::string& reason) {
    std::error_code error;
    auto const root = std::filesystem::temp_directory_path(error);
    if (error) {
        reason = error.message();
        return {};
    }
    static unsigned counter = 0;
    std::random_device random;
    for (int attempt = 0; attempt < 100; ++attempt) {
        // the process id, a counter and random bits: no other window or process makes this folder
        auto const dir = root / ("indicator-" + std::to_string(wxGetProcessId()) + "-" + std::to_string(counter++) +
                                 "-" + std::to_string(random()));
        if (std::filesystem::create_directory(dir, error)) return dir;
        if (error) {
            reason = error.message();
            return {};
        }
    }
    reason = std::make_error_code(std::errc::file_exists).message();
    return {};
}

bool MainWindow::open_inspection(std::filesystem::path const& path) {
    std::string reason;
    auto const dir = new_inspection_dir(reason);
    if (dir.empty()) {
        fail(Message("message.temp_folder").with("reason", reason), true);
        return false;
    }
    inspection_dirs_.push_back(dir);
    auto files = unpack_inspection(path, dir);
    if (files.isError()) {
        fail(files.error(), true);
        return false;
    }
    auto const& f = files();
    if (f.result) {
        // checked first: a refused inspection keeps the photo, camera and result shown
        auto const size = shown_size(f.photo_path);
        if (!size || size->width != f.result->frame.width || size->height != f.result->frame.height) {
            fail(Message("message.size_differs_saved").with("name", utf8(path.filename())), true);
            return false;
        }
    }
    if (!load_photo(f.photo_path)) return false;
    export_stem_ = path.parent_path() / path.stem();
    set_title(utf8(path.filename()));  // not the zip's "original.jpg"
    set_message(Message("message.opened").with("path", utf8(path)));
    camera_ = f.camera;  // until the next import
    if (f.legacy) {
        // its parameters and result are ignored: analysed again with the AI
        note_ = Message("message.older_inspection");
        set_message(*note_);
        analyse();
        return true;
    }
    // the saved result, shown without an analysis
    note_.reset();
    min_area_mm2_ = f.min_area_mm2;  // until the next import
    show_saved_result(*f.result, f.min_area_mm2);
    return true;
}

void MainWindow::choose_export() {
    if (!analysis_) {
        fail(Message(photo_path_.empty() ? "message.import_first" : "message.no_result"), true);
        return;
    }
    // in the last folder, named after the photo
    auto const suggested = path_from_utf8(settings_.last_folder) / (export_stem_.filename().u8string() + u8".zip");
    auto const path = choose_save_file(this, wx(Message("dialog.save").render()), wx(utf8(suggested)),
                                       filter_with(Message("dialog.inspections").render(), "*.zip"), "zip");
    if (path.empty()) return;
    remember_folder(chosen(path));
    export_inspection(chosen(path));
}

bool MainWindow::export_inspection(std::filesystem::path const& path) {
    if (busy()) {
        fail(Message("message.wait"), true);
        return false;
    }
    if (!analysis_) {
        fail(Message("message.nothing_to_save"), true);
        return false;
    }
    InspectionFiles files;
    files.photo_path = photo_path_;
    files.camera = camera_;
    files.min_area_mm2 = analysed_min_area_mm2_;
    auto entries = inspection_entries(files, *analysis_);
    if (entries.isError()) {
        fail(Message("message.cannot_save").with("path", utf8(path)).with("reason", entries.error()), true);
        return false;
    }
    if (auto written = Tools::write_zip(utf8(path), entries()); written.isError()) {
        fail(Message("message.cannot_save").with("path", utf8(path)).with("reason", from_library(written.error())), true);
        return false;
    }
    error_ = {};
    set_message(Message("message.saved").with("path", utf8(path)));
    return true;
}

bool MainWindow::load_photo(std::filesystem::path const& path) {
    PhotoFile const file = read_photo(path);
    if (!file.image.IsOk()) {
        fail(Message("message.cannot_read").with("path", utf8(path)).with("reason", file.error), true);
        return false;
    }
    photo_path_ = path;
    photo_ = file.image;
    metadata_ = file.metadata;  // to resolve the camera again
    expected_id_ = 0;  // a result still coming for the previous photo is dropped
    analysed_min_area_mm2_ = 0;
    set_running(false);  // an analysis still running is obsolete
    Vision::PixelFrame const frame {photo_.GetWidth(), photo_.GetHeight(), 0};
    raw_->set_photo(photo_, frame);
    processed_->set_photo(photo_, frame);
    show_no_result(false, true);  // the readout counts from the area shown to the new one
    set_title(utf8(path.filename()));
    return true;
}
