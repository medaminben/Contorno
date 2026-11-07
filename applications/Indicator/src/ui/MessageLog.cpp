#include "ui/MessageLog.h"

#include "model/Language.h"
#include "ui/Fonts.h"
#include "ui/Localise.h"
#include "ui/Paint.h"
#include "ui/Text.h"

#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/dcbuffer.h>
#include <wx/dcmemory.h>
#include <wx/datetime.h>
#ifdef INDICATOR_GTK3
#include <wx/fontutil.h>
#endif
#include <wx/graphics.h>
#include <wx/menu.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <string>
#include <vector>
#include <cctype>

// The text of a message is laid out, measured and painted by one TextLayout, so the caret, a click
// and the selection agree with what is on screen. On GTK it is Pango, the toolkit's own text
// engine, which also handles right-to-left and mixed text. Elsewhere a portable layout wraps at
// spaces and measures with the device context in logical order (left to right extents; a
// right-to-left line is only right-aligned). Everything toolkit-specific stays inside the two
// TextLayout classes and word_class().
#ifdef INDICATOR_GTK3
#include <cairo.h>
#include <gtk/gtk.h>
#include <pango/pangocairo.h>
#endif

namespace {

// The area readout's face, frame and lit green (SegmentDisplay.cpp).
wxColour const face_colour(0x05, 0x08, 0x05), lit_colour(0x47, 0xFF, 0x86),
    frame_colour(0x1B, 0x2A, 0x1F);
wxColour const selection_colour(0x1f, 0x5c, 0x35), selection_text_colour(0xc8, 0xff, 0xd9);
constexpr int glide_ms = 300;
constexpr int tick_ms = 15;
constexpr int pad_x = 8, pad_y = 4;  // the text's padding inside the frame
constexpr double glow_alpha = 140 / 255.0, glow_blur = 8;
constexpr double bar_width = 8, bar_margin = 2;

wxColour with_alpha(wxColour const& colour, double alpha) {
    return wxColour(colour.Red(), colour.Green(), colour.Blue(),
                    static_cast<unsigned char>(std::lround(alpha * 255)));
}

/// A rectangle in a layout's coordinates (px, origin at its top-left).
struct Box {
    double x, y, w, h;
};

#ifdef INDICATOR_GTK3

struct Unref {
    void operator()(PangoLayout* p) const { g_object_unref(p); }
};

/// One message's text as a Pango layout wrapped at the log's width, with the UTF-8 byte of each
/// character. A right-to-left line gets that base direction whatever its first letter is.
class TextLayout {
public:
    TextLayout(wxWindow const* window, wxFont const& font, wxString const& text, bool rtl,
               int width) {
        PangoContext* context =
            gtk_widget_create_pango_context(static_cast<GtkWidget*>(window->GetHandle()));
        pango_context_set_base_dir(context, rtl ? PANGO_DIRECTION_RTL : PANGO_DIRECTION_LTR);
        layout_.reset(pango_layout_new(context));
        g_object_unref(context);
        pango_layout_set_auto_dir(layout_.get(), FALSE);
        pango_layout_set_font_description(layout_.get(), font.GetNativeFontInfo()->description);
        pango_layout_set_wrap(layout_.get(), PANGO_WRAP_WORD_CHAR);
        pango_layout_set_alignment(layout_.get(),
                                   rtl ? PANGO_ALIGN_RIGHT : PANGO_ALIGN_LEFT);  // physical sides
        pango_layout_set_width(layout_.get(), width * PANGO_SCALE);
        utf8_ = text.utf8_string();
        pango_layout_set_text(layout_.get(), utf8_.data(), static_cast<int>(utf8_.size()));
        byte_of_.push_back(0);
        for (wxUniChar const ch : text)
            byte_of_.push_back(byte_of_.back() +
                               static_cast<int>(wxString(ch).utf8_string().size()));
    }
    int rows() const { return pango_layout_get_line_count(layout_.get()); }
    double height() const {
        PangoRectangle logical;
        pango_layout_get_extents(layout_.get(), nullptr, &logical);
        return static_cast<double>(logical.height) / PANGO_SCALE;
    }
    /// The character boundary nearest to (x, y).
    std::size_t hit(double x, double y, int width) const {
        int index = 0, trailing = 0;
        pango_layout_xy_to_index(layout_.get(),
                                 static_cast<int>(std::clamp<double>(x, 0, width) * PANGO_SCALE),
                                 static_cast<int>(y * PANGO_SCALE), &index, &trailing);
        return chr(index) + static_cast<std::size_t>(trailing);
    }
    /// The caret before character `offset`: x, and y at the middle of its row.
    wxPoint2DDouble caret(std::size_t offset) const {
        PangoRectangle strong;
        pango_layout_get_cursor_pos(layout_.get(), byte(offset), &strong, nullptr);
        return {strong.x / double(PANGO_SCALE), (strong.y + strong.height / 2.0) / PANGO_SCALE};
    }
    /// Where characters [a, b) are drawn; a mixed-direction row can give several boxes.
    std::vector<Box> selection(std::size_t a, std::size_t b) const {
        std::vector<Box> out;
        int const sa = byte(a), sb = byte(b);
        PangoLayoutIter* iter = pango_layout_get_iter(layout_.get());
        do {
            PangoLayoutLine* row = pango_layout_iter_get_line_readonly(iter);
            PangoRectangle logical;
            pango_layout_iter_get_line_extents(iter, nullptr, &logical);
            int const s = std::max(sa, row->start_index),
                      e = std::min(sb, row->start_index + row->length);
            if (s >= e)
                continue;
            int* ranges = nullptr;
            int count = 0;
            // the ranges are in the layout's coordinates already (the row's alignment is in them)
            pango_layout_line_get_x_ranges(row, s, e, &ranges, &count);
            for (int k = 0; k < count; ++k) {
                double const x0 = ranges[2 * k] / double(PANGO_SCALE),
                             x1 = ranges[2 * k + 1] / double(PANGO_SCALE);
                out.push_back({x0, logical.y / double(PANGO_SCALE), x1 - x0,
                               logical.height / double(PANGO_SCALE)});
            }
            g_free(ranges);
        } while (pango_layout_iter_next_line(iter));
        pango_layout_iter_free(iter);
        return out;
    }
    /// Draws the text at (x, y) in `fg`; characters [a, b) in `selected` (or not at all with
    /// `skip_selected`).
    void paint(wxGraphicsContext& gc, double x, double y, wxColour const& fg, std::size_t a,
               std::size_t b, wxColour const& selected, bool skip_selected) const {
        PangoAttrList* attrs = pango_attr_list_new();
        if (a < b) {
            PangoAttribute* attr = skip_selected ? pango_attr_foreground_alpha_new(0)
                                                 : pango_attr_foreground_new(selected.Red() * 257,
                                                                             selected.Green() * 257,
                                                                             selected.Blue() * 257);
            attr->start_index = static_cast<guint>(byte(a));
            attr->end_index = static_cast<guint>(byte(b));
            pango_attr_list_insert(attrs, attr);
        }
        pango_layout_set_attributes(layout_.get(), attrs);
        pango_attr_list_unref(attrs);
        auto* cr = static_cast<cairo_t*>(gc.GetNativeContext());
        cairo_save(cr);
        cairo_set_source_rgba(cr, fg.Red() / 255.0, fg.Green() / 255.0, fg.Blue() / 255.0,
                              fg.Alpha() / 255.0);
        cairo_move_to(cr, x, y);
        pango_cairo_show_layout(cr, layout_.get());
        cairo_restore(cr);
    }

private:
    int byte(std::size_t chr) const { return byte_of_[std::min(chr, byte_of_.size() - 1)]; }
    std::size_t chr(int byte_index) const {
        return static_cast<std::size_t>(
                   std::upper_bound(byte_of_.begin(), byte_of_.end(), byte_index) -
                   byte_of_.begin()) -
               1;
    }
    std::unique_ptr<PangoLayout, Unref> layout_;
    std::string utf8_;
    std::vector<int> byte_of_;  ///< byte index of character i; size + 1 entries
};

/// 0 a word character (letter, digit, mark, underscore, any script), 1 white space, 2 other.
int word_class(wxUniChar ch) {
    gunichar const c = ch.GetValue();
    if (g_unichar_isalnum(c) || g_unichar_ismark(c) || c == '_')
        return 0;
    return g_unichar_isspace(c) ? 1 : 2;
}

#else  // portable layout: wraps at spaces, logical-order extents

class TextLayout {
public:
    TextLayout(wxWindow const*, wxFont const& font, wxString const& text, bool rtl, int width)
        : font_(font), width_(width), rtl_(rtl) {
        wxBitmap bitmap(1, 1);
        wxMemoryDC dc(bitmap);
        dc.SetFont(font);
        row_h_ = dc.GetCharHeight();
        std::size_t start = 0;
        while (start < text.length() || rows_.empty()) {
            wxString rest = text.Mid(start);
            wxArrayInt partial;
            dc.GetPartialTextExtents(rest, partial);
            std::size_t fit = 0;
            while (fit < rest.length() && partial[fit] <= width)
                ++fit;
            std::size_t cut = rest.length();
            if (fit < rest.length()) {
                cut = std::max<std::size_t>(fit, 1);
                std::size_t const space = rest.Left(fit + 1).find_last_of(" ");
                if (space != wxString::npos && space > 0)
                    cut = space + 1;
            }
            Row row {start, rest.Left(cut), {0.0}};
            wxArrayInt ex;
            dc.GetPartialTextExtents(row.text, ex);
            for (int v : ex)
                row.edge.push_back(v);
            rows_.push_back(std::move(row));
            start += cut;
        }
    }
    int rows() const { return static_cast<int>(rows_.size()); }
    double height() const { return rows() * row_h_; }
    std::size_t hit(double x, double y, int) const {
        Row const& row =
            rows_[std::min(rows_.size() - 1, static_cast<std::size_t>(std::max(0.0, y) / row_h_))];
        double const rel = x - origin(row);
        std::size_t i = 0;
        while (i + 1 < row.edge.size() && rel >= (row.edge[i] + row.edge[i + 1]) / 2)
            ++i;
        return row.start + i;
    }
    wxPoint2DDouble caret(std::size_t offset) const {
        for (std::size_t r = 0; r < rows_.size(); ++r) {
            Row const& row = rows_[r];
            if (offset < row.start + row.text.length() || r + 1 == rows_.size())
                return {origin(row) + row.edge[std::min(offset - row.start, row.edge.size() - 1)],
                        (r + 0.5) * row_h_};
        }
        return {};
    }
    std::vector<Box> selection(std::size_t a, std::size_t b) const {
        std::vector<Box> out;
        for (std::size_t r = 0; r < rows_.size(); ++r) {
            Row const& row = rows_[r];
            std::size_t const s =
                std::clamp(a, row.start, row.start + row.text.length()) - row.start;
            std::size_t const e =
                std::clamp(b, row.start, row.start + row.text.length()) - row.start;
            if (s < e)
                out.push_back(
                    {origin(row) + row.edge[s], r * row_h_, row.edge[e] - row.edge[s], row_h_});
        }
        return out;
    }
    void paint(wxGraphicsContext& gc, double x, double y, wxColour const& fg, std::size_t a,
               std::size_t b, wxColour const& selected, bool skip_selected) const {
        for (std::size_t r = 0; r < rows_.size(); ++r) {
            Row const& row = rows_[r];
            std::size_t const s =
                std::clamp(a, row.start, row.start + row.text.length()) - row.start;
            std::size_t const e =
                std::clamp(b, row.start, row.start + row.text.length()) - row.start;
            double const top = y + r * row_h_, left = x + origin(row);
            auto piece = [&](std::size_t from, std::size_t to, wxColour const& colour) {
                if (from >= to)
                    return;
                gc.SetFont(font_, colour);
                gc.DrawText(row.text.Mid(from, to - from), left + row.edge[from], top);
            };
            piece(0, s, fg);
            if (!skip_selected)
                piece(s, e, selected);
            piece(e, row.text.length(), fg);
        }
    }

private:
    struct Row {
        std::size_t start;
        wxString text;
        std::vector<double> edge;  ///< x of each character boundary from the row's first character
    };
    /// A right-to-left line is right-aligned; the spaces that end a row do not count.
    double origin(Row const& row) const {
        if (!rtl_)
            return 0;
        std::size_t n = row.text.length();
        while (n > 0 && row.text[n - 1] == ' ')
            --n;
        return width_ - row.edge[n];
    }
    wxFont font_;
    int width_;
    bool rtl_;
    double row_h_ = 16;
    std::vector<Row> rows_;
};

/// 0 a word character (a letter or digit of any script, a combining mark, underscore), 1 white
/// space, 2 other. Outside ASCII this treats everything as a letter except the common punctuation
/// blocks.
int word_class(wxUniChar ch) {
    unsigned const c = ch.GetValue();
    if (c == '_')
        return 0;
    if (c < 0x80)
        return std::isalnum(static_cast<int>(c)) ? 0 : (std::isspace(static_cast<int>(c)) ? 1 : 2);
    if (c == 0xA0 || (c >= 0x2000 && c <= 0x200A) || c == 0x3000)
        return 1;
    bool const punctuation = (c >= 0xA1 && c <= 0xBF) || c == 0xD7 || c == 0xF7 ||
                             (c >= 0x2010 && c <= 0x206F) || (c >= 0x3001 && c <= 0x3003) ||
                             c == 0x060C || c == 0x061B || c == 0x061F ||
                             (c >= 0x066A && c <= 0x066D) || c == 0x06D4;
    return punctuation ? 2 : 0;
}

#endif

}  // namespace

