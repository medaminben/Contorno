#ifndef INDICATOR_MESSAGELOG_H
#define INDICATOR_MESSAGELOG_H
#include <wx/bitmap.h>
#include <wx/font.h>
#include <wx/panel.h>
#include <wx/string.h>
#include <wx/timer.h>

#include <chrono>
#include <compare>
#include <cstddef>
#include <deque>
#include <vector>

#include "model/Message.h"

/// The log in the control panel in the area readout's style: the dark face
/// and frame, the text in the lit green with its glow, a fixed-width font.
/// Each message is a new line with its time, wrapped at the width; the newest
/// is at the bottom and glides into view (about 0.3 s), the oldest go after
/// `max_lines`. It keeps the messages, not their text: `rerender()` (after a
/// change of language) renders every line again, each line right to left in
/// Arabic. Painted by hand, as a list of lines, because a text field cannot
/// give each line its own direction. Read-only, like the Qt log: the text can
/// be selected (drag, double-click a word, Ctrl+A, in the order it is written)
/// and copied (Ctrl+C or the context menu; nothing is copied without a
/// selection). The wheel scrolls, the handle drags, a click on the track pages.
class MessageLog : public wxPanel {
public:
    static constexpr int max_lines = 1000;

    explicit MessageLog(wxWindow* parent, wxWindowID id = wxID_ANY);

    /// The frame's line width and corner radius (px), e.g. the readout's
    /// beside it (2 and 6 at first).
    void set_frame(double width, double radius);
    /// Adds "hh:mm:ss  message" at the bottom (the time is now); it glides into view.
    void add(Message const& message);
    /// Renders every line again in the current language, at once (no glide).
    void rerender();

    /// The lines kept, oldest first.
    std::size_t line_count() const { return lines_.size(); }
    /// Line `i` as shown: "hh:mm:ss  message".
    wxString line_text(std::size_t i) const;
    /// The direction line `i` is laid out in: right to left in Arabic.
    wxLayoutDirection line_direction(std::size_t i) const;
    /// All lines (not their wrapped rows), joined by "\n", oldest first.
    wxString text() const;
    /// The last line without its time and without the invisible isolates around plain values ("" before the first).
    wxString last_message() const;
    /// The last message ({} before the first).
    Message last_text() const { return lines_.empty() ? Message() : lines_.back().message; }

    /// How far the lines are scrolled (px) and how far they can be.
    double scroll_position() const { return scroll_; }
    double scroll_range() const;
    /// Gliding to the newest line.
    bool gliding() const { return timer_.IsRunning(); }
    /// The selected text, lines joined by "\n" ("" without a selection).
    wxString selected_text() const;
    void select_all();
    void clear_selection();
    /// Puts the selected text on the clipboard; false without a selection or a clipboard.
    bool copy() const;
    /// The rows line `i` is wrapped into at the current width.
    int row_count(std::size_t i) const { return i < lines_.size() ? lines_[i].rows : 0; }
    /// The height of line `i`'s block (px): its layout's own height, not rows times a nominal height.
    double block_height(std::size_t i) const { return i < lines_.size() ? lines_[i].height : 0; }
    /// The client point of the caret before character `offset` of line `line`, as the layout that
    /// paints places it (right to left in Arabic lines), at the middle of the row's height.
    wxPoint position_of(std::size_t line, std::size_t offset) const;
    /// The scroll bar's handle (client coordinates); empty when everything fits.
    wxRect handle_rect() const;
    bool AcceptsFocusFromKeyboard() const override { return false; }  ///< focus by click only, as the Qt log
    /// The font the lines are laid out in.
    wxFont const& text_font() const { return font_; }
    /// The log as drawn, `scale` device pixels per logical one.
    wxBitmap render(double scale) const;

protected:
    wxSize DoGetBestSize() const override;

private:
    struct Line {
        wxString time;
        Message message;
        wxString text;                 ///< time, two spaces, the message in its language
        wxLayoutDirection direction;   ///< of the language it was rendered in
        int rows = 1;                  ///< wrapped at the current width
        double height = 0;             ///< of the wrapped block (px)
    };
    void render_line(Line& line) const;
    void wrap_all();
    void wrap(Line& line) const;
    int text_width() const;
    double content_height() const;
    double row_height() const { return row_h_; }  ///< a nominal row, for scroll steps
    void scroll_to(double position);
    void on_paint(wxPaintEvent&);
    void on_size(wxSizeEvent&);
    void on_tick(wxTimerEvent&);
    void on_wheel(wxMouseEvent&);
    void on_key(wxKeyEvent&);
    void on_down(wxMouseEvent&);
    void on_up(wxMouseEvent&);
    void on_move(wxMouseEvent&);
    void on_double_click(wxMouseEvent&);
    void on_context_menu(wxContextMenuEvent&);
    /// A place in the text: a line and a character offset in its text.
    struct Pos {
        std::size_t line = 0, offset = 0;
        auto operator<=>(Pos const&) const = default;
    };
    Pos position_at(wxPoint point) const;
    bool has_selection() const { return anchor_ != caret_; }
    void draw(class wxGraphicsContext& gc, wxSize size) const;

    std::deque<Line> lines_;
    wxFont font_;
    double row_h_ = 16;            ///< the height of a row (px), from the font
    double frame_width_ = 2, frame_radius_ = 6;
    double scroll_ = 0;            ///< the offset shown, px
    double glide_from_ = 0, glide_to_ = 0;
    std::chrono::steady_clock::time_point glide_start_;
    int wrapped_width_ = -1;
    Pos anchor_, caret_;           ///< the selection, from where the press was to where the mouse is
    enum class Drag { none, text, handle } drag_ = Drag::none;
    double handle_grab_ = 0, handle_scroll_ = 0;
    wxTimer timer_;
};

#endif  // INDICATOR_MESSAGELOG_H
