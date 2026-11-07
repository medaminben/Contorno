#ifndef INDICATOR_CAMERADIALOG_H
#define INDICATOR_CAMERADIALOG_H
#include "model/Measure.h"
#include "model/Message.h"
#include "ui/Key.h"
#include "ui/NumberField.h"

#include <wx/combobox.h>
#include <wx/dialog.h>
#include <wx/gbsizer.h>
#include <wx/radiobut.h>
#include <wx/stattext.h>

#include <optional>
#include <string>
#include <vector>

/// The Set Up dialog: the sensor as its size or its pixel size (width and
/// height, linked or not), the distance to the table and the focal length,
/// with the table area and the scale they give. `photo` is the shown photo's
/// size (empty without one) and whether it is shown rotated by 90 degrees, which
/// swaps the pixel width and height: the results need them. The dialog writes
/// itself in its own language `language`; a flag previews another one here
/// only (nothing outside the dialog changes), and `language()` is the pressed
/// flag's code. The Theme key is on for light panels (`light()`); like the
/// language, it is previewed in the dialog and changes nothing else until OK.
/// The help key opens the user manual in the dialog's language.
/// Its children are found by name: width, height, link, distance, focal,
/// covered, scale, language_<code>, theme, help.
class CameraDialog : public wxDialog {
public:
    CameraDialog(wxWindow* parent, Vision::CameraSettings const& camera, wxSize photo, bool rotated,
                 std::string const& language, bool light = false);

    Vision::CameraSettings camera() const;
    std::string const& language() const { return language_; }
    bool light() const { return theme_->IsChecked(); }
    /// OK: puts a half-typed box right before the dialog closes.
    bool TransferDataFromWindow() override;

private:
    /// A list entry: its value and, for a named sensor width, the height that goes with it.
    struct Item {
        double value;
        double height;
    };
    bool sensor_mode() const;
    void fill_lists();
    void show_pair();
    void mode_changed();
    bool filter_typed(wxComboBox* box, std::string& last);
    void width_edited(wxString const& text);
    void height_edited(wxString const& text);
    void width_chosen(int index);
    void set_width(double value, std::optional<double> named_height);
    void link_toggled();
    void update_results();
    /// All texts in `language_`, and the layout in its direction.
    void retranslate();
    std::string covered_text(std::string const& value) const;
    void language_chosen(std::string const& code);
    void show_theme(bool light);
    void open_manual();
    Key* round_key(wxSizer* row, wxString const& name, wxBitmapBundle const& icon);
    void show_value(wxComboBox* box, std::vector<Item> const& items, double value);
    /// The height box shows `h` and is enabled when the link is off.
    void show_height(double h);

    std::string language_;
    wxSize photo_;
    bool rotated_;
    NumberField* distance_;
    NumberField* focal_;
    wxComboBox* width_;
    wxComboBox* height_;
    Key* link_;
    wxRadioButton* pixel_mode_;
    wxRadioButton* sensor_mode_;
    wxStaticText* covered_;
    wxStaticText* scale_;
    wxStaticText* sensor_given_;
    wxStaticText* width_label_;
    wxStaticText* height_label_;
    wxStaticText* distance_label_;
    wxStaticText* focal_label_;
    Key* theme_;
    Key* help_;
    std::vector<Key*> flags_;
    Key* ok_;
    Key* cancel_;
    std::string width_text_, height_text_;  ///< what each box last showed validly
    std::vector<Item> width_items_, height_items_;
    /// Each mode's pair, width and height, while the dialog is open.
    double sensor_w_, sensor_h_, pixel_w_, pixel_h_;
    bool was_sensor_;
    bool busy_ {false};
    int open_lists_ {0};   ///< drop-down lists that are open
    int field_width_ {0};  ///< the widest field's own least width (px)
};

#endif  // INDICATOR_CAMERADIALOG_H
