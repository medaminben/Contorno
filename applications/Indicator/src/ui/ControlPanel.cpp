#include "ui/ControlPanel.h"

#include <wx/bmpbndl.h>
#include <wx/display.h>
#include <wx/sizer.h>

#include <algorithm>
#include <cmath>

#include "model/Language.h"
#include "ui/Fonts.h"
#include "ui/Localise.h"
#include "ui/Text.h"
#include "ui/Theme.h"

// The wordmark, compiled into the executable (see indicator_icon_data.cpp).
extern unsigned char const indicator_wordmark_svg[];

namespace {

/// The control panel as laid out on a 1920×1080 screen.
int const ref_screen_w = 1920;
int const ref_screen_h = 1080;
int const ref_height = 140;
int const ref_wordmark = 110;
int const ref_gap = 6;
int const ref_margin = 9;
int const ref_spacer = 22;
int const ref_log_width = 320;
int const ref_label_px = 16;

int scaled(int reference, double scale) {
    return std::max(1, static_cast<int>(std::lround(reference * scale)));
}

std::string text_of(std::string const& key) {
    return lookup(current_language(), key);
}

/// True when `colour` is closer to black than to white.
bool is_dark(wxColour const& colour) {
    return colour.Red() * 299 + colour.Green() * 587 + colour.Blue() * 114 < 128000;
}

}  // namespace

ControlPanelMetrics control_panel_metrics(int width, int height) {
    ControlPanelMetrics m;
    if (width > 0 && height > 0)
        m.scale = std::min(double(width) / ref_screen_w, double(height) / ref_screen_h);
    m.height = scaled(ref_height, m.scale);
    m.wordmark = scaled(ref_wordmark, m.scale);
    m.gap = scaled(ref_gap, m.scale);
    m.margin = scaled(ref_margin, m.scale);
    m.spacer = scaled(ref_spacer, m.scale);
    m.log_min_width = scaled(ref_log_width, m.scale);
    m.label_px = scaled(ref_label_px, m.scale);
    m.key_side = std::max(1, (m.height - m.gap) / 2);
    return m;
}

ControlPanelMetrics control_panel_metrics_for(wxWindow* window) {
    unsigned count = wxDisplay::GetCount();
    if (count == 0)
        return control_panel_metrics(ref_screen_w, ref_screen_h);
    int index = 0;
    if (window) {
        wxWindow* const top = wxGetTopLevelParent(window);
        int const found = top ? wxDisplay::GetFromWindow(top) : wxNOT_FOUND;
        if (found != wxNOT_FOUND)
            index = found;
    }
    if (index < 0 || static_cast<unsigned>(index) >= count)
        index = 0;
    wxSize const screen = wxDisplay(static_cast<unsigned>(index)).GetGeometry().GetSize();
    return control_panel_metrics(screen.x, screen.y);
}

