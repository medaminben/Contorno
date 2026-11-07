#ifndef INDICATOR_KEY_H
#define INDICATOR_KEY_H
#include <wx/bitmap.h>
#include <wx/bmpbndl.h>
#include <wx/control.h>
#include <wx/string.h>

/// A raised dark key drawn by hand: a face lit from above (grey to near
/// black), a light upper and a dark lower edge, the text in the readout's
/// green. Hovered the text turns white; pressed the key sinks: the shading
/// flips and the text moves down. `pad_y` and `pad_x` are the text's padding
/// in pixels. Emits wxEVT_BUTTON on a click, wxEVT_TOGGLEBUTTON when checkable
/// (a key that is on is lit green all round).
class Key : public wxControl {
public:
    Key(wxWindow* parent, wxWindowID id, wxString const& label, wxBitmapBundle const& icon = {},
        int pad_y = 0, int pad_x = 0);
    void SetCheckable(bool checkable);
    bool IsChecked() const { return checked_; }
    void SetChecked(bool checked);
    wxBitmapBundle const& icon() const { return icon_; }
    /// Replaces the icon (the theme key shows the moon, or the sun when it is on).
    void SetIcon(wxBitmapBundle const& icon);
    /// A round key: the corners are half the key's size and, hovered, hovered it gets a 1 px green
    /// ring.
    void SetRound(bool round);
    bool IsRound() const { return round_; }
    /// A click as the user makes it: toggles a checkable key and emits the event.
    void Click();
    /// The key as drawn, `scale` device pixels per logical one (the window's own scale for the
    /// screen).
    wxBitmap Render(double scale) const;

    bool AcceptsFocus() const override { return IsEnabled(); }
    bool SetFont(wxFont const& font) override;
    bool Enable(bool enable = true) override;
    void SetLabel(wxString const& label) override;

protected:
    wxSize DoGetBestSize() const override;

private:
    void draw(class wxGraphicsContext& gc, wxSize size, double scale) const;
    void on_paint(wxPaintEvent&);
    void set_hover(bool hover);

    wxBitmapBundle icon_;
    int pad_y_, pad_x_;
    bool checkable_ = false, checked_ = false, hover_ = false, pressed_ = false, round_ = false;
};

#endif  // INDICATOR_KEY_H
