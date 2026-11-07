#include "ui/SegmentDisplay.h"

#include "model/Language.h"
#include "ui/Fonts.h"
#include "ui/Localise.h"
#include "model/Message.h"
#include "ui/Paint.h"
#include "ui/Text.h"
#include "ui/Theme.h"

#include <wx/dcbuffer.h>
#include <wx/dcmemory.h>
#include <wx/graphics.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace {

wxColour const face_colour(0x05, 0x08, 0x05), lit_colour(0x47, 0xFF, 0x86),
    unlit_colour(0x0D, 0x24, 0x14), frame_colour(0x1B, 0x2A, 0x1F);
constexpr double dimmed_brightness = 0.4;
constexpr int reference_height = 72;
constexpr double slant = 0.1051;  // tan(6 degrees)
constexpr int count_ms = 700;
constexpr int tick_ms = 15;

/// cm² per unit, 0 for a unit the count does not convert.
double cm2_per(std::string const& unit) {
    if (unit == "cm²")
        return 1;
    if (unit == "m²")
        return 10000;
    return 0;
}

// The display is laid out in reference units (height 72) and scaled as a whole.
constexpr double digit_top_ref = 20, digit_h_ref = 45;
constexpr double digit_w_ref = digit_h_ref * 0.52;
constexpr double pitch_ref =
    digit_w_ref + digit_h_ref * 0.26;  // room for the slant and the decimal point
constexpr double digits_left_ref =
    12 + digit_h_ref * 0.08;  // from the left edge of the digits block
constexpr double unit_width_ref = 36;
constexpr double right_margin_ref = 8;
constexpr double digits_block_ref =
    digits_left_ref + SegmentDisplay::cells * pitch_ref + 4 + unit_width_ref + right_margin_ref;

// Segment bits: a top, b top right, c bottom right, d bottom, e bottom left, f top left, g middle.
enum Segment : unsigned { a = 1, b = 2, c = 4, d = 8, e = 16, f = 32, g = 64 };

unsigned segments_of(char ch) {
    static constexpr std::array<unsigned, 10> digits {
        a | b | c | d | e | f, b | c,         a | b | d | e | g,
        a | b | c | d | g,     b | c | f | g, a | c | d | f | g,
        a | c | d | e | f | g, a | b | c,     a | b | c | d | e | f | g,
        a | b | c | d | f | g};
    if (ch >= '0' && ch <= '9')
        return digits[static_cast<std::size_t>(ch - '0')];
    if (ch == '-')
        return g;
    return 0;
}

/// One digit cell: its segment mask and whether its decimal point is lit.
struct Cell {
    unsigned mask = 0;
    bool point = false;
};

/// The cells for `text`, right-aligned; a '.' lights the point of the cell before it.
std::vector<Cell> layout_cells(std::string const& text) {
    std::vector<Cell> digits;
    for (char const ch : text) {
        if (ch == '.') {
            if (!digits.empty())
                digits.back().point = true;
        } else {
            digits.push_back({segments_of(ch), false});
        }
    }
    std::vector<Cell> out(static_cast<std::size_t>(SegmentDisplay::cells));
    std::size_t const n = std::min(digits.size(), out.size());
    for (std::size_t i = 0; i < n; ++i)
        out[out.size() - n + i] = digits[digits.size() - n + i];
    return out;
}

/// Digits `[0-9.]` with at most one point and at most `SegmentDisplay::cells` digits.
bool fits(std::string const& text) {
    int digits = 0, points = 0;
    for (char const ch : text) {
        if (ch == '.')
            ++points;
        else if (ch >= '0' && ch <= '9')
            ++digits;
        else
            return false;
    }
    return digits >= 1 && digits <= SegmentDisplay::cells && points <= 1;
}

/// `text` (digits with at most one point) as a number, locale-independent.
double to_number(std::string const& text) {
    double value = 0;
    std::from_chars(text.data(), text.data() + text.size(), value);
    return value;
}

std::string fixed(double value, int decimals) {
    char buffer[64];
    auto const result =
        std::to_chars(buffer, buffer + sizeof buffer, value, std::chars_format::fixed, decimals);
    return std::string(buffer, result.ptr);
}

wxColour lit(bool dimmed) {
    if (!dimmed)
        return lit_colour;
    auto mix = [](int fg, int back) {
        return static_cast<unsigned char>(
            std::lround(fg * dimmed_brightness + back * (1 - dimmed_brightness)));
    };
    return wxColour(mix(lit_colour.Red(), face_colour.Red()),
                    mix(lit_colour.Green(), face_colour.Green()),
                    mix(lit_colour.Blue(), face_colour.Blue()));
}

