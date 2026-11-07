#ifndef INDICATOR_SEGMENTDISPLAY_H
#define INDICATOR_SEGMENTDISPLAY_H
#include <wx/bitmap.h>
#include <wx/font.h>
#include <wx/string.h>
#include <wx/timer.h>
#include <wx/window.h>

#include <chrono>

/// A green seven-segment readout of the surface area: lit segments glow,
/// unlit ones stay faintly visible, like a classic LED display. Painted by
/// hand (no font), five digit cells that never change width, the segments
/// scaling with the window's height. The caption ("SURFACE AREA") sits top-left
/// (top-right, right to left, in Arabic), the unit to the right of the digits.
/// A new value is counted to from the one shown (up or down, about 0.7 s),
/// like a meter settling.
class SegmentDisplay : public wxWindow {
public:
    /// Number of digit cells; a decimal point is a segment of its cell, not a cell.
    static constexpr int cells = 5;

    explicit SegmentDisplay(wxWindow* parent, wxWindowID id = wxID_ANY);

    /// Shows `digits` (characters `[0-9.]`, at most one point) right-aligned in
    /// the cells, leading cells blank, followed by `unit`. A leading point is
    /// read as "0." (".5" shows and reports "0.5"). A value that does not fit
    /// the cells (more than `cells` digits, no digit, another character) shows
    /// dashes instead, since a wrong area is worse than none: `text()` is then
    /// "-----" and `unit()` empty. The display is not dimmed afterwards.
    /// The cells count to the value from the one shown (from 0 after dashes;
    /// cm² and m² converted), in the new value's decimals; `text()` is the
    /// new value at once.
    void set_value(wxString const& digits, wxString const& unit);
    /// Every cell shows segment g, no unit.
    void set_dashes();
    /// Lit segments at about 40 % brightness (a stale value while analysing).
    void set_dimmed(bool dimmed);

    /// "1.842", or "-----" for dashes: the value set, even while counting to it.
    wxString text() const { return text_; }
    wxString unit() const { return unit_; }
    bool dimmed() const { return dimmed_; }
    /// The text in the cells now (differs from text() while counting).
    wxString shown_text() const { return shown_; }
    /// Counting to the value set.
    bool counting() const { return timer_.IsRunning(); }

    /// The frame's corner radius and line width at the window's size (px),
    /// for widgets beside it that take its look.
    double corner_radius() const;
    double frame_width() const;
    /// The caption's font at `scale`.
    wxFont caption_font(double scale) const;
    /// The caption's extra space between letters at `scale` (px): none in Arabic, which joins its
    /// letters.
    double caption_spacing(double scale) const;
    /// The display as drawn, `scale` device pixels per logical one (the window's own scale for the
    /// screen).
    wxBitmap render(double scale) const;

protected:
    /// About 72 px high, as wide as the cells, the unit and the margins need.
    /// A narrower or lower window scales the whole display down; a wider one
    /// keeps the digits and unit right-aligned.
    wxSize DoGetBestSize() const override;

private:
    void draw(class wxGraphicsContext& gc, wxSize size, double scale) const;
    void on_paint(wxPaintEvent&);
    void on_tick(wxTimerEvent&);
    /// The cells show `value` in the decimals of `text_` (fewer if it does not fit).
    void show_count(double value);

    wxString text_;
    wxString unit_;
    wxString shown_;  ///< in the cells: text_, or a step of the count to it
    int decimals_ = 0;
    bool dimmed_ = false;
    wxTimer timer_;
    std::chrono::steady_clock::time_point count_start_;
    double count_from_ = 0, count_to_ = 0;
};

#endif  // INDICATOR_SEGMENTDISPLAY_H
