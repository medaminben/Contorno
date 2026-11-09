#include "ui/CameraDialog.h"

#include "model/Language.h"
#include "model/ManualLocation.h"
#include "model/Utf8Path.h"
#include "ui/FlagIcons.h"
#include "ui/Text.h"
#include "ui/Theme.h"
#include "ui/WxConfigStore.h"

#include <wx/display.h>
#include <wx/filename.h>
#include <wx/mimetype.h>
#include <wx/sizer.h>
#include <wx/stdpaths.h>
#include <wx/tglbtn.h>
#include <wx/utils.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <initializer_list>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace {

constexpr double min_size = 0.1;
constexpr double max_size = 100;

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/// The number a text starts with, when the rest is nothing, the unit or a
/// label in parentheses; within the sensor ranges only. '.' and ',' both
/// separate the decimals.
std::optional<double> parse_size(std::string const& text) {
    size_t i = 0;
    size_t const n = text.size();
    auto digit = [&](size_t k) { return k < n && text[k] >= '0' && text[k] <= '9'; };
    auto separator = [&](size_t k) { return k < n && (text[k] == '.' || text[k] == ','); };
    auto skip = [&] {
        while (i < n && is_space(text[i]))
            ++i;
    };
    skip();
    size_t const start = i;
    if (digit(i)) {
        while (digit(i))
            ++i;
        if (separator(i)) {
            ++i;
            while (digit(i))
                ++i;
        }
    } else if (separator(i) && digit(i + 1)) {
        ++i;
        while (digit(i))
            ++i;
    } else {
        return std::nullopt;
    }
    std::string number = text.substr(start, i - start);
    std::replace(number.begin(), number.end(), ',', '.');
    skip();
    // the unit: mm, um, or the micro sign (U+00B5) or Greek mu (U+03BC) with m
    for (std::string const unit : {"mm", "um", "\xC2\xB5m", "\xCE\xBCm"}) {
        if (i + unit.size() > n)
            continue;
        std::string slice = text.substr(i, unit.size());
        for (char& c : slice) {
            if (c >= 'A' && c <= 'Z')
                c = static_cast<char>(c - 'A' + 'a');
        }
        if (slice == unit) {
            i += unit.size();
            break;
        }
    }
    skip();
    if (i < n) {
        size_t last = n;
        while (last > i && is_space(text[last - 1]))
            --last;
        if (!(text[i] == '(' && last > i + 1 && text[last - 1] == ')'))
            return std::nullopt;
    }
    double value = 0;
    auto const result = std::from_chars(number.data(), number.data() + number.size(), value);
    if (result.ec != std::errc() || result.ptr != number.data() + number.size())
        return std::nullopt;
    if (!(value >= min_size && value <= max_size))
        return std::nullopt;
    return value;
}

/// True when `text` can still become a size by typing on: digits, separators and spaces, then
/// letters (the unit), then a label in parentheses. The Qt validator's "intermediate" states.
bool partial_size(std::string const& text) {
    size_t i = 0;
    size_t const n = text.size();
    auto starts = [&](char const* s) { return text.compare(i, std::strlen(s), s) == 0; };
    while (i < n && (is_space(text[i]) || (text[i] >= '0' && text[i] <= '9') || text[i] == '.' ||
                     text[i] == ','))
        ++i;
    while (i < n) {
        char const c = text[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == ' ')
            ++i;
        else if (starts("\xC2\xB5") || starts("\xCE\xBC"))
            i += 2;
        else
            break;
    }
    while (i < n && is_space(text[i]))
        ++i;
    return i == n || text[i] == '(';
}

struct Named {
    double width, height;
    char const* label;
};

constexpr Named sensors[] = {
    {6.17, 4.55, "6.17 (1/2.3″)"},   {7.18, 5.32, "7.18 (1/1.8″)"},
    {8.80, 6.60, "8.80 (2/3″)"},     {13.20, 8.80, "13.20 (1″)"},
    {15.04, 11.28, nullptr},         {17.30, 13.00, "17.30 (4/3)"},
    {23.50, 15.60, "23.50 (APS-C)"}, {36.00, 24.00, "36.00 (full frame)"},
};
constexpr double pixels[] = {1.40, 2.00, 2.40, 3.45, 3.76, 4.50, 5.50};