ControlPanel::ControlPanel(wxWindow* parent, wxWindowID id) {
    wxPanel::Create(parent, id, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxBORDER_NONE);
    wxPanel::SetBackgroundColour(palette(light_theme()).panel);
    SetLayoutDirection(wxLayout_LeftToRight);  // the row never mirrors, whatever the language

    logo_ = new wxStaticBitmap(this, wxID_ANY, wxBitmapBundle());

    display_ = new SegmentDisplay(this);
    display_->set_dashes();

    import_key_ = new Key(this, ID_IMPORT, {});
    open_key_ = new Key(this, ID_OPEN, {});
    save_key_ = new Key(this, ID_SAVE, {});
    setup_key_ = new Key(this, ID_SETUP, {});
    keys_grid_ = new wxGridSizer(2, 2, 0, 0);  // row by row: Import Save / Open Set up
    keys_grid_->Add(import_key_);
    keys_grid_->Add(save_key_);
    keys_grid_->Add(open_key_);
    keys_grid_->Add(setup_key_);

    mask_ = new PhotoView(this);
    mask_->set_backdrop(
        wxColour());  // no box: the shape and its outline on the panel's own background

    log_ = new MessageLog(this);

    row_ = new wxBoxSizer(wxHORIZONTAL);
    row_->Add(logo_, 0, wxALIGN_CENTER_VERTICAL);
    spacers_[0] = row_->AddSpacer(0);
    row_->Add(keys_grid_, 0, wxALIGN_CENTER_VERTICAL);
    spacers_[1] = row_->AddSpacer(0);
    row_->Add(mask_, 0, wxALIGN_CENTER_VERTICAL);
    spacers_[2] = row_->AddSpacer(0);
    row_->Add(log_, 1, wxALIGN_CENTER_VERTICAL);
    spacers_[3] = row_->AddSpacer(0);
    row_->Add(display_, 0, wxALIGN_CENTER_VERTICAL);
    outer_ = new wxBoxSizer(wxVERTICAL);
    outer_->Add(row_, 1, wxEXPAND | wxALL, 0);
    apply_screen();
    log_->set_frame(display_->frame_width(), display_->corner_radius());
    SetSizerAndFit(outer_);

    // the readout, the log and the mask never mirror either (their text carries its own direction)
    for (wxWindow* part : {static_cast<wxWindow*>(display_), static_cast<wxWindow*>(log_),
                           static_cast<wxWindow*>(mask_)}) {
        part->Bind(EVT_LANGUAGE_CHANGED, [part](wxCommandEvent& event) {
            part->SetLayoutDirection(wxLayout_LeftToRight);
            event.Skip();
        });
    }
    Bind(EVT_LANGUAGE_CHANGED, [this](wxCommandEvent& event) {
        SetLayoutDirection(
            wxLayout_LeftToRight);  // apply_language turned this window with the others
        retranslate();
        Layout();
        event.Skip();
    });
    Bind(wxEVT_DPI_CHANGED, [this](wxDPIChangedEvent& event) {
        apply_screen();
        update_wordmark();
        event.Skip();
    });
    // the scale of a window that is not on screen yet may not be its final one
    Bind(wxEVT_SHOW, [this](wxShowEvent& event) {
        if (event.IsShown())
            apply_screen();
        if (event.IsShown() && GetContentScaleFactor() != wordmark_scale_)
            update_wordmark();
        event.Skip();
    });
    Bind(wxEVT_SIZE, [this](wxSizeEvent& event) {
        log_->set_frame(display_->frame_width(), display_->corner_radius());
        event.Skip();
    });
    screen_top_ = wxGetTopLevelParent(this);
    if (screen_top_ && screen_top_ != this) {
        screen_top_->Bind(wxEVT_MOVE, &ControlPanel::on_move, this);
        screen_top_->Bind(wxEVT_DISPLAY_CHANGED, &ControlPanel::on_display, this);
    } else {
        screen_top_ = nullptr;
    }
}

ControlPanel::~ControlPanel() {
    if (!screen_top_)
        return;
    screen_top_->Unbind(wxEVT_MOVE, &ControlPanel::on_move, this);
    screen_top_->Unbind(wxEVT_DISPLAY_CHANGED, &ControlPanel::on_display, this);
}

void ControlPanel::on_move(wxMoveEvent& event) {
    apply_screen();
    event.Skip();
}

void ControlPanel::on_display(wxDisplayChangedEvent& event) {
    apply_screen();
    event.Skip();
}

void ControlPanel::apply_screen() {
    apply_metrics(control_panel_metrics_for(this));
}

void ControlPanel::apply_metrics(ControlPanelMetrics const& metrics) {
    if (metrics.scale == applied_scale_)
        return;
    applied_scale_ = metrics.scale;
    key_side_ = metrics.key_side;
    label_px_ = metrics.label_px;
    wordmark_height_ = metrics.wordmark;

    wxSize const hint = display_->GetBestSize();
    int const readout_w = hint.y > 0 ? hint.x * metrics.height / hint.y : metrics.height;
    wxSize const readout(readout_w, metrics.height);
    display_->SetMinSize(readout);
    display_->SetMaxSize(readout);
    display_->SetSize(readout);

    wxSize const key(metrics.key_side, metrics.key_side);
    for (Key* one : {import_key_, open_key_, save_key_, setup_key_}) {
        one->SetMinSize(key);
        one->SetMaxSize(key);
    }
    keys_grid_->SetHGap(metrics.gap);
    keys_grid_->SetVGap(metrics.gap);

    mask_->SetMinSize(wxSize(metrics.height, metrics.height));
    mask_->SetMaxSize(wxSize(metrics.height, metrics.height));
    log_->SetMinSize(wxSize(metrics.log_min_width, metrics.height));
    log_->SetMaxSize(wxSize(-1, metrics.height));
    for (wxSizerItem* spacer : spacers_) {
        if (spacer)
            spacer->AssignSpacer(wxSize(metrics.spacer, 0));
    }
    if (outer_ && outer_->GetItemCount() > 0)
        outer_->GetItem(static_cast<size_t>(0))->SetBorder(metrics.margin);

    update_wordmark();
    retranslate();
    InvalidateBestSize();
    if (GetSizer()) {
        Layout();
        if (GetParent())
            GetParent()->Layout();
    }
}

