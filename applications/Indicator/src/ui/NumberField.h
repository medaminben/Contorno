#ifndef INDICATOR_NUMBERFIELD_H
#define INDICATOR_NUMBERFIELD_H
#include <wx/event.h>
#include <wx/panel.h>
#include <wx/spinbutt.h>
#include <wx/string.h>
#include <wx/textctrl.h>

#include <string>

/// `value` with `decimals` decimals and always a '.', whatever the locale.
std::string fixed_text(double value, int decimals);

/// The text of `field` (a text field or an editable combo box) at its left
/// edge in the field's current direction too, as Qt shows numbers; again after
/// a change of direction (GTK turns the alignment of a right-to-left field).
void keep_text_left(wxWindow* field);

/// Sent to the field's parent when the value changes, typed (within the range) or stepped.
wxDECLARE_EVENT(EVT_NUMBER_CHANGED, wxCommandEvent);

/// A number with a unit in a text field and a pair of step arrows: "1500 mm".
/// The text is written and read in English format (Latin digits, '.') whatever
/// the interface language, and the value is the last one typed within the range.
/// Left, the field shows the value again, clamped. Up and Down step by one, Page up and Page down
/// by ten, a notch of the wheel by one. Group separators are read as in English: "1,500" is 1500.
class NumberField : public wxPanel {
public:
    NumberField(wxWindow* parent, double min, double max, int decimals, wxString const& suffix,
                double value);

    double value() const { return value_; }
    /// Clamps to the range and shows the value; no event.
    void set_value(double value);
    /// The text as shown, "4.25 mm".
    wxString text() const { return entry_->GetValue(); }
    wxTextCtrl* entry() { return entry_; }

private:
    void show();
    void step(int amount);
    void changed();

    wxTextCtrl* entry_;
    wxSpinButton* arrows_;
    double min_, max_;
    int decimals_;
    wxString suffix_;
    double value_ {0};
    bool busy_ {false};
};

#endif  // INDICATOR_NUMBERFIELD_H