std::string plain(double value, bool sensor) {
    return fixed_text(value, 2) + (sensor ? " mm" : " µm");
}

/// True when `path` is a file this user may run.
bool runnable(std::filesystem::path const& path) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error))
        return false;
#ifdef _WIN32
    return true;
#else
    return access(path.c_str(), X_OK) == 0;
#endif
}

/// The browsers of `candidates` on this machine: a path that exists, or a
/// bare name found on the PATH.
std::vector<std::string> existing_browsers(std::vector<std::string> const& candidates) {
    std::vector<std::filesystem::path> folders;
    if (char const* path = std::getenv("PATH")) {
#ifdef _WIN32
        char const separator = ';';
#else
        char const separator = ':';
#endif
        std::string const all = path;
        size_t from = 0;
        while (from <= all.size()) {
            size_t const to = std::min(all.find(separator, from), all.size());
            if (to > from)
                folders.push_back(path_from_utf8(all.substr(from, to - from)));
            from = to + 1;
        }
    }
    std::vector<std::string> found;
    for (std::string const& candidate : candidates) {
        std::filesystem::path const file = path_from_utf8(candidate);
        bool exists = false;
        if (file.is_absolute()) {
            exists = runnable(file);
        } else {
            exists = std::any_of(folders.begin(), folders.end(),
                                 [&](auto const& folder) { return runnable(folder / file); });
        }
        if (exists)
            found.push_back(candidate);
    }
    return found;
}

std::string env(char const* name) {
    wxString value;
    wxGetEnv(name, &value);
    return utf8(value);
}

/// The direction on `window` and every window below it (children do not inherit it).
void set_direction(wxWindow* window, wxLayoutDirection direction) {
    window->SetLayoutDirection(direction);
    for (wxWindow* child : window->GetChildren())
        set_direction(child, direction);
}

}  // namespace