MessageLog::MessageLog(wxWindow* parent, wxWindowID id) : timer_(this) {
    wxPanel::Create(parent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetLayoutDirection(wxLayout_LeftToRight);  // the lines carry the direction
    font_ = pixel_font(wxFont(wxFontInfo().Family(wxFONTFAMILY_TELETYPE)), 14);
    SetMinSize(wxSize(40, 40));
    {
        TextLayout const sample(this, font_, "Ag", false, 1000);
        row_h_ = std::max(1.0, sample.height());
    }
    Bind(wxEVT_PAINT, &MessageLog::on_paint, this);
    Bind(wxEVT_SIZE, &MessageLog::on_size, this);
    Bind(wxEVT_TIMER, &MessageLog::on_tick, this);
    Bind(wxEVT_MOUSEWHEEL, &MessageLog::on_wheel, this);
    Bind(wxEVT_KEY_DOWN, &MessageLog::on_key, this);
    Bind(wxEVT_LEFT_DOWN, &MessageLog::on_down, this);
    Bind(wxEVT_LEFT_UP, &MessageLog::on_up, this);
    Bind(wxEVT_LEFT_DCLICK, &MessageLog::on_double_click, this);
    Bind(wxEVT_MOTION, &MessageLog::on_move, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, [this](wxMouseCaptureLostEvent&) { drag_ = Drag::none; });
    Bind(wxEVT_CONTEXT_MENU, &MessageLog::on_context_menu, this);
    Bind(EVT_LANGUAGE_CHANGED, [this](wxCommandEvent& e) {
        rerender();
        e.Skip();  // other handlers on the window still run
    });            // as the Qt log on LanguageChange
}

wxSize MessageLog::DoGetBestSize() const {
    return wxSize(240, 120);
}

void MessageLog::set_frame(double width, double radius) {
    frame_width_ = width;
    frame_radius_ = radius;
    wrapped_width_ = -1;
    Refresh();
}

int MessageLog::text_width() const {
    return std::max(10, GetClientSize().x - 2 * pad_x - static_cast<int>(frame_width_) * 2 -
                            static_cast<int>(bar_width + 2 * bar_margin));
}

double MessageLog::content_height() const {
    double height = 0;
    for (Line const& line : lines_)
        height += line.height;
    return height;
}

double MessageLog::scroll_range() const {
    double const view = GetClientSize().y - 2 * pad_y - 2 * frame_width_;
    return std::max(0.0, content_height() - view);
}

void MessageLog::render_line(Line& line) const {
    line.text = line.time + "  " + wx(line.message.render());
    line.direction = text_direction() == TextDirection::right_to_left ? wxLayout_RightToLeft
                                                                      : wxLayout_LeftToRight;
}

void MessageLog::wrap(Line& line) const {
    TextLayout const layout(this, font_, line.text, line.direction == wxLayout_RightToLeft,
                            text_width());
    line.rows = std::max(1, layout.rows());
    line.height = layout.height();
}

void MessageLog::wrap_all() {
    wrapped_width_ = text_width();
    for (Line& line : lines_)
        wrap(line);
}

void MessageLog::add(Message const& message) {
    if (wrapped_width_ != text_width())
        wrap_all();
    double const from = scroll_;
    Line line {wxDateTime::Now().Format("%H:%M:%S"), message, {}, wxLayout_LeftToRight, 1, 0};
    render_line(line);
    wrap(line);
    lines_.push_back(std::move(line));
    if (static_cast<int>(lines_.size()) > max_lines) {
        lines_.pop_front();  // the selection moves up with the lines
        for (Pos* p : {&anchor_, &caret_}) {
            if (p->line == 0)
                p->offset = 0;
            else
                --p->line;
        }
    }
    // from where the log was to the new bottom, in pixels
    timer_.Stop();
    scroll_ = std::min(from, scroll_range());
    glide_from_ = scroll_;
    glide_to_ = scroll_range();
    glide_start_ = std::chrono::steady_clock::now();
    timer_.Start(tick_ms);
    Refresh();
}

void MessageLog::rerender() {
    timer_.Stop();
    clear_selection();  // the text changes under it
    for (Line& line : lines_)
        render_line(line);
    wrap_all();
    scroll_ = scroll_range();  // at once, no glide
    Refresh();
}

void MessageLog::scroll_to(double position) {
    scroll_ = std::clamp(position, 0.0, scroll_range());
    Refresh();
}

void MessageLog::on_tick(wxTimerEvent&) {
    double const t =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - glide_start_)
            .count() /
        glide_ms;
    glide_to_ = scroll_range();
    if (t >= 1) {
        timer_.Stop();
        scroll_to(glide_to_);
        return;
    }
    double const eased = 1 - std::pow(1 - t, 3);  // out-cubic
    scroll_to(glide_from_ + (glide_to_ - glide_from_) * eased);
}

