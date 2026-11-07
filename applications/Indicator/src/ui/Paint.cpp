#include "ui/Paint.h"

std::unique_ptr<wxGraphicsContext> paint_context(wxDC& dc, wxWindow const& window) {
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::CreateFromUnknownDC(dc));
#ifdef __WXGTK3__
    // measured on GTK 3 only: its paint DC of a right-to-left window comes mirrored; elsewhere the context is kept
    if (gc && dc.GetLayoutDirection() == wxLayout_RightToLeft) {
        gc->Translate(window.GetClientSize().x, 0);
        gc->Scale(-1, 1);
    }
#else
    (void)window;
#endif
    return gc;
}
