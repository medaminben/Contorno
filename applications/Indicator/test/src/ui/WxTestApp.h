#ifndef INDICATOR_TEST_WXTESTAPP_H
#define INDICATOR_TEST_WXTESTAPP_H
#include <wx/app.h>
#include <wx/dialog.h>
#include <wx/image.h>

#include <chrono>
#include <cstddef>
#include <functional>
#include <vector>

/// Runs the wx event loop until `done` returns true or `timeout` has passed;
/// true when `done` was satisfied.
bool pump_until(std::function<bool()> done, std::chrono::milliseconds timeout);

/// Calls `action` with the modal dialog that is open next (polled every 50 ms),
/// from inside the dialog's own event loop. A dialog still open after `timeout`
/// is cancelled so a failing test cannot hang on it; `action` is not called then.
/// With `name`, only the dialog of that wx name counts (to tell nested dialogs apart).
/// The caller then runs the code that opens the dialog.
void drive_modal(std::function<void(wxDialog&)> action,
                 std::chrono::milliseconds timeout = std::chrono::milliseconds(5000),
                 wxString const& name = {});

/// What `window` shows on the screen: it is painted now (Refresh and Update) and
/// read back from its top-level window; invalid where the screen cannot be read.
wxImage on_screen(wxWindow& window);

/// Where an image shows the keys' green text, cropped to the ink's box;
/// `read` is false when there was no image to look at (the screen could not be read).
struct Ink {
    int w = 0, h = 0, count = 0;
    bool read = false;
    std::vector<bool> on;
    bool at(int x, int y) const {
        return x >= 0 && y >= 0 && x < w && y < h && on[static_cast<std::size_t>(y * w + x)];
    }
};

/// The green ink of `image` inside `area`.
Ink ink_of(wxImage const& image, wxRect area);

/// The share of `a`'s and `b`'s ink pixels that are not in both (0 for the same drawing, 1 for no
/// ink).
double ink_difference(Ink const& a, Ink const& b);

/// The green ink `window` shows on the screen inside `area`, once the window is
/// mapped and painted (empty when it is not within three seconds; not `read`
/// where the screen cannot be read, off GTK 3).
Ink ink_on_screen(wxWindow& window, wxRect area);

/// The `T` among `parent`'s descendants, depth first; nullptr when there is none.
template <class T>
T* find_descendant(wxWindow* parent) {
    for (wxWindow* child : parent->GetChildren()) {
        if (auto* found = dynamic_cast<T*>(child))
            return found;
        if (auto* deeper = find_descendant<T>(child))
            return deeper;
    }
    return nullptr;
}

/// Every `T` among `parent`'s descendants.
template <class T>
void collect_descendants(wxWindow* parent, std::vector<T*>& out) {
    for (wxWindow* child : parent->GetChildren()) {
        if (auto* found = dynamic_cast<T*>(child))
            out.push_back(found);
        collect_descendants<T>(child, out);
    }
}

#endif  // INDICATOR_TEST_WXTESTAPP_H