void MessageLog::on_wheel(wxMouseEvent& event) {
    if (event.GetWheelAxis() != wxMOUSE_WHEEL_VERTICAL || event.GetWheelDelta() == 0) {
        event.Skip();
        return;
    }
    timer_.Stop();
    scroll_to(scroll_ - event.GetWheelRotation() / static_cast<double>(event.GetWheelDelta()) * 3 *
                            row_height());
}

void MessageLog::on_key(wxKeyEvent& event) {
    if (event.ControlDown() && event.GetKeyCode() == 'C') {
        copy();
        return;
    }
    if (event.ControlDown() && event.GetKeyCode() == 'A') {
        select_all();
        return;
    }
    event.Skip();
}

void MessageLog::on_size(wxSizeEvent& event) {
    wrap_all();
    scroll_ = std::min(scroll_, scroll_range());
    Refresh();
    event.Skip();
}

wxString MessageLog::line_text(std::size_t i) const {
    return i < lines_.size() ? lines_[i].text : wxString();
}

wxLayoutDirection MessageLog::line_direction(std::size_t i) const {
    return i < lines_.size() ? lines_[i].direction : wxLayout_LeftToRight;
}

wxString MessageLog::text() const {
    wxString all;
    for (Line const& line : lines_) {
        if (!all.empty())
            all += "\n";
        all += line.text;
    }
    return all;
}

