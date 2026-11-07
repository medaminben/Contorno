#ifndef INDICATOR_PHOTOVIEW_H
#define INDICATOR_PHOTOVIEW_H
#include <Pot/Pot.h>
#include <Vision/Camera.h>

#include <wx/bitmap.h>
#include <wx/colour.h>
#include <wx/graphics.h>
#include <wx/brush.h>
#include <wx/image.h>
#include <wx/pen.h>
#include <wx/scrolwin.h>

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

/// The AI mask as a grey image: black 0, white 255, one pixel per mask value.
/// Invalid (`!IsOk()`) for an empty mask or one whose data does not fit its size.
wxImage mask_image(Vision::Mask const& mask);

/// A photo with polygon layers over it, drawn by hand; the wheel zooms about
/// the view's centre (a factor 1.15 per notch); link_to makes views zoom and
/// scroll together. The photo is fitted in the view and stays fitted when the
/// view is resized, until the wheel zooms it; then the zoom stays, the photo
/// can be dragged with the hand and the scroll bars show. The backdrop behind
/// the photo is grey (60, 60, 60) at first.
class PhotoView : public wxScrolledCanvas {
public:
    explicit PhotoView(wxWindow* parent, wxWindowID id = wxID_ANY);

    /// Replaces the content with `photo`; fits it in the view unless
    /// `keep_view` and the photo has the size of the one shown (then the zoom
    /// and scroll stay, e.g. the mask of a new result). Removes the polygons.
    void set_photo(wxImage const& photo, Vision::PixelFrame frame, bool keep_view = false);
    /// Adds a layer of polygons in the image mm frame (y up, origin
    /// bottom-left; drawn with to_px of the photo's frame, the pen as many
    /// device pixels wide as it says at every zoom, odd-even fill) over the earlier ones.
    void add_polygons(std::vector<Pot::Polygon64> const& mm_polygons, wxPen pen, wxBrush brush);
    /// Removes the polygon layers from the given one (0 = all) on.
    void remove_polygons(std::size_t first_layer = 0);
    /// Shows or hides one polygon layer.
    void set_layer_visible(std::size_t layer, bool visible);
    std::size_t polygon_layers() const { return layers_.size(); }
    /// The photo with the visible layers, at photo resolution, with an alpha channel: transparent where the
    /// photo is (invalid without a photo).
    wxImage render_image() const;
    /// Removes the photo and the polygons.
    void clear();
    /// Zoom and scroll together with `other` (both ways).
    void link_to(PhotoView* other);
    /// Called after a wheel zoom (or a linked view's) with the device pixels per image pixel and the zoomed flag.
    void on_zoom_changed(std::function<void(double zoom, bool zoomed)> listener) { listener_ = std::move(listener); }
    /// The colour behind the photo; an invalid one takes the parent's background (a view without a box).
    void set_backdrop(wxColour const& colour);

    bool has_photo() const { return image_.IsOk(); }
    /// Device pixels per image pixel now.
    double zoom() const;
    /// Fitted in the view (not zoomed by the wheel).
    bool fitted() const { return !zoomed_; }
    /// Multiplies the zoom by `factor` about the client point `at`, which stays over the same image point.
    void zoom_at(wxPoint at, double factor);
    /// The image point (pixels, fractional) under a client point.
    wxPoint2DDouble image_point_at(wxPoint client) const;
    /// How far the photo is scrolled (px).
    wxPoint view_origin() const;
    /// The view as drawn, `scale` device pixels per logical one.
    wxBitmap render(double scale) const;

private:
    struct Layer {
        std::vector<std::vector<wxPoint2DDouble>> rings;  ///< in image pixels, pixel (i, j) spanning [i, i + 1)
        wxPen pen;
        wxBrush brush;
        bool visible = true;
    };
    /// The top-left of the photo in client coordinates at the current scroll (it is centred while smaller than the view).
    wxPoint2DDouble origin() const;
    void update_scrollbars(wxPoint keep_origin);
    void draw(wxGraphicsContext& gc, wxSize size) const;
    /// The photo and the visible layers with the photo's top-left at `origin`, `scale` device pixels per image pixel.
    void draw_photo(wxGraphicsContext& gc, wxPoint2DDouble origin, double scale) const;
    /// Follows a linked view: its zoom and zoomed flag, then (follow_scroll) its scroll.
    void follow_zoom(double zoom, bool zoomed);
    void follow_scroll(wxPoint origin);
    /// Tells the linked views where this one is scrolled.
    void sync_scroll();
    void on_paint(wxPaintEvent&);
    void on_size(wxSizeEvent&);
    void on_wheel(wxMouseEvent&);
    void on_down(wxMouseEvent&);
    void on_move(wxMouseEvent&);
    void on_up(wxMouseEvent&);

    wxImage image_;
    wxBitmap bitmap_;
    mutable wxGraphicsBitmap graphics_bitmap_;
    Vision::PixelFrame frame_ {};
    std::vector<Layer> layers_;
    std::vector<PhotoView*> linked_;
    std::function<void(double, bool)> listener_;
    wxColour backdrop_ {60, 60, 60};
    bool zoomed_ = false;
    bool following_ = false;  ///< applying a linked view's change: not passed on again
    double scale_ = 1;  ///< the zoom while zoomed
    bool dragging_ = false;
    wxPoint drag_from_, drag_origin_;
};

#endif  // INDICATOR_PHOTOVIEW_H
