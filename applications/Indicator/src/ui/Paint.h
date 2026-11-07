#ifndef INDICATOR_PAINT_H
#define INDICATOR_PAINT_H
#include <wx/dc.h>
#include <wx/graphics.h>
#include <wx/window.h>

#include <memory>

/// A graphics context on `window`'s paint DC `dc` that is never mirrored. GTK 3
/// gives a right-to-left window a context mirrored left to right, where text
/// and bitmaps come out back to front; the drawn windows place their content
/// themselves, as the Qt Indicator's do. Other toolkits get the DC's own context.
std::unique_ptr<wxGraphicsContext> paint_context(wxDC& dc, wxWindow const& window);

#endif  // INDICATOR_PAINT_H