wxString MessageLog::last_message() const {
    if (lines_.empty())
        return {};
    wxString text = wx(lines_.back().message.render());
    text.Replace(wxString(wxUniChar(0x2066)), "");
    text.Replace(wxString(wxUniChar(0x2069)), "");
    return text;
}

void MessageLog::draw(wxGraphicsContext& gc, wxSize size) const {
    gc.SetAntialiasMode(wxANTIALIAS_DEFAULT);
    // the surround, so the rounded corners show the parent's colour
    gc.SetPen(*wxTRANSPARENT_PEN);
    gc.SetBrush(wxBrush(GetParent() ? GetParent()->GetBackgroundColour() : GetBackgroundColour()));
    gc.DrawRectangle(0, 0, size.x, size.y);

    // face with its frame, inset by half the line so the line stays inside the window
    double const fw = frame_width_;
    gc.SetPen(gc.CreatePen(wxGraphicsPenInfo(frame_colour).Width(fw)));
    gc.SetBrush(wxBrush(face_colour));
    gc.DrawRoundedRectangle(fw / 2, fw / 2, size.x - fw, size.y - fw, frame_radius_);

    double const left = fw + pad_x, top = fw + pad_y;
    double const view_h = size.y - 2 * fw - 2 * pad_y;

    gc.PushState();
    gc.Clip(fw, fw + pad_y / 2.0, size.x - 2 * fw, size.y - 2 * fw - pad_y);
    Pos const from = std::min(anchor_, caret_), to = std::max(anchor_, caret_);
    wxColour const glow = with_alpha(lit_colour, glow_alpha * 0.16);
    double y = top - scroll_;
    for (std::size_t li = 0; li < lines_.size(); ++li) {
        Line const& line = lines_[li];
        if (y + line.height < top - pad_y || y > top + view_h + pad_y) {
            y += line.height;
            continue;
        }
        TextLayout const layout(this, font_, line.text, line.direction == wxLayout_RightToLeft,
                                text_width());
        // the selected characters of this line
        std::size_t sel_a = 0, sel_b = 0;
        if (li >= from.line && li <= to.line && from != to) {
            sel_a = li == from.line ? from.offset : 0;
            sel_b = li == to.line ? to.offset : line.text.length();
        }
        // the highlight, where the layout draws those characters
        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.SetBrush(wxBrush(selection_colour));
        for (Box const& box : layout.selection(sel_a, sel_b))
            gc.DrawRectangle(left + box.x, y + box.y, box.w, box.h);
        // the glow: the text again, shifted around itself in two rings, faint (not over the
        // selection); then the text in the lit green, the selected part in the selection's colour
        for (double radius : {glow_blur / 4, glow_blur / 2}) {
            for (int k = 0; k < 8; ++k) {
                double const ang = k * std::numbers::pi / 4;
                layout.paint(gc, left + radius * std::cos(ang), y + radius * std::sin(ang), glow,
                             sel_a, sel_b, selection_text_colour, true);
            }
        }
        layout.paint(gc, left, y, lit_colour, sel_a, sel_b, selection_text_colour, false);
        y += line.height;
    }
    gc.PopState();

    // the scroll bar's handle, only when the lines do not all fit
    if (scroll_range() > 0) {
        wxRect const handle = handle_rect();
        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.SetBrush(wxBrush(frame_colour));
        gc.DrawRoundedRectangle(handle.x, handle.y, handle.width, handle.height, 3);
    }
}

