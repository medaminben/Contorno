#include "ui/Fonts.h"

#include <wx/dcscreen.h>

#ifdef INDICATOR_GTK3
#include <gtk/gtk.h>
#endif

wxFont pixel_font(wxFont font, double pixels) {
    double dpi = 0;
#ifdef INDICATOR_GTK3
    dpi = gdk_screen_get_resolution(
        gdk_screen_get_default());  // what Pango turns points into pixels with
#endif
    if (dpi <= 0)
        dpi = wxScreenDC().GetPPI().y;
    if (dpi <= 0)
        dpi = 96;
    font.SetFractionalPointSize(pixels * 72.0 / dpi);
    return font;
}
