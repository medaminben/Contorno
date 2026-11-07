#include "ui/PhotoView.h"

#include "ui/Paint.h"

#include <wx/dcbuffer.h>
#include <wx/dcmemory.h>

#include <algorithm>
#include <cmath>
#include <memory>

namespace {

double const wheel_step = 1.15;
// guards against a runaway wheel, not a limit anyone reaches: the Qt view has none
double const min_zoom = 1e-3, max_zoom = 1e3;

}  // namespace

wxImage mask_image(Vision::Mask const& mask) {
    auto const w = static_cast<std::size_t>(std::max(mask.width, 0));
    auto const h = static_cast<std::size_t>(std::max(mask.height, 0));
    if (w == 0 || h == 0 || mask.data.size() != w * h)
        return {};
    wxImage image(mask.width, mask.height, false);
    unsigned char* rgb = image.GetData();
    for (std::size_t i = 0; i < w * h; ++i)
        rgb[3 * i] = rgb[3 * i + 1] = rgb[3 * i + 2] = mask.data[i];
    return image;
}

PhotoView::PhotoView(wxWindow* parent, wxWindowID id) {
    wxScrolledCanvas::Create(parent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetCursor(wxCursor(wxCURSOR_HAND));
    Bind(wxEVT_PAINT, &PhotoView::on_paint, this);
    Bind(wxEVT_SIZE, &PhotoView::on_size, this);
    Bind(wxEVT_MOUSEWHEEL, &PhotoView::on_wheel, this);
    Bind(wxEVT_LEFT_DOWN, &PhotoView::on_down, this);
    Bind(wxEVT_MOTION, &PhotoView::on_move, this);
    Bind(wxEVT_LEFT_UP, &PhotoView::on_up, this);
    // the scroll bars moved by the user: the linked views follow once the position is final
    Bind(wxEVT_SCROLLWIN_TOP, [this](wxScrollWinEvent& e) {
        e.Skip();
        CallAfter([this] { sync_scroll(); });
    });
    for (wxEventType type :
         {wxEVT_SCROLLWIN_BOTTOM, wxEVT_SCROLLWIN_LINEUP, wxEVT_SCROLLWIN_LINEDOWN,
          wxEVT_SCROLLWIN_PAGEUP, wxEVT_SCROLLWIN_PAGEDOWN, wxEVT_SCROLLWIN_THUMBTRACK,
          wxEVT_SCROLLWIN_THUMBRELEASE}) {
        Bind(wxEventTypeTag<wxScrollWinEvent>(type), [this](wxScrollWinEvent& e) {
            e.Skip();
            CallAfter([this] { sync_scroll(); });
        });
    }
    Bind(wxEVT_MOUSE_CAPTURE_LOST, [this](wxMouseCaptureLostEvent&) { dragging_ = false; });
}

void PhotoView::clear() {
    image_ = wxImage();
    bitmap_ = wxBitmap();
    graphics_bitmap_ = wxGraphicsBitmap();
    layers_.clear();
    frame_ = {};
    zoomed_ = false;
    scale_ = 1;
    dragging_ = false;
    if (HasCapture())
        ReleaseMouse();
    SetScrollbars(1, 1, 0, 0);
    Refresh();
}

void PhotoView::set_backdrop(wxColour const& colour) {
    backdrop_ = colour;
    Refresh();
}

void PhotoView::set_photo(wxImage const& photo, Vision::PixelFrame frame, bool keep_view) {
    bool const keep =
        keep_view && image_.IsOk() && photo.IsOk() && photo.GetSize() == image_.GetSize();
    wxPoint const kept = view_origin();
    double const kept_scale = scale_;
    bool const kept_zoomed = zoomed_;
    clear();
    image_ = photo;
    frame_ = frame;
    if (!image_.IsOk())
        return;
    bitmap_ = wxBitmap(image_);
    if (keep && kept_zoomed) {
        zoomed_ = true;
        scale_ = kept_scale;
        update_scrollbars(kept);
    }
    Refresh();
}

void PhotoView::add_polygons(std::vector<Pot::Polygon64> const& mm_polygons, wxPen pen,
                             wxBrush brush) {
    Layer layer;
    layer.pen = pen;
    layer.brush = brush;
    auto add_ring = [&](Pot::Path64 const& ring) {
        std::vector<wxPoint2DDouble> points;
        for (auto const& p : ring) {
            auto const px = Vision::to_px(frame_, Pot::to_pointD(p));
            // Vision pixel coordinates index pixel centres; a pixel (i, j) spans [i, i + 1) here
            points.emplace_back(px.x + 0.5, px.y + 0.5);
        }
        if (!points.empty())
            layer.rings.push_back(std::move(points));
    };
    for (auto const& polygon : mm_polygons) {
        add_ring(polygon.outer);
        for (auto const& hole : polygon.holes)
            add_ring(hole);
    }
    layers_.push_back(std::move(layer));
    Refresh();
}

void PhotoView::remove_polygons(std::size_t first_layer) {
    if (first_layer < layers_.size())
        layers_.erase(layers_.begin() + static_cast<std::ptrdiff_t>(first_layer), layers_.end());
    Refresh();
}

void PhotoView::set_layer_visible(std::size_t layer, bool visible) {
    if (layer >= layers_.size())
        return;
    layers_[layer].visible = visible;
    Refresh();
}

wxImage PhotoView::render_image() const {
    if (!image_.IsOk())
        return {};
    // on a transparent start: a photo with transparency (the mask's shape) keeps it
    wxImage blank(image_.GetWidth(), image_.GetHeight(), true);
    blank.SetAlpha();
    std::fill_n(
        blank.GetAlpha(),
        static_cast<std::size_t>(image_.GetWidth()) * static_cast<std::size_t>(image_.GetHeight()),
        0);
    wxBitmap bitmap(blank, 32);
    wxMemoryDC dc(bitmap);
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (gc) {
        gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
        wxGraphicsBitmap const own = gc->CreateBitmap(bitmap_);
        gc->DrawBitmap(own, 0, 0, image_.GetWidth(), image_.GetHeight());
        for (auto const& layer : layers_) {
            if (!layer.visible)
                continue;
            gc->SetPen(layer.pen);
            gc->SetBrush(layer.brush);
            wxGraphicsPath path = gc->CreatePath();
            for (auto const& ring : layer.rings) {
                for (std::size_t i = 0; i < ring.size(); ++i) {
                    if (i == 0)
                        path.MoveToPoint(ring[i].m_x, ring[i].m_y);
                    else
                        path.AddLineToPoint(ring[i].m_x, ring[i].m_y);
                }
                path.CloseSubpath();
            }
            gc->DrawPath(path, wxODDEVEN_RULE);
        }
    }
    gc.reset();
    dc.SelectObject(wxNullBitmap);
    return bitmap.ConvertToImage();
}

double PhotoView::zoom() const {
    if (!image_.IsOk())
        return 1;
    if (zoomed_)
        return scale_;
    wxSize const view = GetClientSize();
    return std::min(static_cast<double>(view.x) / image_.GetWidth(),
                    static_cast<double>(view.y) / image_.GetHeight());
}

wxPoint PhotoView::view_origin() const {
    int x = 0, y = 0;
    GetViewStart(&x, &y);
    return {x, y};
}

wxPoint2DDouble PhotoView::origin() const {
    if (!image_.IsOk())
        return {0, 0};
    wxSize const view = GetClientSize();
    double const s = zoom();
    wxPoint const scrolled = view_origin();
    double const w = image_.GetWidth() * s, h = image_.GetHeight() * s;
    // smaller than the view in a direction: centred there; larger: scrolled
    double const x = w < view.x ? (view.x - w) / 2 : -scrolled.x;
    double const y = h < view.y ? (view.y - h) / 2 : -scrolled.y;
    return {x, y};
}

wxPoint2DDouble PhotoView::image_point_at(wxPoint client) const {
    wxPoint2DDouble const o = origin();
    double const s = zoom();
    return {(client.x - o.m_x) / s, (client.y - o.m_y) / s};
}

void PhotoView::update_scrollbars(wxPoint keep_origin) {
    if (!image_.IsOk() || !zoomed_) {
        SetScrollbars(1, 1, 0, 0);
        return;
    }
    int const w = static_cast<int>(std::ceil(image_.GetWidth() * scale_));
    int const h = static_cast<int>(std::ceil(image_.GetHeight() * scale_));
    SetScrollbars(1, 1, w, h, keep_origin.x, keep_origin.y);
}

void PhotoView::zoom_at(wxPoint at, double factor) {
    if (!image_.IsOk())
        return;
    wxPoint2DDouble const under = image_point_at(at);
    double const target = std::clamp(zoom() * factor, min_zoom, max_zoom);
    zoomed_ = true;
    scale_ = target;
    update_scrollbars(view_origin());
    // the scroll that puts `under` back at `at` (the photo's centring is part of origin())
    wxPoint2DDouble const o = origin();
    wxPoint2DDouble const now = {o.m_x + under.m_x * target, o.m_y + under.m_y * target};
    wxPoint const view = view_origin();
    Scroll(static_cast<int>(std::lround(view.x + now.m_x - at.x)),
           static_cast<int>(std::lround(view.y + now.m_y - at.y)));
    Refresh();
    if (following_)
        return;
    for (PhotoView* other : linked_)
        other->follow_zoom(scale_, true);
    sync_scroll();
    if (listener_)
        listener_(scale_, true);
}

void PhotoView::follow_zoom(double zoom, bool zoomed) {
    if (!image_.IsOk() || (zoom == scale_ && zoomed == zoomed_))
        return;
    following_ = true;
    zoomed_ = zoomed;
    scale_ = zoom;
    update_scrollbars(view_origin());
    Refresh();
    following_ = false;
    if (listener_)
        listener_(zoom, zoomed);
}

void PhotoView::follow_scroll(wxPoint origin) {
    if (view_origin() == origin)
        return;
    following_ = true;
    Scroll(origin.x, origin.y);
    Refresh();
    following_ = false;
}

void PhotoView::sync_scroll() {
    if (following_)
        return;
    for (PhotoView* other : linked_)
        other->follow_scroll(view_origin());
}

void PhotoView::link_to(PhotoView* other) {
    if (other == this || std::find(linked_.begin(), linked_.end(), other) != linked_.end())
        return;
    linked_.push_back(other);
    other->linked_.push_back(this);
}

void PhotoView::on_wheel(wxMouseEvent& event) {
    if (!image_.IsOk() || event.GetWheelRotation() == 0) {
        event.Skip();
        return;
    }
    // about the view's centre, as the Qt view (its default transformation anchor)
    wxSize const view = GetClientSize();
    zoom_at(wxPoint(view.x / 2, view.y / 2),
            event.GetWheelRotation() > 0 ? wheel_step : 1 / wheel_step);
}

void PhotoView::on_down(wxMouseEvent& event) {
    if (!image_.IsOk()) {
        event.Skip();
        return;
    }
    dragging_ = true;
    drag_from_ = event.GetPosition();
    drag_origin_ = view_origin();
    // a window not on screen has nothing to grab the pointer with
    if (!HasCapture() && IsShownOnScreen())
        CaptureMouse();
}

void PhotoView::on_move(wxMouseEvent& event) {
    if (!dragging_) {
        event.Skip();
        return;
    }
    if (!event.LeftIsDown()) {  // the release was missed
        dragging_ = false;
        if (HasCapture())
            ReleaseMouse();
        return;
    }
    // the photo follows the hand
    wxPoint const delta = event.GetPosition() - drag_from_;
    Scroll(drag_origin_.x - delta.x, drag_origin_.y - delta.y);
    Refresh();
    sync_scroll();
}

void PhotoView::on_up(wxMouseEvent& event) {
    dragging_ = false;
    if (HasCapture())
        ReleaseMouse();
    event.Skip();
}

void PhotoView::on_size(wxSizeEvent& event) {
    if (zoomed_) {
        update_scrollbars(view_origin());
        sync_scroll();  // the new range may have clamped the scroll
    }
    Refresh();
    event.Skip();
}

void PhotoView::draw(wxGraphicsContext& gc, wxSize size) const {
    gc.SetAntialiasMode(wxANTIALIAS_DEFAULT);
    wxColour const backdrop = backdrop_.IsOk() ? backdrop_
                                               : (GetParent() ? GetParent()->GetBackgroundColour()
                                                              : GetBackgroundColour());
    gc.SetPen(*wxTRANSPARENT_PEN);
    gc.SetBrush(wxBrush(backdrop));
    gc.DrawRectangle(0, 0, size.x, size.y);
    if (!image_.IsOk())
        return;

    draw_photo(gc, origin(), zoom());
}

void PhotoView::draw_photo(wxGraphicsContext& gc, wxPoint2DDouble o, double s) const {
    if (bitmap_.IsOk()) {
        if (graphics_bitmap_.IsNull())
            graphics_bitmap_ = gc.CreateBitmap(bitmap_);
        gc.PushState();
        gc.Translate(o.m_x, o.m_y);
        gc.Scale(s, s);
        gc.SetInterpolationQuality(
            wxINTERPOLATION_NONE);  // pixels stay square, as the Qt view draws them
        gc.DrawBitmap(graphics_bitmap_, 0, 0, image_.GetWidth(), image_.GetHeight());
        gc.PopState();
    }
    // the layers at their pens' own width, whatever the zoom
    for (auto const& layer : layers_) {
        if (!layer.visible)
            continue;
        gc.SetPen(layer.pen);
        gc.SetBrush(layer.brush);
        wxGraphicsPath path = gc.CreatePath();
        for (auto const& ring : layer.rings) {
            for (std::size_t i = 0; i < ring.size(); ++i) {
                double const x = o.m_x + ring[i].m_x * s, y = o.m_y + ring[i].m_y * s;
                if (i == 0)
                    path.MoveToPoint(x, y);
                else
                    path.AddLineToPoint(x, y);
            }
            path.CloseSubpath();
        }
        gc.DrawPath(path, wxODDEVEN_RULE);
    }
}

void PhotoView::on_paint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    std::unique_ptr<wxGraphicsContext> gc = paint_context(dc, *this);
    if (gc)
        draw(*gc, GetClientSize());
}

wxBitmap PhotoView::render(double scale) const {
    wxSize const size = GetClientSize();
    wxBitmap bitmap;
    bitmap.CreateWithDIPSize(size, scale);
    wxMemoryDC dc(bitmap);
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (gc) {
        graphics_bitmap_ = wxGraphicsBitmap();  // belongs to the context that made it
        draw(*gc, size);
        graphics_bitmap_ = wxGraphicsBitmap();
    }
    gc.reset();
    dc.SelectObject(wxNullBitmap);
    return bitmap;
}