void MessageLog::on_paint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    std::unique_ptr<wxGraphicsContext> gc = paint_context(dc, *this);
    if (gc)
        draw(*gc, GetClientSize());
}

wxBitmap MessageLog::render(double scale) const {
    wxSize const size = GetClientSize();
    wxBitmap bitmap;
    bitmap.CreateWithDIPSize(size, scale);
    wxMemoryDC dc(bitmap);
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (gc)
        draw(*gc, size);
    gc.reset();
    dc.SelectObject(wxNullBitmap);
    return bitmap;
}

// ---- selection, copy and the scroll bar ----

MessageLog::Pos MessageLog::position_at(wxPoint point) const {
    if (lines_.empty())
        return {};
    double y = point.y - (frame_width_ + pad_y) + scroll_;  // from the top of the first line
    for (std::size_t li = 0; li < lines_.size(); ++li) {
        Line const& line = lines_[li];
        if (y >= line.height && li + 1 < lines_.size()) {
            y -= line.height;
            continue;
        }
        TextLayout const layout(this, font_, line.text, line.direction == wxLayout_RightToLeft,
                                text_width());
        return {li, layout.hit(point.x - (frame_width_ + pad_x),
                               std::clamp(y, 0.0, line.height - 1), text_width())};
    }
    return {};
}