bool ControlPanel::SetBackgroundColour(wxColour const& colour) {
    bool const changed = wxPanel::SetBackgroundColour(colour);
    if (!logo_)
        return changed;  // during construction
    update_wordmark();
    // the drawn children paint their corners in this colour; a change of it repaints them
    for (wxWindow* child : {static_cast<wxWindow*>(display_), static_cast<wxWindow*>(log_),
                            static_cast<wxWindow*>(mask_), static_cast<wxWindow*>(import_key_),
                            static_cast<wxWindow*>(open_key_), static_cast<wxWindow*>(save_key_),
                            static_cast<wxWindow*>(setup_key_)}) {
        child->Refresh();
    }
    return changed;
}

void ControlPanel::retranslate() {
    struct Entry {
        Key* key;
        char const* label;
        char const* tip;
    };
    Entry const entries[] = {{import_key_, "panel.import", "panel.import.tip"},
                             {open_key_, "panel.open", "panel.open.tip"},
                             {save_key_, "panel.save", "panel.save.tip"},
                             {setup_key_, "panel.setup", "panel.setup.tip"}};
    for (auto const& entry : entries) {
        entry.key->SetLabel(wx(text_of(entry.label)));
        entry.key->SetToolTip(wx(text_of(entry.tip)));
        entry.key->SetLayoutDirection(language_layout_direction());
    }
    // the font as large as lets every label fit
    wxFont font = import_key_->GetFont();
    int pixels = label_px_;
    int const inset =
        std::max(2, static_cast<int>(std::lround(4 * (applied_scale_ > 0 ? applied_scale_ : 1))));
    auto widest = [&] {
        font = pixel_font(font, pixels);
        int w = 0;
        for (auto const& entry : entries) {
            int tw = 0, th = 0;
            entry.key->GetTextExtent(entry.key->GetLabelText(), &tw, &th, nullptr, nullptr, &font);
            w = std::max(w, tw);
        }
        return w;
    };
    while (pixels > 8 && widest() > key_side_ - inset)
        --pixels;
    widest();  // leaves `font` at the chosen size
    for (auto const& entry : entries)
        entry.key->SetFont(font);
}

void ControlPanel::update_wordmark() {
    wxBitmapBundle const svg = wxBitmapBundle::FromSVG(
        reinterpret_cast<char const*>(indicator_wordmark_svg), wxSize(2125, 725));
    if (!svg.IsOk())
        return;
    wxSize const natural = svg.GetDefaultSize();
    double const scale = GetContentScaleFactor();
    wxSize const logical(
        static_cast<int>(std::lround(wordmark_height_ * double(natural.x) / natural.y)),
        wordmark_height_);
    wxSize const pixels(static_cast<int>(std::lround(logical.x * scale)),
                        static_cast<int>(std::lround(logical.y * scale)));
    wxImage image = svg.GetBitmap(pixels).ConvertToImage();
    if (!image.IsOk())
        return;
    // painted over in the colour, its edges' alpha kept
    unsigned char const value = is_dark(GetBackgroundColour()) ? 255 : 0;
    unsigned char* rgb = image.GetData();
    for (int i = 0; i < image.GetWidth() * image.GetHeight(); ++i)
        rgb[3 * i] = rgb[3 * i + 1] = rgb[3 * i + 2] = value;
    wordmark_scale_ = scale;
    logo_->SetBitmap(wxBitmap(image, wxBITMAP_SCREEN_DEPTH, scale));
    logo_->SetMinSize(logical);
    InvalidateBestSize();
    if (GetSizer())
        Layout();
}
