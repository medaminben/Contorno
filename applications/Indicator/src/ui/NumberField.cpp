#include "ui/NumberField.h"

#include "ui/Text.h"

#include <wx/sizer.h>

#ifdef INDICATOR_GTK3
#include <gtk/gtk.h>
#endif

#include <algorithm>
#include <charconv>
#include <cstdlib>

wxDEFINE_EVENT(EVT_NUMBER_CHANGED, wxCommandEvent);

std::string fixed_text(double value, int decimals) {
    char buffer[64];
    auto const result = std::to_chars(buffer, buffer + sizeof buffer, value, std::chars_format::fixed, decimals);
    return std::string(buffer, result.ptr);
}

void keep_text_left(wxWindow* field) {
#ifdef INDICATOR_GTK3
    GtkWidget* entry = field->GetHandle();
    if (entry && !GTK_IS_ENTRY(entry) && GTK_IS_BIN(entry)) entry = gtk_bin_get_child(GTK_BIN(entry));  // a combo box's
    if (!entry || !GTK_IS_ENTRY(entry)) return;
    // GTK reads the alignment from the leading edge: right to left, 1 is the left
    gtk_entry_set_alignment(GTK_ENTRY(entry), field->GetLayoutDirection() == wxLayout_RightToLeft ? 1.0f : 0.0f);
#else
    (void)field;
#endif
}

namespace {
/// The number a text holds, with or without the unit.
bool parse_number(wxString const& text, wxString const& suffix, double& out) {
    std::string s = utf8(text);
    auto const first = s.find_first_not_of(" \t");
    if (first == std::string::npos) return false;
    s.erase(0, first);
    s.erase(s.find_last_not_of(" \t") + 1);
    std::string const unit = utf8(suffix);
    std::string const bare = unit.empty() ? unit : unit.substr(unit.find_first_not_of(' '));
    if (!bare.empty() && s.size() >= bare.size() && s.compare(s.size() - bare.size(), bare.size(), bare) == 0) {
        s.erase(s.size() - bare.size());
        s.erase(s.find_last_not_of(" \t") + 1);
    }
    if (s.empty()) return false;
    // English group separators, as the Qt spin box accepts them: "1,500" and "12,345.6", not "1,5"
    if (s.find(',') != std::string::npos) {
        size_t const end = std::min(s.find('.'), s.size());
        size_t const first = s.find(',');
        bool grouped = first >= 1 && first <= 3 && (end - first) % 4 == 0;
        for (size_t i = 0; i < end && grouped; ++i) {
            bool const comma = i >= first && (i - first) % 4 == 0;
            grouped = comma ? s[i] == ',' : (s[i] >= '0' && s[i] <= '9');
        }
        if (!grouped) return false;
        s.erase(std::remove(s.begin(), s.end(), ','), s.end());
    }
    auto const result = std::from_chars(s.data(), s.data() + s.size(), out);
    return result.ec == std::errc() && result.ptr == s.data() + s.size();
}
}  // namespace

NumberField::NumberField(wxWindow* parent, double min, double max, int decimals, wxString const& suffix, double value)
    : min_(min), max_(max), decimals_(decimals), suffix_(suffix) {
    wxPanel::Create(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    entry_ = new wxTextCtrl(this, wxID_ANY);
    arrows_ = new wxSpinButton(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_VERTICAL);
    arrows_->SetRange(-1000000, 1000000);
    arrows_->SetValue(0);
    auto* row = new wxBoxSizer(wxHORIZONTAL);
    row->Add(entry_, 1, wxALIGN_CENTER_VERTICAL);
    row->Add(arrows_, 0, wxEXPAND);
    SetSizer(row);
    value_ = std::clamp(value, min_, max_);
    show();

    entry_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) {
        if (busy_) return;
        double typed = 0;
        if (!parse_number(entry_->GetValue(), suffix_, typed) || !(typed >= min_ && typed <= max_)) return;
        value_ = typed;
        changed();
    });
    entry_->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& event) {
        show();
        event.Skip();
    });
    entry_->Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& event) {
        switch (event.GetKeyCode()) {
            case WXK_UP: step(1); break;
            case WXK_DOWN: step(-1); break;
            case WXK_PAGEUP: step(10); break;
            case WXK_PAGEDOWN: step(-10); break;
            default: event.Skip();
        }
    });
    // one step per notch of the wheel
    auto wheel = [this](wxMouseEvent& event) {
        int const delta = std::max(1, event.GetWheelDelta());
        int const notches = std::max(1, std::abs(event.GetWheelRotation()) / delta);
        step(event.GetWheelRotation() > 0 ? notches : -notches);
    };
    entry_->Bind(wxEVT_MOUSEWHEEL, wheel);
    arrows_->Bind(wxEVT_MOUSEWHEEL, wheel);
    arrows_->Bind(wxEVT_SPIN_UP, [this](wxSpinEvent&) { step(1); });
    arrows_->Bind(wxEVT_SPIN_DOWN, [this](wxSpinEvent&) { step(-1); });
}

void NumberField::set_value(double value) {
    value_ = std::clamp(value, min_, max_);
    show();
}

void NumberField::show() {
    busy_ = true;
    entry_->ChangeValue(wx(fixed_text(value_, decimals_)) + suffix_);
    busy_ = false;
}

void NumberField::step(int amount) {
    double const next = std::clamp(value_ + amount, min_, max_);
    arrows_->SetValue(0);
    if (next == value_) return;
    value_ = next;
    show();
    changed();
}

void NumberField::changed() {
    wxCommandEvent event(EVT_NUMBER_CHANGED, GetId());
    event.SetEventObject(this);
    GetEventHandler()->ProcessEvent(event);
}
