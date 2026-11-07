#include "ui/Key.h"

#include "ui/Paint.h"
#include "ui/Theme.h"

#include <wx/dcbuffer.h>
#include <wx/dcmemory.h>
#include <wx/graphics.h>
#include <wx/tglbtn.h>

#include <algorithm>
#include <cmath>
#include <memory>

namespace {

wxColour const edge_top("#9aa29c"), edge_left("#5e6660"), edge_right("#101310"), edge_dark("#000000");
wxColour const key_disabled("#5a6b5f");
constexpr int gap_icon_text = 6;

void fill_rounded(wxGraphicsContext& gc, wxColour const& colour, double x, double y, double w, double h, double r) {
    gc.SetPen(*wxTRANSPARENT_PEN);
    gc.SetBrush(wxBrush(colour));
    gc.DrawRoundedRectangle(x, y, w, h, r);
}

/// `icon` redrawn in the disabled colour (#3a4a3f, as the Qt Indicator's tinted icons), keeping its alpha.
wxBitmap dimmed(wxBitmap const& icon) {
    wxImage image = icon.ConvertToImage();
    if (!image.HasAlpha()) image.InitAlpha();
    unsigned char* rgb = image.GetData();
    int const pixels = image.GetWidth() * image.GetHeight();
    for (int i = 0; i < pixels; ++i) {
        rgb[3 * i] = 0x3a;
        rgb[3 * i + 1] = 0x4a;
        rgb[3 * i + 2] = 0x3f;
    }
    return wxBitmap(image);
}

}  // namespace

Key::Key(wxWindow* parent, wxWindowID id, wxString const& label, wxBitmapBundle const& icon, int pad_y, int pad_x)
    : icon_(icon), pad_y_(pad_y), pad_x_(pad_x) {
    wxControl::Create(parent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    wxControl::SetLabel(label);
    SetBackgroundColour(palette(light_theme()).panel);
    SetCursor(wxCursor(wxCURSOR_HAND));
    Bind(wxEVT_PAINT, &Key::on_paint, this);
    Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent& e) { set_hover(true); e.Skip(); });
    Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent& e) { set_hover(false); e.Skip(); });
    // a second quick press arrives as a double click, not as another LEFT_DOWN
    auto press = [this](wxMouseEvent&) {
        if (!IsEnabled()) return;
        SetFocus();
        pressed_ = true;
        if (!HasCapture()) CaptureMouse();
        Refresh();
    };
    Bind(wxEVT_LEFT_DOWN, press);
    Bind(wxEVT_LEFT_DCLICK, press);
    Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& e) {
        bool const was_pressed = pressed_;
        pressed_ = false;
        if (HasCapture()) ReleaseMouse();
        Refresh();
        if (was_pressed && GetClientRect().Contains(e.GetPosition())) Click();
    });
    Bind(wxEVT_MOUSE_CAPTURE_LOST, [this](wxMouseCaptureLostEvent&) { pressed_ = false; Refresh(); });
    // Enter on a focused key presses that key; the dialog's hook would press its default key
    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& e) {
        if ((e.GetKeyCode() == WXK_RETURN || e.GetKeyCode() == WXK_NUMPAD_ENTER) && IsEnabled()) {
            Click();
        } else {
            e.Skip();
        }
    });
    Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& e) {
        if (e.GetKeyCode() == WXK_SPACE || e.GetKeyCode() == WXK_RETURN) {
            pressed_ = true;
            Refresh();
        } else {
            e.Skip();
        }
    });
    Bind(wxEVT_KEY_UP, [this](wxKeyEvent& e) {
        if ((e.GetKeyCode() == WXK_SPACE || e.GetKeyCode() == WXK_RETURN) && pressed_) {
            pressed_ = false;
            Click();
        } else {
            e.Skip();
        }
    });
    Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent& e) { Refresh(); e.Skip(); });
    Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& e) { pressed_ = false; Refresh(); e.Skip(); });
}

void Key::SetCheckable(bool checkable) {
    checkable_ = checkable;
    if (!checkable_) checked_ = false;
    Refresh();
}

void Key::SetChecked(bool checked) {
    checked_ = checkable_ && checked;
    Refresh();
}

void Key::SetIcon(wxBitmapBundle const& icon) {
    icon_ = icon;
    InvalidateBestSize();
    Refresh();
}

void Key::SetRound(bool round) {
    round_ = round;
    Refresh();
}

bool Key::SetFont(wxFont const& font) {
    bool const changed = wxControl::SetFont(font);
    InvalidateBestSize();
    Refresh();
    return changed;
}

bool Key::Enable(bool enable) {
    bool const changed = wxControl::Enable(enable);
    if (!enable) pressed_ = hover_ = false;
    Refresh();
    return changed;
}

void Key::SetLabel(wxString const& label) {
    wxControl::SetLabel(label);
    InvalidateBestSize();
    Refresh();
}

void Key::set_hover(bool hover) {
    hover_ = hover;
    Refresh();
}

void Key::Click() {
    if (!IsEnabled()) return;
    wxEventType type = wxEVT_BUTTON;
    if (checkable_) {
        checked_ = !checked_;
        type = wxEVT_TOGGLEBUTTON;
    }
    wxCommandEvent event(type, GetId());
    event.SetEventObject(this);
    event.SetInt(checked_ ? 1 : 0);
    Refresh();
    GetEventHandler()->ProcessEvent(event);
}

