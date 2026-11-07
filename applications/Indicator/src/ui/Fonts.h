#ifndef INDICATOR_FONTS_H
#define INDICATOR_FONTS_H
#include <wx/font.h>

/// `font` with the size Qt's QFont::setPixelSize(`pixels`) gives: an em of
/// that many pixels. wxFont::SetPixelSize instead picks the size whose whole
/// line (ascent and descent) is that high, about a third smaller.
wxFont pixel_font(wxFont font, double pixels);

#endif  // INDICATOR_FONTS_H
