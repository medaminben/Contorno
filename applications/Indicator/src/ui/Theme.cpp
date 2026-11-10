#include "ui/Theme.h"

#include "model/Language.h"
#include "ui/Fonts.h"
#include "ui/Text.h"

#include <wx/choice.h>
#include <wx/combobox.h>
#include <wx/dcmemory.h>
#include <wx/intl.h>
#include <wx/dcbuffer.h>
#include <wx/dialog.h>
#include <wx/graphics.h>
#include <wx/listbox.h>
#include <wx/listctrl.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/treectrl.h>

#ifdef INDICATOR_GTK3
#include <gtk/gtk.h>
#endif

#ifdef __WXMSW__
#include <dwmapi.h>
#include <uxtheme.h>
#endif

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

namespace {
bool light_ = false;

std::string text_of(char const* key) {
    return lookup(current_language(), key);
}
}  // namespace

bool light_theme() {
    return light_;
}
void set_light_theme(bool light) {
    light_ = light;
}

Palette palette(bool light) {
    // the Qt stylesheet's values; the keys' text is the same on both panels
    wxColour const green(0x47, 0xFF, 0x86);
    if (light) {
        return {wxColour("#E4E7E3"), wxColour("#000000"),
                wxColour("#F7F8F6"), wxColour("#000000"),
                wxColour("#B5BDB6"), wxColour("#000000"),
                wxColour("#000000"), green,
                wxColour("#127A36"), green,
                wxColour("#FFFFFF")};
    }
    return {wxColour("#000000"),
            wxColour("#FFFFFF"),
            wxColour("#050805"),
            wxColour("#FFFFFF"),
            wxColour("#1B2A1F"),
            green,
            green,
            wxColour("#000000"),
            green,
            green,
            wxColour("#FFFFFF")};
}

wxString result_colour(bool light) {
    return light ? "#127A36" : "#47FF86";
}

// ---------------------------------------------------------------- apply_theme

namespace {

bool is_field(wxWindow* window) {
    return dynamic_cast<wxTextCtrl*>(window) || dynamic_cast<wxChoice*>(window) ||
           dynamic_cast<wxComboBox*>(window) || dynamic_cast<wxListBox*>(window) ||
           dynamic_cast<wxListCtrl*>(window) || dynamic_cast<wxSpinCtrl*>(window) ||
           dynamic_cast<wxSpinCtrlDouble*>(window) || dynamic_cast<wxTreeCtrl*>(window);
}

#ifdef INDICATOR_GTK3
/// The fields' frame, the frame of the field in use and the selection, which wx
/// has no call for: GTK style rules for the classes "contorno-dark" and
/// "contorno-light", loaded once with both palettes. A field carries the class
/// of its own window's theme, so a window keeps its colours when another is themed.
void load_gtk_style() {
    static GtkCssProvider* provider = nullptr;
    if (provider)
        return;
    provider = gtk_css_provider_new();
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    auto const css = [](wxColour const& c) {
        return c.GetAsString(wxC2S_HTML_SYNTAX).utf8_string();
    };
    std::string rules;
    for (bool light : {false, true}) {
        Palette const p = palette(light);
        std::string const c = light ? ".contorno-light" : ".contorno-dark";
        rules += c + " { border: 2px solid " + css(p.frame) + "; border-radius: 6px; }\n" + c +
                 ":focus, " + c + " *:focus { border-color: " + css(p.focus) + "; }\n" + c +
                 " selection, " + c + " *:selected { background-color: " + css(p.selection) +
                 "; color: " + css(p.selection_text) + "; }\n";
    }
    gtk_css_provider_load_from_data(provider, rules.c_str(), -1, nullptr);
}

void apply_gtk_class(wxWindow* field, bool light) {
    GtkWidget* widget = field->GetHandle();
    if (!widget)
        return;
    load_gtk_style();
    GtkStyleContext* context = gtk_widget_get_style_context(widget);
    gtk_style_context_remove_class(context, light ? "contorno-dark" : "contorno-light");
    gtk_style_context_add_class(context, light ? "contorno-light" : "contorno-dark");
}
#endif

#ifdef __WXMSW__
/// The frame and the drop-down button that Windows draws itself: its dark visual style on the
/// black panels, the default one on the light panels.
void apply_windows_style(wxWindow* field, bool light) {
    SetWindowTheme(static_cast<HWND>(field->GetHandle()), light ? nullptr : L"DarkMode_CFD",
                   nullptr);
}
#endif

void apply(wxWindow* window, Palette const& p, bool light) {
#ifdef __WXMSW__
    if (is_field(window))
        apply_windows_style(window, light);
#endif
    if (is_field(window)) {
        window->SetBackgroundColour(p.field);
        window->SetForegroundColour(p.field_text);
#ifdef INDICATOR_GTK3
        apply_gtk_class(window, light);
#endif
    } else {
        window->SetBackgroundColour(p.panel);
        // a key paints its own text
        if (!dynamic_cast<Key*>(window))
            window->SetForegroundColour(p.text);
    }
    for (wxWindow* child : window->GetChildren())
        apply(child, p, light);
    window->Refresh();
}

}  // namespace