struct Point {
    double x, y;
};
using Polygon = std::vector<Point>;

/// Hexagonal segment centred on (cx, cy), `len` long and `t` thick along x (horizontal) or y
/// (vertical).
Polygon segment_polygon(double cx, double cy, double len, double t, bool horizontal) {
    double const h = len / 2, r = t / 2;
    if (horizontal)
        return {{cx - h, cy}, {cx - h + r, cy - r}, {cx + h - r, cy - r},
                {cx + h, cy}, {cx + h - r, cy + r}, {cx - h + r, cy + r}};
    return {{cx, cy - h}, {cx + r, cy - h + r}, {cx + r, cy + h - r},
            {cx, cy + h}, {cx - r, cy + h - r}, {cx - r, cy - h + r}};
}

/// Shears `p` about the horizontal line y = `pivot_y` by the italic slant.
Polygon slanted(Polygon p, double pivot_y) {
    for (Point& pt : p)
        pt.x -= (pt.y - pivot_y) * slant;
    return p;
}

wxGraphicsPath path_of(wxGraphicsContext& gc, Polygon const& p) {
    wxGraphicsPath path = gc.CreatePath();
    path.MoveToPoint(p[0].x, p[0].y);
    for (std::size_t i = 1; i < p.size(); ++i)
        path.AddLineToPoint(p[i].x, p[i].y);
    path.CloseSubpath();
    return path;
}

wxColour with_alpha(wxColour const& colour, double alpha) {
    return wxColour(colour.Red(), colour.Green(), colour.Blue(),
                    static_cast<unsigned char>(std::lround(alpha * 255)));
}

struct Rect {
    double x, y, w, h;
    Rect adjusted(double dx) const { return {x + dx, y + dx, w - 2 * dx, h - 2 * dx}; }
    double right() const { return x + w; }
};

/// The whole display scales with the smaller of the height- and width-based factors.
double scale_of(Rect const& outer) {
    return std::min(outer.h / reference_height, outer.w / digits_block_ref);
}

Rect outer_of(wxSize size) {
    return Rect {0.5, 0.5, size.x - 1.0, size.y - 1.0};
}

bool right_to_left() {
    return text_direction() == TextDirection::right_to_left;
}

}  // namespace

SegmentDisplay::SegmentDisplay(wxWindow* parent, wxWindowID id) : timer_(this) {
    wxWindow::Create(parent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetMinSize(wxSize(static_cast<int>(std::ceil(digits_block_ref * 0.5)), reference_height / 2));
    Bind(wxEVT_PAINT, &SegmentDisplay::on_paint, this);
    Bind(wxEVT_TIMER, &SegmentDisplay::on_tick, this);
    Bind(EVT_LANGUAGE_CHANGED, [this](wxCommandEvent& e) {
        Refresh();
        e.Skip();
    });  // the caption and its direction
    set_dashes();
}

void SegmentDisplay::set_value(wxString const& digits, wxString const& unit) {
    std::string text = utf8(digits);
    if (text.starts_with('.'))
        text = "0" + text;
    if (!fits(text)) {
        set_dashes();
        return;
    }
    std::string const old_unit = utf8(unit_), new_unit = utf8(unit);
    // where the count starts: the value shown, in the new unit (0 after dashes)
    bool const was_number = fits(utf8(shown_));
    double const from_cm2 = was_number ? to_number(utf8(shown_)) * cm2_per(old_unit) : 0;
    bool const convertible = cm2_per(new_unit) > 0 && (!was_number || cm2_per(old_unit) > 0);
    timer_.Stop();
    text_ = wx(text);
    unit_ = unit;
    auto const point = text.find('.');
    decimals_ = point == std::string::npos ? 0 : static_cast<int>(text.size() - point - 1);
    dimmed_ = false;
    double const to = to_number(text);
    double const from = convertible ? from_cm2 / cm2_per(new_unit) : to;
    shown_ = text_;
    if (from != to) {
        count_from_ = from;
        count_to_ = to;
        count_start_ = std::chrono::steady_clock::now();
        show_count(from);
        timer_.Start(tick_ms);
    }
    Refresh();
}

void SegmentDisplay::show_count(double value) {
    for (int decimals = decimals_; decimals >= 0; --decimals) {
        std::string const step = fixed(value, decimals);
        if (fits(step)) {
            shown_ = wx(step);
            Refresh();
            return;
        }
    }
}

void SegmentDisplay::on_tick(wxTimerEvent&) {
    double const t =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - count_start_)
            .count() /
        count_ms;
    if (t >= 1) {
        timer_.Stop();
        shown_ = text_;
        Refresh();
        return;
    }
    double const eased = 1 - std::pow(1 - t, 3);  // out-cubic: fast at first, settling on the value
    show_count(count_from_ + (count_to_ - count_from_) * eased);
}

