#include "ui/AppIcon.h"

#include <wx/bmpbndl.h>
#include <wx/icon.h>
#include <wx/image.h>
#include <wx/log.h>
#include <wx/mstream.h>
#include <wx/settings.h>
#include <wx/toplevel.h>

#include "ui/PhotoFile.h"

// The Contorno C, compiled into the executable (see indicator_icon_data.cpp).
extern unsigned char const indicator_icon_dark_png[];
extern unsigned int const indicator_icon_dark_png_len;
extern unsigned char const indicator_icon_light_png[];
extern unsigned int const indicator_icon_light_png_len;
extern unsigned char const indicator_icon_dark_svg[];
extern unsigned char const indicator_icon_light_svg[];

namespace {

void add_png(wxIconBundle& bundle, unsigned char const* data, unsigned int len) {
    ensure_image_handlers();
    wxMemoryInputStream in(data, len);
    wxImage image;
    if (!image.LoadFile(in, wxBITMAP_TYPE_PNG))
        return;
    wxIcon icon;
    icon.CopyFromBitmap(wxBitmap(image));
    if (icon.IsOk())
        bundle.AddIcon(icon);
}

void add_svg(wxIconBundle& bundle, unsigned char const* svg) {
    wxBitmapBundle const drawn =
        wxBitmapBundle::FromSVG(reinterpret_cast<char const*>(svg), wxSize(64, 64));
    if (!drawn.IsOk())
        return;
    for (int size : {16, 24, 32, 48, 64, 128}) {
        wxIcon icon;
        icon.CopyFromBitmap(drawn.GetBitmap(wxSize(size, size)));
        if (icon.IsOk())
            bundle.AddIcon(icon);
    }
}

}  // namespace

wxIconBundle app_icon(bool dark_scheme) {
    wxIconBundle bundle;
    wxLogNull const quiet;
    if (dark_scheme) {
        add_png(bundle, indicator_icon_dark_png, indicator_icon_dark_png_len);
        add_svg(bundle, indicator_icon_dark_svg);
    } else {
        add_png(bundle, indicator_icon_light_png, indicator_icon_light_png_len);
        add_svg(bundle, indicator_icon_light_svg);
    }
    return bundle;
}

void follow_system_scheme(wxTopLevelWindow* window) {
    auto apply = [window] {
        window->SetIcons(app_icon(wxSystemSettings::GetAppearance().IsDark()));
    };
    apply();
    window->Bind(wxEVT_SYS_COLOUR_CHANGED, [apply](wxSysColourChangedEvent& event) {
        apply();
        event.Skip();
    });
    // SetIcons before the window is realized only stores the bundle. GTK applies it on realize,
    // and again here once the window exists, so the title-bar icon is the bundle's.
    window->Bind(wxEVT_SHOW, [apply](wxShowEvent& event) {
        if (event.IsShown())
            apply();
        event.Skip();
    });
}