CameraDialog::CameraDialog(wxWindow* parent, Vision::CameraSettings const& camera, wxSize photo,
                           bool rotated, std::string const& language, bool light)
    : wxDialog(parent, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      language_(language),
      photo_(photo),
      rotated_(rotated),
      sensor_w_(camera.sensor_width_mm),
      sensor_h_(camera.sensor_height_mm),
      pixel_w_(camera.pixel_width_um),
      pixel_h_(camera.pixel_height_um),
      was_sensor_(camera.input == Vision::SensorInput::sensor) {
    SetName("contorno-setup");
    distance_ = new NumberField(this, 1, 100000, 0, " mm", camera.distance_mm);
    distance_->SetName("distance");
    focal_ = new NumberField(this, 0.1, 2000, 2, " mm", camera.focal_length_mm);
    focal_->SetName("focal");
    int const box_width = GetTextExtent(wxString('0', 14)).x + FromDIP(40);
    for (wxComboBox** box : {&width_, &height_}) {
        *box = new wxComboBox(this, wxID_ANY, "", wxDefaultPosition, wxSize(box_width, -1));
    }
    width_->SetName("width");
    height_->SetName("height");
    link_ = new Key(this, wxID_ANY, "", tinted_icon("link"), 4, 6);
    link_->SetName("link");
    link_->SetCheckable(true);
    // Sensor size first and in the group: choosing one radio button clears the other
    sensor_mode_ =
        new wxRadioButton(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    pixel_mode_ = new wxRadioButton(this, wxID_ANY, "");
    (was_sensor_ ? sensor_mode_ : pixel_mode_)->SetValue(true);
    covered_ = new wxStaticText(this, wxID_ANY, "");
    covered_->SetName("covered");
    scale_ = new wxStaticText(this, wxID_ANY, "");
    scale_->SetName("scale");
    sensor_given_ = new wxStaticText(this, wxID_ANY, "");
    width_label_ = new wxStaticText(this, wxID_ANY, "");
    height_label_ = new wxStaticText(this, wxID_ANY, "");
    distance_label_ = new wxStaticText(this, wxID_ANY, "");
    focal_label_ = new wxStaticText(this, wxID_ANY, "");

    // Qt's (Breeze) spacing: 6 px between the rows, the columns and the parts, 10 px margins
    int const spacing = FromDIP(6), margin = FromDIP(10);
    auto* grid = new wxGridBagSizer(spacing, spacing);
    grid->Add(sensor_given_, wxGBPosition(0, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
    auto* modes = new wxBoxSizer(wxHORIZONTAL);
    modes->Add(sensor_mode_, 0, wxRIGHT, 16);
    modes->Add(pixel_mode_);
    grid->Add(modes, wxGBPosition(0, 1), wxGBSpan(1, 4), wxALIGN_CENTER_VERTICAL);
    grid->Add(width_label_, wxGBPosition(1, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
    grid->Add(width_, wxGBPosition(1, 1), wxDefaultSpan, wxEXPAND | wxALIGN_CENTER_VERTICAL);
    grid->Add(link_, wxGBPosition(1, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
    grid->Add(height_label_, wxGBPosition(1, 3), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
    grid->Add(height_, wxGBPosition(1, 4), wxDefaultSpan, wxEXPAND | wxALIGN_CENTER_VERTICAL);
    grid->Add(distance_label_, wxGBPosition(2, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
    grid->Add(distance_, wxGBPosition(2, 1), wxDefaultSpan, wxEXPAND | wxALIGN_CENTER_VERTICAL);
    grid->Add(focal_label_, wxGBPosition(2, 3), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
    grid->Add(focal_, wxGBPosition(2, 4), wxDefaultSpan, wxEXPAND | wxALIGN_CENTER_VERTICAL);
    grid->Add(covered_, wxGBPosition(3, 0), wxGBSpan(1, 3), wxALIGN_CENTER_VERTICAL);
    grid->Add(scale_, wxGBPosition(3, 3), wxGBSpan(1, 2), wxALIGN_CENTER_VERTICAL);
    // the labels' and the fields' columns grow alike; retranslate makes them equally wide, as Qt's
    // grid does
    for (int col : {0, 1, 3, 4})
        grid->AddGrowableCol(col);

    // the language, the theme and the help (the user manual)
    auto* looks = new wxBoxSizer(wxHORIZONTAL);
    for (std::string const& code : language_codes()) {
        Key* flag = round_key(looks, "language_" + wx(code), flag_icon(code));
        flag->SetCheckable(true);
        flag->SetChecked(code == language_);
        flags_.push_back(flag);
        flag->Bind(wxEVT_TOGGLEBUTTON, [this, code](wxCommandEvent&) { language_chosen(code); });
    }
    looks->AddStretchSpacer();
    // the theme: the moon for dark panels, pressed the sun for light ones
    theme_ = round_key(looks, "theme", tinted_icon("moon"));
    theme_->SetCheckable(true);
    theme_->SetChecked(light);
    theme_->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent&) { show_theme(theme_->IsChecked()); });
    help_ = round_key(looks, "help", tinted_icon("help"));
    help_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { open_manual(); });

    // the check mark and the "no" sign of Qt's dialog buttons, in the dark grey of the desktop's
    // icon theme (Breeze) that Qt draws them in, in either panel theme
    wxColour const icon_grey("#232629");
    ok_ = new Key(this, wxID_OK, "", tinted_icon("ok", icon_grey, 16), 6, 18);
    cancel_ = new Key(this, wxID_CANCEL, "", tinted_icon("cancel", icon_grey, 16), 6, 18);
    auto* buttons = new wxBoxSizer(wxHORIZONTAL);
    buttons->AddStretchSpacer();
    buttons->Add(ok_, 0, wxRIGHT, FromDIP(8));
    buttons->Add(cancel_);
    SetAffirmativeId(wxID_OK);
    SetEscapeId(wxID_CANCEL);

    auto* column = new wxBoxSizer(wxVERTICAL);
    column->Add(grid, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, margin);
    column->AddSpacer(spacing);
    column->Add(looks, 0, wxEXPAND | wxLEFT | wxRIGHT, margin);
    column->Add(0, spacing, 1);  // resized, the rows stay together and OK / Cancel move down
    column->Add(buttons, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, margin);
    SetSizer(column);
    for (wxWindow* field : std::initializer_list<wxWindow*> {width_, height_, distance_, focal_}) {
        field_width_ = std::max(field_width_, field->GetEffectiveMinSize().x);
    }

    show_theme(light);
    fill_lists();
    link_->SetChecked(was_sensor_ ? sensor_w_ == sensor_h_ : pixel_w_ == pixel_h_);
    show_pair();
    retranslate();
    column->Fit(this);
    SetMinSize(GetSize());

    sensor_mode_->Bind(wxEVT_RADIOBUTTON, [this](wxCommandEvent&) { mode_changed(); });
    pixel_mode_->Bind(wxEVT_RADIOBUTTON, [this](wxCommandEvent&) { mode_changed(); });
    link_->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent&) { link_toggled(); });
    for (NumberField* field : {distance_, focal_}) {
        field->Bind(EVT_NUMBER_CHANGED, [this](wxCommandEvent&) { update_results(); });
    }
    width_->Bind(wxEVT_TEXT, [this](wxCommandEvent& event) {
        if (filter_typed(width_, width_text_))
            width_edited(event.GetString());
    });
    height_->Bind(wxEVT_TEXT, [this](wxCommandEvent& event) {
        if (filter_typed(height_, height_text_))
            height_edited(event.GetString());
    });
    // Enter is OK and Escape Cancel wherever the focus is (the keys are not wxButtons, so wx has no
    // default one) (a focused key takes Enter itself before it gets here; an open list takes Enter
    // and Escape)
    for (wxComboBox* box : {width_, height_}) {
        box->Bind(wxEVT_COMBOBOX_DROPDOWN, [this](wxCommandEvent& event) {
            ++open_lists_;
            event.Skip();
        });
        box->Bind(wxEVT_COMBOBOX_CLOSEUP, [this](wxCommandEvent& event) {
            open_lists_ = std::max(0, open_lists_ - 1);
            event.Skip();
        });
    }
    // a list whose close-up never came ends when the dialog is shown or hidden; leaving the box
    // does not end it, since GTK may move the focus into the open list's popup
    Bind(wxEVT_SHOW, [this](wxShowEvent& event) {
        open_lists_ = 0;
        event.Skip();
    });
    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& event) {
        if (open_lists_ > 0)
            event.Skip();
        else if (event.GetKeyCode() == WXK_ESCAPE)
            cancel_->Click();
        else if (event.GetKeyCode() == WXK_RETURN || event.GetKeyCode() == WXK_NUMPAD_ENTER)
            ok_->Click();
        else
            event.Skip();
    });
    width_->Bind(wxEVT_COMBOBOX,
                 [this](wxCommandEvent& event) { width_chosen(event.GetSelection()); });
    // a typed text out of range or unparsable is put back when the box is left
    for (wxComboBox* box : {width_, height_}) {
        box->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& event) {
            show_pair();
            event.Skip();
        });
    }
}

Key* CameraDialog::round_key(wxSizer* row, wxString const& name, wxBitmapBundle const& icon) {
    auto* key = new Key(this, wxID_ANY, "", icon);
    key->SetName(name);
    key->SetRound(true);
    key->SetMinSize(wxSize(34, 34));
    key->SetMaxSize(wxSize(34, 34));
    row->Add(key, 0, wxRIGHT, FromDIP(6));
    return key;
}

/// Every text of the dialog in its own language (never the application's),
/// and the layout in that language's direction.
void CameraDialog::retranslate() {
    auto text = [&](char const* key) { return wx(Message(key).render(language_)); };
    set_direction(this, text_direction(language_) == TextDirection::right_to_left
                            ? wxLayout_RightToLeft
                            : wxLayout_LeftToRight);
    for (wxWindow* field :
         std::initializer_list<wxWindow*> {width_, height_, distance_->entry(), focal_->entry()}) {
        keep_text_left(field);
    }
    SetTitle(text("setup.title"));
    sensor_given_->SetLabel(text("setup.sensor_given_as"));
    sensor_mode_->SetLabel(text("setup.sensor_size"));
    pixel_mode_->SetLabel(text("setup.pixel_size"));
    width_label_->SetLabel(text("setup.width"));
    height_label_->SetLabel(text("setup.height"));
    distance_label_->SetLabel(text("setup.distance"));
    focal_label_->SetLabel(text("setup.focal"));
    link_->SetToolTip(text("setup.link_tip"));
    size_t at = 0;
    for (std::string const& code : language_codes())
        flags_[at++]->SetToolTip(wx(language_name(code)));
    theme_->SetToolTip(text("setup.theme"));
    help_->SetToolTip(text("setup.help"));
    ok_->SetLabel(text("common.ok"));
    cancel_->SetLabel(text("common.cancel"));
    // at least 64 px wide, wider for a longer text
    for (Key* key : {ok_, cancel_})
        key->SetMinSize(wxSize(std::max(FromDIP(64), key->GetBestSize().x), -1));
    // the four columns as wide as the widest of their labels and fields
    std::initializer_list<wxStaticText*> const labels {sensor_given_, width_label_, height_label_,
                                                       distance_label_, focal_label_};
    int widest = field_width_;
    for (wxStaticText* label : labels)
        widest = std::max(widest, label->GetBestSize().x);
    for (wxWindow* part : std::initializer_list<wxWindow*> {
             sensor_given_, width_label_, height_label_, distance_label_, focal_label_, width_,
             height_, distance_, focal_}) {
        part->SetMinSize(wxSize(widest, -1));
    }
    update_results();
    GetSizer()->Layout();
    wxSize grown = GetSize();
    grown.IncTo(GetBestSize());
    SetSize(grown);
    Layout();
}

/// The dialog in the theme, light or not: previewed here, applied on OK.
void CameraDialog::show_theme(bool light) {
    theme_->SetIcon(tinted_icon(light ? "sun" : "moon"));
    apply_theme(this, light);
    for (wxStaticText* label : {covered_, scale_})
        label->SetForegroundColour(wxColour(result_colour(light)));
}

/// A flag pressed: the dialog previews that language, nothing else.
void CameraDialog::language_chosen(std::string const& code) {
    size_t at = 0;
    for (std::string const& other : language_codes())
        flags_[at++]->SetChecked(other == code);  // exclusive, and one stays on
    if (code == language_)
        return;
    language_ = code;
    retranslate();
}

/// The manual in the dialog's language, in a browser's app window: Chromium
/// or Chrome (Linux), Edge (Windows), else the system's browser.
void CameraDialog::open_manual() {
    wxString const title = wx(Message("setup.help").render(language_));
    std::filesystem::path const app_dir =
        path_from_utf8(utf8(wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath()));
    std::filesystem::path const page = find_manual(
        language_, app_dir, path_from_utf8(INDICATOR_MANUAL_DIR), user_data_folder() / "manual");
    if (page.empty()) {
        warn(this, title, wx(Message("help.not_found").render(language_)));
        return;
    }
#ifdef _WIN32
    constexpr bool windows = true;
#else
    constexpr bool windows = false;
#endif
    std::vector<std::string> const browsers = existing_browsers(
        manual_browser_candidates(windows, env("ProgramFiles"), env("ProgramFiles(x86)")));
    std::filesystem::path const profile =
        path_from_utf8(utf8(wxStandardPaths::Get().GetUserDataDir())) / "manual-browser";
    Rect available;
    int const display = wxDisplay::GetFromWindow(this);
    if (display != wxNOT_FOUND) {
        wxRect const area = wxDisplay(static_cast<unsigned>(display)).GetClientArea();
        available = {area.x, area.y, area.width, area.height};
    }
    auto const command =
        manual_browser_launch(browsers, page, profile, manual_window_rect(available));
    if (!command.program.empty()) {
        std::vector<std::string> argv {command.program};
        argv.insert(argv.end(), command.arguments.begin(), command.arguments.end());
        std::vector<char const*> pointers;
        for (std::string const& argument : argv)
            pointers.push_back(argument.c_str());
        pointers.push_back(nullptr);
        if (wxExecute(pointers.data(), wxEXEC_ASYNC) > 0)
            return;
    }
    if (wxLaunchDefaultApplication(wx(utf8(page))))
        return;
    warn(this, title, wx(Message("help.no_browser").with("path", utf8(page)).render(language_)));
}

bool CameraDialog::TransferDataFromWindow() {
    show_pair();
    return true;
}

bool CameraDialog::sensor_mode() const {
    return sensor_mode_->GetValue();
}

/// The two lists for the mode: the sensor widths and heights in mm, or the
/// pixel sizes in µm.
void CameraDialog::fill_lists() {
    busy_ = true;
    bool const sensor = sensor_mode();
    width_->Clear();
    height_->Clear();
    width_items_.clear();
    height_items_.clear();
    if (sensor) {
        for (auto const& s : sensors) {
            width_->Append(wx(s.label ? std::string(s.label) : plain(s.width, true)));
            width_items_.push_back({s.width, s.height});
            height_->Append(wx(plain(s.height, true)));
            height_items_.push_back({s.height, 0});
        }
    } else {
        for (double p : pixels) {
            width_->Append(wx(plain(p, false)));
            width_items_.push_back({p, 0});
            height_->Append(wx(plain(p, false)));
            height_items_.push_back({p, 0});
        }
    }
    busy_ = false;
}

/// Shows `value` in `box`: its list entry when it has one, else its plain size.
void CameraDialog::show_value(wxComboBox* box, std::vector<Item> const& items, double value) {
    auto const at = std::find_if(items.begin(), items.end(),
                                 [&](Item const& item) { return item.value == value; });
    busy_ = true;
    if (at != items.end()) {
        int const i = static_cast<int>(at - items.begin());
        box->SetSelection(i);
        box->ChangeValue(box->GetString(static_cast<unsigned>(i)));
    } else {
        box->SetSelection(wxNOT_FOUND);
        box->ChangeValue(wx(plain(value, sensor_mode())));
    }
    (box == width_ ? width_text_ : height_text_) = utf8(box->GetValue());
    busy_ = false;
}

void CameraDialog::show_height(double h) {
    show_value(height_, height_items_, h);
    height_->Enable(!link_->IsChecked());
}

/// Shows the current mode's pair, and the link, and the results.
void CameraDialog::show_pair() {
    bool const sensor = sensor_mode();
    show_value(width_, width_items_, sensor ? sensor_w_ : pixel_w_);
    show_height(sensor ? sensor_h_ : pixel_h_);
    update_results();
}

void CameraDialog::mode_changed() {
    if (sensor_mode() == was_sensor_)
        return;
    was_sensor_ = sensor_mode();
    fill_lists();
    link_->SetChecked(sensor_mode() ? sensor_w_ == sensor_h_ : pixel_w_ == pixel_h_);
    show_pair();
}

/// A typed or pasted text of `box`: kept when it can still become a size, else the box shows the
/// last text again, so text that cannot be a size cannot be entered.
bool CameraDialog::filter_typed(wxComboBox* box, std::string& last) {
    if (busy_)
        return false;
    std::string const text = utf8(box->GetValue());
    if (partial_size(text)) {
        last = text;
        return true;
    }
    busy_ = true;
    box->ChangeValue(wx(last));
    box->SetInsertionPointEnd();
    busy_ = false;
    return false;
}

/// A width typed: the number it holds, when within the range.
void CameraDialog::width_edited(wxString const& text) {
    if (busy_)
        return;
    auto const value = parse_size(utf8(text));
    if (!value)
        return;
    set_width(*value, std::nullopt);
}

/// A width chosen from the list: a sensor's named width brings its height.
void CameraDialog::width_chosen(int index) {
    if (busy_ || index < 0 || index >= static_cast<int>(width_items_.size()))
        return;
    Item const& item = width_items_[static_cast<size_t>(index)];
    set_width(item.value,
              sensor_mode() && item.height > 0 ? std::optional<double>(item.height) : std::nullopt);
}

void CameraDialog::set_width(double value, std::optional<double> named_height) {
    bool const sensor = sensor_mode();
    double& w = sensor ? sensor_w_ : pixel_w_;
    double& h = sensor ? sensor_h_ : pixel_h_;
    w = value;
    if (named_height) {
        h = *named_height;
        if (h != w && link_->IsChecked())
            link_->SetChecked(false);
    } else if (link_->IsChecked()) {
        h = w;
    }
    // the height box follows; the width box is being typed in
    show_height(h);
    update_results();
}

void CameraDialog::height_edited(wxString const& text) {
    if (busy_)
        return;
    bool const sensor = sensor_mode();
    int const named = height_->FindString(text, false);
    std::optional<double> value;
    if (named >= 0)
        value = height_items_[static_cast<size_t>(named)].value;
    else
        value = parse_size(utf8(text));
    if (!value)
        return;
    (sensor ? sensor_h_ : pixel_h_) = *value;
    update_results();
}

void CameraDialog::link_toggled() {
    if (link_->IsChecked())
        (sensor_mode() ? sensor_h_ : pixel_h_) = sensor_mode() ? sensor_w_ : pixel_w_;
    show_pair();
}

Vision::CameraSettings CameraDialog::camera() const {
    Vision::CameraSettings c;
    c.distance_mm = distance_->value();
    c.focal_length_mm = focal_->value();
    c.input = sensor_mode() ? Vision::SensorInput::sensor : Vision::SensorInput::pixel;
    c.pixel_width_um = pixel_w_;
    c.pixel_height_um = pixel_h_;
    c.sensor_width_mm = sensor_w_;
    c.sensor_height_mm = sensor_h_;
    return c;
}

/// The covered line around the plain size `value` ("W × H mm").
std::string CameraDialog::covered_text(std::string const& value) const {
    return Message("setup.covered").with("value", value).render(language_);
}

/// The table area covered and the scale, for the mode.
void CameraDialog::update_results() {
    double const k = distance_->value() / focal_->value();
    bool const photo = photo_.x > 0 && photo_.y > 0;
    auto size_text = [](double w, double h) {
        return fixed_text(w, 0) + " × " + fixed_text(h, 0) + " mm";
    };
    std::string covered, scale;
    if (sensor_mode()) {
        // in the photo's orientation: the sensor's sides swap for a rotated photo
        double const cw = rotated_ ? sensor_h_ : sensor_w_;
        double const ch = rotated_ ? sensor_w_ : sensor_h_;
        covered = covered_text(size_text(k * cw, k * ch));
        if (photo) {
            // square pixels, as the photo will be measured
            double const p = Vision::sensor_pixel_um(sensor_w_, sensor_h_, photo_.x, photo_.y);
            Vision::PixelScale const s =
                Vision::lens_scale({distance_->value(), focal_->value(), p, p});
            scale =
                Message("setup.scale").with("value", scale_text(s) + " mm/px").render(language_);
        } else {
            scale = Message("setup.scale")
                        .with("value", Message("setup.needs_photo"))
                        .render(language_);
        }
    } else {
        Vision::LensCamera const lens {distance_->value(), focal_->value(), pixel_w_, pixel_h_};
        Vision::PixelScale const s = Vision::lens_scale(lens, rotated_);
        scale = Message("setup.scale").with("value", scale_text(s) + " mm/px").render(language_);
        covered = photo ? covered_text(size_text(s.x * photo_.x, s.y * photo_.y))
                        : Message("setup.covered")
                              .with("value", Message("setup.needs_photo"))
                              .render(language_);
    }
    covered_->SetLabel(wx(covered));
    scale_->SetLabel(wx(scale));
}