wxSize Key::DoGetBestSize() const {
    wxSize const text = GetTextExtent(GetLabelText());
    wxSize content(text.x, text.y);
    if (icon_.IsOk()) {
        wxSize const icon = icon_.GetDefaultSize();
        content.x = (text.x > 0 ? text.x + gap_icon_text : 0) + icon.x;
        content.y = std::max(text.y, icon.y);
    }
    // 1 px edges left and right, 1 above and 5 below
    return wxSize(content.x + 2 * pad_x_ + 2, content.y + 2 * pad_y_ + 6);
}

void Key::draw(wxGraphicsContext& gc, wxSize size, double scale) const {
    Palette const p = palette(false);  // the keys are dark in both themes
    double const w = size.x, h = size.y;
    gc.SetAntialiasMode(wxANTIALIAS_DEFAULT);
    // the surround, so the rounded corners show the panel the key is on
    gc.SetPen(*wxTRANSPARENT_PEN);
    gc.SetBrush(wxBrush(GetParent() ? GetParent()->GetBackgroundColour() : GetBackgroundColour()));
    gc.DrawRectangle(0, 0, w, h);

    double const radius = round_ ? std::min(w, h) / 2 : 8;
    bool const sunk = pressed_ || checked_;
    double face_top, face_height;
    wxGraphicsGradientStops stops;
    fill_rounded(gc, edge_dark, 0, 0, w, h, radius);
    if (!sunk) {
        // 1 px light above, 1 px left and right, 5 px dark below
        fill_rounded(gc, edge_right, 0, 0, w, h - 4, radius);
        fill_rounded(gc, edge_left, 0, 0, w - 1, h - 4, radius);
        fill_rounded(gc, edge_top, 1, 0, w - 1, h - 5, radius);
        face_top = 1;
        face_height = h - 6;
        stops = wxGraphicsGradientStops(wxColour("#5f6862"), wxColour("#1e231f"));
        stops.Add(wxColour("#343a35"), 0.5f);
    } else {
        // 5 px dark above, 1 px below
        fill_rounded(gc, edge_right, 0, 4, w, h - 4, radius);
        fill_rounded(gc, edge_left, 0, 4, w - 1, h - 4, radius);
        face_top = 5;
        face_height = h - 6;
        stops = wxGraphicsGradientStops(wxColour("#050805"), wxColour("#262b27"));
        stops.Add(wxColour("#121612"), 0.6f);
    }
    bool const lit = checked_;
    if (lit) {
        // lit green all round
        fill_rounded(gc, p.key_text, 0, 2, w, h - 3, radius);
        face_top = 4;
        face_height = h - 7;
    }
    double const face_x = lit ? 2 : 1, face_w = lit ? w - 4 : w - 2;
    gc.SetBrush(gc.CreateLinearGradientBrush(0, face_top, 0, face_top + face_height, stops));
    gc.DrawRoundedRectangle(face_x, face_top, face_w, face_height, radius - 1);

    if (round_ && hover_ && !checked_ && IsEnabled()) {
        // hovered, a round key gets a 1 px green ring (a key that is on has it already)
        gc.SetPen(wxPen(p.key_text, 1));
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        gc.DrawRoundedRectangle(0.5, 0.5, w - 1, h - 1, radius - 0.5);
    }

    // the content, centred in the face: the icon before the text, after it right to left
    wxString const label = GetLabelText();
    wxSize icon_size;
    wxBitmap icon;
    if (icon_.IsOk()) {
        icon_size = icon_.GetDefaultSize();
        icon = icon_.GetBitmap(wxSize(std::lround(icon_size.x * scale), std::lround(icon_size.y * scale)));
        if (!IsEnabled()) icon = dimmed(icon);
    }
    wxColour colour = IsEnabled() ? ((hover_ || pressed_) ? p.key_hover : p.key_text) : key_disabled;
    gc.SetFont(GetFont(), colour);
    double text_w = 0, text_h = 0;
    if (!label.empty()) gc.GetTextExtent(label, &text_w, &text_h);
    double const total = text_w + (icon.IsOk() ? icon_size.x + (text_w > 0 ? gap_icon_text : 0) : 0);
    double x = std::floor(face_x + (face_w - total) / 2);
    double const middle = face_top + face_height / 2;
    bool const icon_last = GetLayoutDirection() == wxLayout_RightToLeft;
    auto draw_icon = [&] {
        gc.DrawBitmap(icon, x, std::floor(middle - icon_size.y / 2.0), icon_size.x, icon_size.y);
        x += icon_size.x + (text_w > 0 ? gap_icon_text : 0);
    };
    if (icon.IsOk() && !icon_last) draw_icon();
    if (!label.empty()) {
        gc.DrawText(label, x, std::floor(middle - text_h / 2));
        x += text_w + gap_icon_text;
    }
    if (icon.IsOk() && icon_last) draw_icon();
}

void Key::on_paint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    std::unique_ptr<wxGraphicsContext> gc = paint_context(dc, *this);
    if (!gc) return;
    draw(*gc, GetClientSize(), GetContentScaleFactor());
    if (HasFocus() && !pressed_) {
        gc->SetPen(wxPen(palette(false).key_text, 1, wxPENSTYLE_DOT));
        gc->SetBrush(*wxTRANSPARENT_BRUSH);
        gc->DrawRoundedRectangle(3.5, 3.5, GetClientSize().x - 7, GetClientSize().y - 11, 4);
    }
}

wxBitmap Key::Render(double scale) const {
    wxSize const size = GetClientSize();
    wxBitmap bitmap;
    bitmap.CreateWithDIPSize(size, scale);
    wxMemoryDC dc(bitmap);
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (gc) draw(*gc, size, scale);
    gc.reset();
    dc.SelectObject(wxNullBitmap);
    return bitmap;
}
