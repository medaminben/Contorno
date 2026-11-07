#include "ui/FlagIcons.h"

#include <wx/bmpbndl.h>
#include <wx/image.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

/// The flags as square SVGs (viewBox 0 0 100 100) from the Qt Indicator's drawing:
/// the United Kingdom stretched to the square so the diagonals survive the disc
/// (the counterchanged red saltire as four half strips: the SVG renderer has no
/// clip paths), Spain its bands only, Tunisia zoomed 1.5 times about the centre.
char const* svg_of(std::string const& code) {
    if (code == "en") return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100" viewBox="0 0 100 100"><g transform="scale(2 3.3333)"><rect width="50" height="30" fill="#012169"/><path d="M0 0L50 30M50 0L0 30" stroke="#fff" stroke-width="6"/><polygon fill="#C8102E" points="25.00,15.00 50.00,30.00 51.03,28.29 26.03,13.29"/><polygon fill="#C8102E" points="25.00,15.00 0.00,30.00 1.03,31.71 26.03,16.71"/><polygon fill="#C8102E" points="25.00,15.00 0.00,0.00 -1.03,1.71 23.97,16.71"/><polygon fill="#C8102E" points="25.00,15.00 50.00,0.00 48.97,-1.71 23.97,13.29"/><polygon fill="#C8102E" stroke="#fff" stroke-width="2" points="-1,11 21,11 21,-1 29,-1 29,11 51,11 51,19 29,19 29,31 21,31 21,19 -1,19"/></g></svg>)SVG";
    if (code == "de") return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100" viewBox="0 0 100 100"><rect x="0" y="0.0000" width="100" height="33.3333" fill="#000000"/><rect x="0" y="33.3333" width="100" height="33.3333" fill="#DD0000"/><rect x="0" y="66.6667" width="100" height="33.3333" fill="#FFCE00"/></svg>)SVG";
    if (code == "es") return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100" viewBox="0 0 100 100"><rect x="0" y="0.0000" width="100" height="25.0000" fill="#AA151B"/><rect x="0" y="25.0000" width="100" height="50.0000" fill="#F1BF00"/><rect x="0" y="75.0000" width="100" height="25.0000" fill="#AA151B"/></svg>)SVG";
    if (code == "it") return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100" viewBox="0 0 100 100"><rect x="0.0000" y="0" width="33.3333" height="100" fill="#009246"/><rect x="33.3333" y="0" width="33.3333" height="100" fill="#FFFFFF"/><rect x="66.6667" y="0" width="33.3333" height="100" fill="#CE2B37"/></svg>)SVG";
    if (code == "fr") return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100" viewBox="0 0 100 100"><rect x="0.0000" y="0" width="33.3333" height="100" fill="#002654"/><rect x="33.3333" y="0" width="33.3333" height="100" fill="#FFFFFF"/><rect x="66.6667" y="0" width="33.3333" height="100" fill="#CE1126"/></svg>)SVG";
    if (code == "nl") return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100" viewBox="0 0 100 100"><rect x="0" y="0.0000" width="100" height="33.3333" fill="#AE1C28"/><rect x="0" y="33.3333" width="100" height="33.3333" fill="#FFFFFF"/><rect x="0" y="66.6667" width="100" height="33.3333" fill="#21468B"/></svg>)SVG";
    if (code == "ar") return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100" viewBox="0 0 100 100"><g transform="translate(50 50) scale(1.875)"><rect x="-60" y="-40" width="120" height="80" fill="#E70013"/><circle r="20" fill="#fff"/><circle r="15" fill="#E70013"/><circle cx="4" r="12" fill="#fff"/><polygon fill="#E70013" fill-rule="nonzero" points="-5,0 11.281,-5.29 1.22,8.56 1.22,-8.56 11.28,5.29"/></g></svg>)SVG";
    return nullptr;
}

/// An SVG flag cut to a disc by an antialiased mask, at any size.
class DiscFlag : public wxBitmapBundleImpl {
public:
    DiscFlag(char const* svg, wxSize size, double opacity) : svg_(svg), size_(size), opacity_(opacity) {}

    wxSize GetDefaultSize() const override { return size_; }
    wxSize GetPreferredBitmapSizeAtScale(double scale) const override {
        int const side = std::max(1, static_cast<int>(std::lround(size_.x * scale)));
        return wxSize(side, side);
    }
    wxBitmap GetBitmap(const wxSize& size) override {
        wxBitmapBundle const flag = wxBitmapBundle::FromSVG(svg_, size);
        wxImage image = flag.GetBitmap(size).ConvertToImage();
        if (!image.HasAlpha()) image.InitAlpha();
        double const radius = size.x / 2.0, centre = size.x / 2.0;
        for (int y = 0; y < size.y; ++y) {
            for (int x = 0; x < size.x; ++x) {
                // one pixel of coverage across the edge
                double const d = std::hypot(x + 0.5 - centre, y + 0.5 - centre);
                double const coverage = std::clamp(radius - d + 0.5, 0.0, 1.0);
                image.SetAlpha(x, y, static_cast<unsigned char>(std::lround(image.GetAlpha(x, y) * coverage * opacity_)));
            }
        }
        return wxBitmap(image);
    }

private:
    char const* svg_;
    wxSize size_;
    double opacity_;
};

}  // namespace

wxBitmapBundle flag_icon(std::string const& code, bool disabled) {
    char const* const svg = svg_of(code);
    if (!svg) return {};
    return wxBitmapBundle::FromImpl(new DiscFlag(svg, wxSize(24, 24), disabled ? 0.45 : 1.0));
}