void apply_theme(wxWindow* top, bool light) {
    apply(top, palette(light), light);
    if (top->IsTopLevel())
        apply_title_bar(top, light);
}

void apply_title_bar(wxWindow* top, bool light) {
#ifdef __WXMSW__
    // DWMWA_USE_IMMERSIVE_DARK_MODE: 20 since Windows 10 20H1, 19 before it
    BOOL const dark = light ? FALSE : TRUE;
    HWND const window = static_cast<HWND>(top->GetHandle());
    if (FAILED(DwmSetWindowAttribute(window, 20, &dark, sizeof dark)))
        DwmSetWindowAttribute(window, 19, &dark, sizeof dark);
#else
    (void)top;
    (void)light;
#endif
}

// ---------------------------------------------------------------- tinted icons

namespace {

/// The tool icon `name` on a 20 x 20 grid, drawn into `gc` (already scaled).
void draw_tool_icon(wxGraphicsContext& gc, wxString const& name, wxColour const& colour) {
    wxPen pen(colour, 2);
    pen.SetCap(wxCAP_ROUND);
    pen.SetJoin(wxJOIN_ROUND);
    gc.SetPen(pen);
    gc.SetBrush(*wxTRANSPARENT_BRUSH);
    auto line = [&](double x1, double y1, double x2, double y2) { gc.StrokeLine(x1, y1, x2, y2); };
    auto polyline = [&](std::vector<wxPoint2DDouble> const& points) {
        gc.StrokeLines(points.size(), points.data());
    };
    auto arrow = [&](wxPoint2DDouble tip, wxPoint2DDouble tail, wxPoint2DDouble wing1,
                     wxPoint2DDouble wing2) {
        line(tail.m_x, tail.m_y, tip.m_x, tip.m_y);
        polyline({wing1, tip, wing2});
    };
    if (name == "back") {
        arrow({4, 10}, {16, 10}, {9, 5}, {9, 15});
    } else if (name == "forward") {
        arrow({16, 10}, {4, 10}, {11, 5}, {11, 15});
    } else if (name == "up") {
        arrow({10, 4}, {10, 16}, {5, 9}, {15, 9});
    } else if (name == "new folder") {
        polyline({{11, 16}, {3, 16}, {3, 4}, {8, 4}, {10, 6}, {17, 6}, {17, 10}});
        line(15, 12, 15, 18);
        line(12, 15, 18, 15);
    } else if (name == "link") {
        // two chain links side by side, overlapping in the middle
        gc.DrawRoundedRectangle(2.5, 7.5, 9, 5, 2.5);
        gc.DrawRoundedRectangle(8.5, 7.5, 9, 5, 2.5);
    } else if (name == "moon") {
        // a crescent cut from a disc, which keeps the horns sharp
        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.SetBrush(wxBrush(colour));
        gc.DrawEllipse(3, 3, 14, 14);
        gc.SetCompositionMode(wxCOMPOSITION_CLEAR);
        gc.DrawEllipse(13 - 6.5, 7.5 - 6.5, 13, 13);
        gc.SetCompositionMode(wxCOMPOSITION_OVER);
    } else if (name == "sun") {
        // a disc and eight short rays
        gc.SetBrush(wxBrush(colour));
        gc.DrawEllipse(6.5, 6.5, 7, 7);
        for (int i = 0; i < 8; ++i) {
            double const dx = std::cos(i * std::numbers::pi / 4),
                         dy = std::sin(i * std::numbers::pi / 4);
            line(10 + 6.5 * dx, 10 + 6.5 * dy, 10 + 8.5 * dx, 10 + 8.5 * dy);
        }
    } else if (name == "help") {
        wxFont const font = pixel_font(wxFont(wxFontInfo().Bold()), 17);
        gc.SetFont(font, colour);
        double w = 0, h = 0;
        gc.GetTextExtent("?", &w, &h);
        gc.DrawText("?", (20 - w) / 2, (20 - h) / 2);
    } else if (name == "ok") {
        polyline({{4, 10.5}, {8, 14.5}, {16, 5.5}});
    } else if (name == "cancel") {
        gc.DrawEllipse(3, 3, 14, 14);
        line(5.2, 5.2, 14.8, 14.8);
    } else if (name == "list") {
        for (double x : {3.0, 11.0}) {
            for (double y : {5.0, 10.0, 15.0})
                line(x, y, x + 6, y);
        }
    } else if (name == "details") {
        for (double y : {5.0, 10.0, 15.0}) {
            line(3, y, 3.01, y);
            line(7, y, 17, y);
        }
    }
}

wxBitmap tool_icon(wxString const& name, wxColour const& colour, int px) {
    wxBitmap bitmap(px, px, 32);
    wxMemoryDC dc(bitmap);
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (gc) {
        gc->SetCompositionMode(wxCOMPOSITION_SOURCE);
        gc->SetBrush(wxBrush(wxColour(0, 0, 0, 0)));
        gc->SetPen(*wxTRANSPARENT_PEN);
        gc->DrawRectangle(0, 0, px, px);
        gc->SetCompositionMode(wxCOMPOSITION_OVER);
        gc->Scale(px / 20.0, px / 20.0);
        draw_tool_icon(*gc, name, colour);
    }
    gc.reset();
    dc.SelectObject(wxNullBitmap);
    return bitmap;
}

}  // namespace