wxPoint MessageLog::position_of(std::size_t line, std::size_t offset) const {
    if (line >= lines_.size())
        return {};
    double y = frame_width_ + pad_y - scroll_;
    for (std::size_t li = 0; li < line; ++li)
        y += lines_[li].height;
    TextLayout const layout(this, font_, lines_[line].text,
                            lines_[line].direction == wxLayout_RightToLeft, text_width());
    wxPoint2DDouble const caret = layout.caret(offset);
    return wxPoint(static_cast<int>(std::lround(frame_width_ + pad_x + caret.m_x)),
                   static_cast<int>(std::lround(y + caret.m_y)));
}

wxString MessageLog::selected_text() const {
    if (!has_selection())
        return {};
    Pos const from = std::min(anchor_, caret_), to = std::max(anchor_, caret_);
    wxString out;
    for (std::size_t li = from.line; li <= to.line && li < lines_.size(); ++li) {
        wxString const& t = lines_[li].text;
        std::size_t const s = li == from.line ? from.offset : 0;
        std::size_t const e = li == to.line ? to.offset : t.length();
        if (li > from.line)
            out += "\n";
        out += t.Mid(s, e - s);
    }
    return out;
}

void MessageLog::select_all() {
    if (lines_.empty())
        return;
    anchor_ = {0, 0};
    caret_ = {lines_.size() - 1, lines_.back().text.length()};
    Refresh();
}

