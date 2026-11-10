#ifndef INDICATOR_CONTROLPANEL_H
#define INDICATOR_CONTROLPANEL_H
#include <wx/panel.h>
#include <wx/statbmp.h>

#include "ui/Key.h"
#include "ui/MessageLog.h"
#include "ui/PhotoView.h"
#include "ui/SegmentDisplay.h"

class wxBoxSizer;
class wxGridSizer;
class wxSizerItem;

/// The ids of the four keys, taken from wx's pool of control ids so no other id can equal them;
/// each click reaches the panel (and the windows above it) as a wxEVT_BUTTON with its id.
inline wxWindowID const ID_IMPORT = wxWindow::NewControlId();
inline wxWindowID const ID_OPEN = wxWindow::NewControlId();
inline wxWindowID const ID_SAVE = wxWindow::NewControlId();
inline wxWindowID const ID_SETUP = wxWindow::NewControlId();

/// Pixel sizes of the control panel. The numbers below are the 1920×1080 layout.
struct ControlPanelMetrics {
    double scale = 1;
    int height = 140;  ///< the mask box, the readout, two keys
    int wordmark = 110;
    int gap = 6;
    int margin = 9;
    int spacer = 22;
    int log_min_width = 320;
    int key_side = 67;  ///< (height − gap) / 2
    int label_px = 16;
};

/// Scale of the 1920×1080 sizes on a screen of `width`×`height` pixels:
/// min(width/1920, height/1080). That is 1 at 1920×1080 and 768/1080 at 1368×768.
/// Each size is that scale times its 1920×1080 size, rounded to a pixel.
/// The key side stays (height − gap) / 2.
ControlPanelMetrics control_panel_metrics(int width, int height);

/// The metrics for the screen `window` stands on. A window not placed yet uses display 0;
/// no display at all keeps the 1920×1080 sizes.
ControlPanelMetrics control_panel_metrics_for(wxWindow* window);

/// The row under the photos, left to right: the Contorno wordmark, the square
/// keys in two columns (Import over Open, Save over "Set up"), the mask in a
/// square box, the message log and the area readout at the right end. On a
/// 1920×1080 screen the row is 140 px high: the mask box and the readout that
/// high, each key half of it. On another screen every one of those sizes is
/// the same fraction of the screen (see control_panel_metrics). The row never
/// mirrors (it is left to right in every language); only the keys' labels turn
/// right to left in Arabic. The labels and tooltips follow the language, the
/// wordmark (black on light panels, white on dark ones) and the drawn children
/// follow the panel's colour.
class ControlPanel : public wxPanel {
public:
    explicit ControlPanel(wxWindow* parent, wxWindowID id = wxID_ANY);
    ~ControlPanel() override;

    SegmentDisplay& display() { return *display_; }
    /// The AI mask with the object's outline.
    PhotoView& mask() { return *mask_; }
    MessageLog& log() { return *log_; }

    /// Takes the colour, then draws the wordmark in the colour that reads on it and repaints the
    /// drawn children, which show it in their corners.
    bool SetBackgroundColour(wxColour const& colour) override;

    /// Sizes the row with `metrics`, unless they are at the scale already applied. The panel's
    /// own minimum follows, so the window's layout gives it the new height.
    void apply_metrics(ControlPanelMetrics const& metrics);

private:
    /// The labels and tooltips of the keys in the current language, each key in its text direction,
    /// the font as large as lets every label fit.
    void retranslate();
    /// The wordmark at the window's scale, white or black to read on the panel's colour.
    void update_wordmark();
    /// Sizes the row for the screen the window is on, when that scale changed.
    void apply_screen();
    void on_move(wxMoveEvent& event);
    void on_display(wxDisplayChangedEvent& event);

    Key* import_key_;
    Key* open_key_;
    Key* save_key_;
    Key* setup_key_;
    int key_side_ = 0;
    int label_px_ = 16;
    int wordmark_height_ = 110;
    double applied_scale_ = -1;  ///< the scale the sizes were last set at
    double wordmark_scale_ = 0;  ///< the content scale the wordmark was drawn at
    wxWindow* screen_top_ = nullptr;
    wxStaticBitmap* logo_ = nullptr;
    wxBoxSizer* row_ = nullptr;
    wxGridSizer* keys_grid_ = nullptr;
    wxBoxSizer* outer_ = nullptr;
    wxSizerItem* spacers_[4] = {};
    PhotoView* mask_;
    MessageLog* log_;
    SegmentDisplay* display_;
};

#endif  // INDICATOR_CONTROLPANEL_H