void SegmentDisplay::set_dashes() {
    timer_.Stop();
    text_ = wxString(std::string(cells, '-'));
    unit_.clear();
    shown_ = text_;
    dimmed_ = false;
    Refresh();
}

void SegmentDisplay::set_dimmed(bool dimmed) {
    if (dimmed_ == dimmed)
        return;
    dimmed_ = dimmed;
    Refresh();
}

wxSize SegmentDisplay::DoGetBestSize() const {
    return wxSize(static_cast<int>(std::ceil(digits_block_ref)), reference_height);
}

double SegmentDisplay::corner_radius() const {
    return 6 * scale_of(outer_of(GetClientSize()));
}

double SegmentDisplay::frame_width() const {
    return std::max(1.0, 2 * scale_of(outer_of(GetClientSize())));
}

wxFont SegmentDisplay::caption_font(double scale) const {
    wxFont caption = GetFont();
    return pixel_font(caption, std::max(7, static_cast<int>(std::lround(9 * scale))));
}

double SegmentDisplay::caption_spacing(double scale) const {
    return right_to_left() ? 0 : 1 * scale;
}

void SegmentDisplay::draw(wxGraphicsContext& gc, wxSize size, double) const {
    gc.SetAntialiasMode(wxANTIALIAS_DEFAULT);
    // the surround, so the rounded corners show the parent's colour
    gc.SetPen(*wxTRANSPARENT_PEN);
    gc.SetBrush(wxBrush(GetParent() ? GetParent()->GetBackgroundColour() : GetBackgroundColour()));
    gc.DrawRectangle(0, 0, size.x, size.y);

    Rect const outer = outer_of(size);
    double const scale = scale_of(outer);

    // face with an inset frame
    double const radius = 6 * scale;
    Rect const face = outer.adjusted(scale);
    gc.SetPen(gc.CreatePen(wxGraphicsPenInfo(frame_colour).Width(std::max(1.0, 2 * scale))));
    gc.SetBrush(wxBrush(face_colour));
    gc.DrawRoundedRectangle(face.x, face.y, face.w, face.h, radius);
    Rect const inner = outer.adjusted(3 * scale);
    gc.SetPen(gc.CreatePen(wxGraphicsPenInfo(wxColour(0, 0, 0)).Width(std::max(1.0, scale))));
    gc.SetBrush(*wxTRANSPARENT_BRUSH);
    gc.DrawRoundedRectangle(inner.x, inner.y, inner.w, inner.h, 4 * scale);

    wxColour const green = lit(dimmed_);

    // caption top-left, in the lit green; Arabic as Qt draws it: in the right half, right to left,
    // where the alignment "right" of Qt's right-to-left text is the half's left edge
    bool const rtl = right_to_left();
    gc.SetFont(caption_font(scale), green);
    wxString const caption = wx(Message("display.caption").render());
    double const spacing = caption_spacing(scale);
    double text_h = 0;
    gc.GetTextExtent(caption, nullptr, &text_h);
    double const caption_y = outer.y + 5 * scale + (12 * scale - text_h) / 2;
    double caption_x = rtl ? outer.right() - 10 * scale - outer.w / 2 : outer.x + 10 * scale;
    if (spacing > 0) {
        for (wxUniChar const ch : caption) {
            wxString const letter(ch);
            double letter_w = 0, letter_h = 0;
            gc.GetTextExtent(letter, &letter_w, &letter_h);
            gc.DrawText(letter, caption_x, caption_y);
            caption_x += letter_w + spacing;
        }
    } else {
        gc.DrawText(caption, caption_x, caption_y);
    }

    // digits and unit: right-aligned, vertically centred, drawn in reference units
    double const origin_x = outer.right() - digits_block_ref * scale;
    double const origin_y = outer.y + outer.h / 2 - reference_height * scale / 2;
    gc.PushState();
    gc.Translate(origin_x, origin_y);
    gc.Scale(scale, scale);

    double const digit_top = digit_top_ref, digit_h = digit_h_ref, digit_w = digit_w_ref;
    double const t = digit_h * 0.13;  // segment thickness
    double const gap = t * 0.22;
    double const mid_y = digit_top + digit_h / 2;

    auto const cell_list = layout_cells(utf8(shown_));
    wxColour const glow = with_alpha(green, 0.22);
    for (std::size_t i = 0; i < cell_list.size(); ++i) {
        double const x0 = digits_left_ref + static_cast<double>(i) * pitch_ref;
        double const cx = x0 + digit_w / 2;
        double const hl = digit_w - t - 2 * gap;          // horizontal segment length
        double const vl = digit_h / 2 - t / 2 - 2 * gap;  // vertical segment length
        double const top = digit_top + t / 2, bot = digit_top + digit_h - t / 2;
        double const left_x = x0 + t / 2, right_x = x0 + digit_w - t / 2;
        std::array<Polygon, 7> const polys {
            segment_polygon(cx, top, hl, t, true),
            segment_polygon(right_x, (top + mid_y) / 2, vl, t, false),
            segment_polygon(right_x, (mid_y + bot) / 2, vl, t, false),
            segment_polygon(cx, bot, hl, t, true),
            segment_polygon(left_x, (mid_y + bot) / 2, vl, t, false),
            segment_polygon(left_x, (top + mid_y) / 2, vl, t, false),
            segment_polygon(cx, mid_y, hl, t, true)};

        // unlit segments always, so the cell never looks empty
        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.SetBrush(wxBrush(unlit_colour));
        for (Polygon const& poly : polys)
            gc.FillPath(path_of(gc, slanted(poly, mid_y)));
        double const dp_r = t * 0.62;
        double const dp_y = digit_top + digit_h - dp_r;
        Point const dp {x0 + digit_w + t * 0.95 - (dp_y - mid_y) * slant, dp_y};
        gc.DrawEllipse(dp.x - dp_r, dp.y - dp_r, 2 * dp_r, 2 * dp_r);

        // lit segments: a wide translucent stroke for the glow, then the solid shape
        for (std::size_t s = 0; s < polys.size(); ++s) {
            if (!(cell_list[i].mask & (1u << s)))
                continue;
            wxGraphicsPath const path = path_of(gc, slanted(polys[s], mid_y));
            gc.SetPen(gc.CreatePen(
                wxGraphicsPenInfo(glow).Width(t * 1.1).Join(wxJOIN_ROUND).Cap(wxCAP_ROUND)));
            gc.SetBrush(wxBrush(glow));
            gc.DrawPath(path);
            gc.SetPen(*wxTRANSPARENT_PEN);
            gc.SetBrush(wxBrush(green));
            gc.FillPath(path);
        }
        if (cell_list[i].point) {
            gc.SetPen(*wxTRANSPARENT_PEN);
            gc.SetBrush(wxBrush(glow));
            gc.DrawEllipse(dp.x - dp_r * 1.7, dp.y - dp_r * 1.7, dp_r * 3.4, dp_r * 3.4);
            gc.SetBrush(wxBrush(green));
            gc.DrawEllipse(dp.x - dp_r, dp.y - dp_r, 2 * dp_r, 2 * dp_r);
        }
    }
    gc.PopState();

    // unit to the right of the digits, bottom-aligned with them (drawn unscaled so the font is
    // exact)
    if (!unit_.empty()) {
        wxFont unit_font = GetFont();
        unit_font = pixel_font(unit_font,
                               std::max(7, static_cast<int>(std::lround(digit_h * 0.34 * scale))));
        unit_font.MakeBold();
        gc.SetFont(unit_font, green);
        double unit_w = 0, unit_h = 0;
        gc.GetTextExtent(unit_, &unit_w, &unit_h);
        double const ux = origin_x + (digits_left_ref + cells * pitch_ref + 4) * scale;
        gc.DrawText(unit_, ux, origin_y + (digit_top + digit_h) * scale - unit_h);
    }
}

void SegmentDisplay::on_paint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    std::unique_ptr<wxGraphicsContext> gc = paint_context(dc, *this);
    if (gc)
        draw(*gc, GetClientSize(), GetContentScaleFactor());
}

wxBitmap SegmentDisplay::render(double scale) const {
    wxSize const size = GetClientSize();
    wxBitmap bitmap;
    bitmap.CreateWithDIPSize(size, scale);
    wxMemoryDC dc(bitmap);
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (gc)
        draw(*gc, size, scale);
    gc.reset();
    dc.SelectObject(wxNullBitmap);
    return bitmap;
}