void MessageLog::clear_selection() {
    anchor_ = caret_ = {};
    Refresh();
}

bool MessageLog::copy() const {
    wxString const text = selected_text();
    if (text.empty() || !wxTheClipboard->Open())
        return false;
    bool const ok = wxTheClipboard->SetData(new wxTextDataObject(text));
    wxTheClipboard->Close();
    return ok;
}

wxRect MessageLog::handle_rect() const {
    double const range = scroll_range();
    if (range <= 0)
        return {};
    wxSize const size = GetClientSize();
    double const view_h = size.y - 2 * frame_width_ - 2 * pad_y;
    double const track = size.y - 2 * frame_radius_;
    double const handle = std::max(16.0, track * view_h / (view_h + range));
    double const hy = frame_radius_ + (track - handle) * (scroll_ / range);
    return wxRect(static_cast<int>(std::lround(size.x - frame_width_ - bar_margin - bar_width)),
                  static_cast<int>(std::lround(hy)), static_cast<int>(bar_width),
                  static_cast<int>(std::lround(handle)));
}

void MessageLog::on_down(wxMouseEvent& event) {
    SetFocus();
    wxPoint const point = event.GetPosition();
    wxRect const handle = handle_rect();
    double const bar_left = GetClientSize().x - frame_width_ - bar_margin - bar_width - bar_margin;
    if (scroll_range() > 0 && point.x >= bar_left) {
        timer_.Stop();
        if (handle.Contains(point)) {
            drag_ = Drag::handle;
            handle_grab_ = point.y;
            handle_scroll_ = scroll_;
        } else {
            // a click on the track: a page up or down
            double const page = GetClientSize().y - 2 * frame_width_ - 2 * pad_y;
            scroll_to(scroll_ + (point.y < handle.y ? -page : page));
            return;
        }
    } else {
        drag_ = Drag::text;
        anchor_ = caret_ = position_at(point);
        Refresh();
    }
    if (IsShownOnScreen() && !HasCapture())
        CaptureMouse();  // a window not on screen cannot grab
}

void MessageLog::on_move(wxMouseEvent& event) {
    if (drag_ == Drag::handle) {
        wxSize const size = GetClientSize();
        double const track = size.y - 2 * frame_radius_;
        double const room = track - handle_rect().height;
        if (room > 0)
            scroll_to(handle_scroll_ +
                      (event.GetPosition().y - handle_grab_) * scroll_range() / room);
    } else if (drag_ == Drag::text) {
        int const above = frame_width_ + pad_y - event.GetPosition().y;
        int const below = event.GetPosition().y - (GetClientSize().y - frame_width_ - pad_y);
        if (above > 0)
            scroll_to(scroll_ - row_height());  // the text follows a drag past the edge
        else if (below > 0)
            scroll_to(scroll_ + row_height());
        caret_ = position_at(event.GetPosition());
        Refresh();
    }
    event.Skip();
}

void MessageLog::on_up(wxMouseEvent& event) {
    drag_ = Drag::none;
    if (HasCapture())
        ReleaseMouse();
    event.Skip();
}

void MessageLog::on_double_click(wxMouseEvent& event) {
    Pos const at = position_at(event.GetPosition());
    if (at.line >= lines_.size())
        return;
    wxString const& t = lines_[at.line].text;
    if (t.empty())
        return;
    // the character the click is on: the one after the caret, or the last at the end of the line
    std::size_t const i = std::min(at.offset, t.length() - 1);
    int const cls = word_class(t[i]);
    std::size_t s = i, e = i + 1;
    while (s > 0 && word_class(t[s - 1]) == cls)
        --s;
    while (e < t.length() && word_class(t[e]) == cls)
        ++e;
    anchor_ = {at.line, s};
    caret_ = {at.line, e};
    Refresh();
}

void MessageLog::on_context_menu(wxContextMenuEvent& event) {
    wxMenu menu;
    menu.Append(wxID_COPY)->Enable(has_selection());
    menu.Append(wxID_SELECTALL)->Enable(!lines_.empty());
    menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) { copy(); }, wxID_COPY);
    menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) { select_all(); }, wxID_SELECTALL);
    wxPoint point = event.GetPosition();
    point = point == wxDefaultPosition ? wxPoint(10, 10) : ScreenToClient(point);
    PopupMenu(&menu, point);
}
