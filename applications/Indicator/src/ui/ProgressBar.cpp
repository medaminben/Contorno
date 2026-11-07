#include "ui/ProgressBar.h"

#include "ui/Paint.h"

#include <wx/dcbuffer.h>
#include <wx/dcmemory.h>
#include <wx/graphics.h>

#include <algorithm>
#include <cmath>
#include <memory>

namespace {

// the Qt bar's stylesheet: the readout's face and frame, the chunk's three greens
wxColour const face(0x05, 0x08, 0x05);
wxColour const frame(0x1B, 0x2A, 0x1F);
double const frame_width = 2;
double const radius = 6;
double const chunk_margin = 1;
double const chunk_radius = 4;
int const content_height = 14;
double const sweep_ms = 1600;  ///< one pass of the busy chunk
int const tick_ms = 30;

wxColour dimmed(int r, int g, int b, double level) {
    auto const scaled = [level](int v) { return static_cast<unsigned char>(std::lround(v * level)); };
    return wxColour(scaled(r), scaled(g), scaled(b));
}

}  // namespace

ProgressBar::ProgressBar(wxWindow* parent, wxWindowID id) : timer_(this) {
    wxWindow::Create(parent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetMinSize(wxSize(40, content_height + 2 * static_cast<int>(frame_width)));
    Bind(wxEVT_PAINT, &ProgressBar::on_paint, this);
    Bind(wxEVT_TIMER, &ProgressBar::on_tick, this);
}

wxSize ProgressBar::DoGetBestSize() const { return wxSize(200, content_height + 2 * static_cast<int>(frame_width)); }

void ProgressBar::set_running(bool running) {
    state_ = running ? State::running : State::stopped;
    since_ = std::chrono::steady_clock::now();
    timer_.Start(tick_ms);
    Refresh();
}

double ProgressBar::elapsed_ms() const {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - since_).count();
}

bool ProgressBar::full() const { return state_ == State::stopped && elapsed_ms() < full_ms + fade_ms; }

bool ProgressBar::empty() const { return state_ == State::empty || (state_ == State::stopped && !full()); }

double ProgressBar::level() const {
    if (state_ != State::stopped) return 1;
    double const fading = elapsed_ms() - full_ms;
    if (fading <= 0 || fading >= fade_ms) return 1;  // full, or empty and lit again
    return 1 - fading / fade_ms;
}

void ProgressBar::on_tick(wxTimerEvent&) {
    if (state_ == State::stopped && !full()) {
        state_ = State::empty;
        timer_.Stop();
    }
    Refresh();
}

void ProgressBar::draw(wxGraphicsContext& gc, wxSize size) const {
    gc.SetAntialiasMode(wxANTIALIAS_DEFAULT);
    // the surround, so the rounded corners show the parent's colour
    gc.SetPen(*wxTRANSPARENT_PEN);
    gc.SetBrush(wxBrush(GetParent() ? GetParent()->GetBackgroundColour() : *wxBLACK));
    gc.DrawRectangle(0, 0, size.x, size.y);
    // the face and its frame
    gc.SetPen(wxPen(frame, static_cast<int>(frame_width)));
    gc.SetBrush(wxBrush(face));
    double const half = frame_width / 2;
    gc.DrawRoundedRectangle(half, half, size.x - frame_width, size.y - frame_width, radius - half);
    if (empty()) return;
    // the chunk inside the frame: the whole width when full, a sweeping part while running
    double const inset = frame_width + chunk_margin;
    double const x0 = inset, y0 = inset, w = size.x - 2 * inset, h = size.y - 2 * inset;
    if (w <= 0 || h <= 0) return;
    double x = x0, cw = w;
    if (running()) {
        cw = std::max(30.0, w / 4);
        double const phase = std::fmod(elapsed_ms(), sweep_ms) / sweep_ms;
        x = x0 - cw + (w + cw) * phase;
    }
    double const level = this->level();
    wxGraphicsGradientStops stops(dimmed(0x7d, 0xff, 0xab, level), dimmed(0x2b, 0xc4, 0x63, level));
    stops.Add(dimmed(0x47, 0xff, 0x86, level), 0.5);
    gc.Clip(x0, y0, w, h);
    gc.SetPen(*wxTRANSPARENT_PEN);
    gc.SetBrush(gc.CreateLinearGradientBrush(0, y0, 0, y0 + h, stops));
    gc.DrawRoundedRectangle(x, y0, cw, h, chunk_radius);
    gc.ResetClip();
}

void ProgressBar::on_paint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    std::unique_ptr<wxGraphicsContext> gc = paint_context(dc, *this);
    if (gc) draw(*gc, GetClientSize());
}

wxBitmap ProgressBar::render(double scale) const {
    wxSize const size = GetClientSize();
    wxBitmap bitmap;
    bitmap.CreateWithDIPSize(size, scale);
    wxMemoryDC dc(bitmap);
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (gc) draw(*gc, size);
    gc.reset();
    dc.SelectObject(wxNullBitmap);
    return bitmap;
}
