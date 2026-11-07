#include "ui/WxTestApp.h"

#include <wx/timer.h>
#include <wx/utils.h>
#include <wx/window.h>

#include <algorithm>
#include <utility>

#ifdef INDICATOR_GTK3
#include <gtk/gtk.h>
#endif

bool pump_until(std::function<bool()> done, std::chrono::milliseconds timeout) {
    auto const end = std::chrono::steady_clock::now() + timeout;
    while (!done()) {
        if (std::chrono::steady_clock::now() >= end) return false;
        wxTheApp->Yield(true);
        wxMilliSleep(5);
    }
    return true;
}

wxImage on_screen(wxWindow& window) {
    window.Refresh();
    window.Update();
#ifdef INDICATOR_GTK3
    GtkWidget* const top = wxGetTopLevelParent(&window)->GetHandle();
    gint x = 0, y = 0;
    if (!gtk_widget_translate_coordinates(window.GetHandle(), top, 0, 0, &x, &y)) return {};
    GdkWindow* const gdk = gtk_widget_get_window(top);
    wxSize const size = window.GetSize();
    GdkPixbuf* const pixbuf = gdk ? gdk_pixbuf_get_from_window(gdk, x, y, size.x, size.y) : nullptr;
    if (!pixbuf) return {};
    int const w = gdk_pixbuf_get_width(pixbuf), h = gdk_pixbuf_get_height(pixbuf);
    int const channels = gdk_pixbuf_get_n_channels(pixbuf), stride = gdk_pixbuf_get_rowstride(pixbuf);
    guchar const* const pixels = gdk_pixbuf_get_pixels(pixbuf);
    wxImage image(w, h, false);
    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            guchar const* p = pixels + row * stride + col * channels;
            image.SetRGB(col, row, p[0], p[1], p[2]);
        }
    }
    g_object_unref(pixbuf);
    return image;
#else
    return {};
#endif
}

Ink ink_of(wxImage const& image, wxRect area) {
    auto green = [&](int x, int y) { return image.GetGreen(x, y) - image.GetRed(x, y) > 60; };
    int left = area.GetRight() + 1, right = area.x - 1, top = area.GetBottom() + 1, bottom = area.y - 1;
    for (int y = area.y; y <= area.GetBottom(); ++y) {
        for (int x = area.x; x <= area.GetRight(); ++x) {
            if (!green(x, y)) continue;
            left = std::min(left, x);
            right = std::max(right, x);
            top = std::min(top, y);
            bottom = std::max(bottom, y);
        }
    }
    Ink ink;
    if (right < left) return ink;
    ink.w = right - left + 1;
    ink.h = bottom - top + 1;
    ink.on.resize(static_cast<std::size_t>(ink.w * ink.h));
    for (int y = 0; y < ink.h; ++y) {
        for (int x = 0; x < ink.w; ++x) {
            bool const on = green(left + x, top + y);
            ink.on[static_cast<std::size_t>(y * ink.w + x)] = on;
            ink.count += on;
        }
    }
    return ink;
}

/// The share of `a`'s and `b`'s ink pixels that are not in both (0 for the same drawing).
double ink_difference(Ink const& a, Ink const& b) {
    int differ = 0, total = 0;
    for (int y = 0; y < std::max(a.h, b.h); ++y) {
        for (int x = 0; x < std::max(a.w, b.w); ++x) {
            bool const in_a = a.at(x, y), in_b = b.at(x, y);
            differ += in_a != in_b;
            total += in_a || in_b;
        }
    }
    return total ? double(differ) / total : 1.0;
}

Ink ink_on_screen(wxWindow& window, wxRect area) {
    Ink ink;
    pump_until([&] {
        wxImage const shown = on_screen(window);
        if (shown.GetSize() == window.GetSize()) {
            ink = ink_of(shown, area);
            ink.read = true;
        }
        return ink.count > 0;
    }, std::chrono::seconds(3));
    return ink;
}

namespace {

/// Polls for a modal dialog; deletes itself when it has acted or timed out.
class ModalDriver : public wxEvtHandler {
public:
    ModalDriver(std::function<void(wxDialog&)> action, std::chrono::milliseconds timeout, wxString name)
        : action_(std::move(action)), end_(std::chrono::steady_clock::now() + timeout), name_(std::move(name)), timer_(this) {
        Bind(wxEVT_TIMER, &ModalDriver::poll, this);
        timer_.Start(50);
    }

private:
    wxDialog* open_modal() const {
        for (wxWindow* window : wxTopLevelWindows) {
            auto* dialog = dynamic_cast<wxDialog*>(window);
            if (dialog && dialog->IsModal() && (name_.empty() || dialog->GetName() == name_)) return dialog;
        }
        return nullptr;
    }
    void poll(wxTimerEvent&) {
        wxDialog* dialog = open_modal();
        bool const late = std::chrono::steady_clock::now() >= end_;
        if (!dialog && !late) return;
        timer_.Stop();
        if (dialog && late) dialog->EndModal(wxID_CANCEL);
        else if (dialog) action_(*dialog);
        wxTheApp->CallAfter([this] { delete this; });
    }
    std::function<void(wxDialog&)> action_;
    std::chrono::steady_clock::time_point end_;
    wxString name_;
    wxTimer timer_;
};

}  // namespace

void drive_modal(std::function<void(wxDialog&)> action, std::chrono::milliseconds timeout, wxString const& name) {
    new ModalDriver(std::move(action), timeout, name);
}
