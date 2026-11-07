#ifndef INDICATOR_PROGRESSBAR_H
#define INDICATOR_PROGRESSBAR_H
#include <wx/bitmap.h>
#include <wx/timer.h>
#include <wx/window.h>

#include <chrono>

/// The busy indicator under the panels in the readout's look: its black face
/// and frame, the bar in its green. Running, a green chunk sweeps across it;
/// stopped, the bar is full for a second, then its green dims to black
/// (0.8 s) and it is empty, lit again for the next run. Painted by hand; the
/// corners show the parent's colour.
class ProgressBar : public wxWindow {
public:
    static constexpr int full_ms = 1000;  ///< full before the fade
    static constexpr int fade_ms = 800;   ///< the green dimming to black

    explicit ProgressBar(wxWindow* parent, wxWindowID id = wxID_ANY);

    /// Busy while running; stopped, full for a second, then the fade.
    void set_running(bool running);
    bool running() const { return state_ == State::running; }
    /// Stopped and not yet emptied (during the fade too).
    bool full() const;
    bool empty() const;
    /// The green's brightness now: 1 lit, 0 black.
    double level() const;
    /// The bar as drawn, `scale` device pixels per logical one.
    wxBitmap render(double scale) const;

protected:
    wxSize DoGetBestSize() const override;

private:
    enum class State { empty, running, stopped };
    /// Milliseconds since the state began.
    double elapsed_ms() const;
    void draw(class wxGraphicsContext& gc, wxSize size) const;
    void on_paint(wxPaintEvent&);
    void on_tick(wxTimerEvent&);

    State state_ = State::empty;
    std::chrono::steady_clock::time_point since_ = std::chrono::steady_clock::now();
    wxTimer timer_;
};

#endif  // INDICATOR_PROGRESSBAR_H