wxBitmapBundle tinted_icon(wxString const& name, wxColour const& colour, int size) {
    wxVector<wxBitmap> bitmaps;
    for (int px : {size, size * 3 / 2, size * 2})
        bitmaps.push_back(tool_icon(name, colour, px));
    return wxBitmapBundle::FromBitmaps(bitmaps);
}
// --------------------------------------------------------------------- dialogs

wxLayoutDirection layout_direction() {
    return text_direction() == TextDirection::right_to_left ? wxLayout_RightToLeft
                                                            : wxLayout_LeftToRight;
}

namespace {

/// A themed message dialog named `name` with one key per (id, label); Escape and Enter answer
/// `escape`.
int ask(wxWindow* parent, wxString const& title, wxString const& message, char const* name,
        std::vector<std::pair<wxWindowID, wxString>> const& answers, wxWindowID escape) {
    wxDialog dialog(parent, wxID_ANY, title);
    dialog.SetName(name);
    dialog.SetLayoutDirection(layout_direction());
    auto* label = new wxStaticText(&dialog, wxID_ANY, message);
    label->Wrap(dialog.FromDIP(420));
    auto* keys = new wxBoxSizer(wxHORIZONTAL);
    Key* first = nullptr;
    for (auto const& [id, text] : answers) {
        auto* key = new Key(&dialog, id, text, {}, 6, 18);
        keys->Add(key, 0, wxLEFT | wxRIGHT, 6);
        if (!first)
            first = key;
    }
    auto* column = new wxBoxSizer(wxVERTICAL);
    column->Add(label, 1, wxEXPAND | wxALL, 20);
    column->Add(keys, 0, wxALIGN_CENTER_HORIZONTAL | wxBOTTOM, 16);
    dialog.SetSizerAndFit(column);
    dialog.SetAffirmativeId(answers.front().first);
    dialog.SetEscapeId(escape);
    apply_theme(&dialog, light_theme());
    first->SetFocus();
    dialog.CentreOnParent();
    return dialog.ShowModal();
}

}  // namespace

void warn(wxWindow* parent, wxString const& title, wxString const& message) {
    ask(parent, title, message, "contorno-warning", {{wxID_OK, wx(text_of("common.ok"))}}, wxID_OK);
}

bool confirm(wxWindow* parent, wxString const& title, wxString const& message) {
    return ask(parent, title, message, "contorno-confirm",
               {{wxID_YES, wxGetTranslation("Yes")}, {wxID_NO, wxGetTranslation("No")}},
               wxID_NO) == wxID_YES;
}
