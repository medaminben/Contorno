#include <Tools/Dxf.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace Tools {
namespace {

struct DxfPoint {
    double x, y;
};

/// An open or closed path: vertices with the bulge of the edge to the next.
struct ArcChain {
    std::vector<DxfPoint> points;
    std::vector<double> bulges;
};

using Groups = std::vector<std::pair<int, std::string>>;

std::string trimmed(std::string s) {
    auto const not_space = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

/// "dxf: <what>"; the reasons inside are not worded separately.
Core::Problem dxf_problem(std::string what) {
    return Core::Problem("dxf.failed", "dxf: {what}", {{"what", std::move(what)}});
}

double number(std::string const& s) {
    std::size_t used = 0;
    double const v = std::stod(s, &used);
    if (!std::isfinite(v))
        throw std::invalid_argument("number not finite: " + s);
    return v;
}

/// The entities of the ENTITIES section, each as its group list (code 0 first).
std::vector<Groups> entities(std::string const& text) {
    std::istringstream in(text);
    std::vector<std::pair<int, std::string>> pairs;
    std::string code, value;
    while (std::getline(in, code)) {
        if (!std::getline(in, value))
            break;
        code = trimmed(code);
        if (code.empty())
            continue;
        pairs.emplace_back(std::stoi(code), trimmed(value));
    }
    std::vector<Groups> out;
    bool in_entities = false;
    for (std::size_t i = 0; i < pairs.size(); ++i) {
        auto const& [c, v] = pairs[i];
        if (c == 0 && v == "SECTION" && i + 1 < pairs.size() && pairs[i + 1].first == 2) {
            in_entities = pairs[i + 1].second == "ENTITIES";
            continue;
        }
        if (c == 0 && v == "ENDSEC") {
            in_entities = false;
            continue;
        }
        if (!in_entities)
            continue;
        if (c == 0)
            out.push_back({{c, v}});
        else if (!out.empty())
            out.back().push_back({c, v});
    }
    return out;
}

double bulge_of_arc(double start_deg, double end_deg) {
    double sweep = std::fmod(end_deg - start_deg, 360.0);
    if (sweep <= 0)
        sweep += 360;
    return std::tan(sweep * std::numbers::pi / 180 / 4);
}

/// Closed chains and open pieces of the drawing.
void collect(std::vector<Groups> const& list, std::vector<ArcChain>& closed,
             std::vector<ArcChain>& open) {
    for (std::size_t e = 0; e < list.size(); ++e) {
        auto const& g = list[e];
        std::string const& type = g.front().second;
        auto first = [&](int code) -> std::optional<double> {
            for (auto const& [c, v] : g)
                if (c == code)
                    return number(v);
            return std::nullopt;
        };
        if (type == "LWPOLYLINE") {
            ArcChain chain;
            int flags = 0;
            for (auto const& [c, v] : g) {
                if (c == 70)
                    flags = static_cast<int>(number(v));
                if (c == 10) {
                    chain.points.push_back({number(v), 0});
                    chain.bulges.push_back(0);
                }
                if (c == 20 && !chain.points.empty())
                    chain.points.back().y = number(v);
                if (c == 42 && !chain.bulges.empty())
                    chain.bulges.back() = number(v);
            }
            if (chain.points.size() >= 2)
                ((flags & 1) ? closed : open).push_back(std::move(chain));
        } else if (type == "POLYLINE") {
            int const flags = static_cast<int>(first(70).value_or(0));
            ArcChain chain;
            for (++e; e < list.size() && list[e].front().second == "VERTEX"; ++e) {
                DxfPoint p {0, 0};
                double b = 0;
                for (auto const& [c, v] : list[e]) {
                    if (c == 10)
                        p.x = number(v);
                    if (c == 20)
                        p.y = number(v);
                    if (c == 42)
                        b = number(v);
                }
                chain.points.push_back(p);
                chain.bulges.push_back(b);
            }
            // e is now at SEQEND (or the next entity, which the loop revisits)
            if (e < list.size() && list[e].front().second != "SEQEND")
                --e;
            if (chain.points.size() >= 2)
                ((flags & 1) ? closed : open).push_back(std::move(chain));
        } else if (type == "CIRCLE") {
            double const cx = first(10).value_or(0), cy = first(20).value_or(0),
                         r = first(40).value_or(0);
            if (r > 0)
                closed.push_back({{{cx + r, cy}, {cx - r, cy}}, {1, 1}});
        } else if (type == "LINE") {
            open.push_back({{{first(10).value_or(0), first(20).value_or(0)},
                             {first(11).value_or(0), first(21).value_or(0)}},
                            {0, 0}});
        } else if (type == "ARC") {
            double const cx = first(10).value_or(0), cy = first(20).value_or(0),
                         r = first(40).value_or(0);
            double const a0 = first(50).value_or(0), a1 = first(51).value_or(0);
            if (r <= 0)
                continue;
            double const t0 = a0 * std::numbers::pi / 180, t1 = a1 * std::numbers::pi / 180;
            open.push_back({{{cx + r * std::cos(t0), cy + r * std::sin(t0)},
                             {cx + r * std::cos(t1), cy + r * std::sin(t1)}},
                            {bulge_of_arc(a0, a1), 0}});
        }
    }
}

ArcChain reversed(ArcChain c) {
    std::reverse(c.points.begin(), c.points.end());
    // the bulge of edge i (points i to i+1) becomes the negated bulge of the reversed edge
    std::vector<double> b(c.bulges.size(), 0);
    std::size_t const n = c.points.size();
    for (std::size_t i = 0; i + 1 < n; ++i)
        b[n - 2 - i] = -c.bulges[i];
    c.bulges = b;
    return c;
}

/// Joins open pieces end to end; closed loops are added to closed.
void join(std::vector<ArcChain> open, std::vector<ArcChain>& closed, double tolerance) {
    auto near = [&](DxfPoint a, DxfPoint b) {
        return std::hypot(a.x - b.x, a.y - b.y) <= tolerance;
    };
    while (!open.empty()) {
        ArcChain chain = std::move(open.back());
        open.pop_back();
        bool grown = true;
        while (grown) {
            grown = false;
            if (chain.points.size() > 2 && near(chain.points.front(), chain.points.back()))
                break;
            for (std::size_t k = 0; k < open.size(); ++k) {
                ArcChain piece = open[k];
                if (near(chain.points.back(), piece.points.back()))
                    piece = reversed(piece);
                if (!near(chain.points.back(), piece.points.front()))
                    continue;
                chain.bulges.pop_back();
                chain.points.pop_back();
                chain.points.insert(chain.points.end(), piece.points.begin(), piece.points.end());
                chain.bulges.insert(chain.bulges.end(), piece.bulges.begin(), piece.bulges.end());
                open.erase(open.begin() + static_cast<std::ptrdiff_t>(k));
                grown = true;
                break;
            }
        }
        if (chain.points.size() > 2 && near(chain.points.front(), chain.points.back())) {
            chain.points.pop_back();
            double const last = chain.bulges[chain.bulges.size() - 2];
            chain.bulges.pop_back();
            chain.bulges.back() = last;
            closed.push_back(std::move(chain));
        }
    }
}

Pot::ArcPath64 to_grid(ArcChain const& c) {
    Pot::ArcPath64 ring;
    for (std::size_t i = 0; i < c.points.size(); ++i) {
        Pot::Point64 const p {
            static_cast<std::int64_t>(std::llround(c.points[i].x * Pot::units_per_mm)),
            static_cast<std::int64_t>(std::llround(c.points[i].y * Pot::units_per_mm))};
        if (!ring.vertices.empty() && ring.vertices.back() == p)
            continue;
        ring.vertices.push_back(p);
        ring.bulges.push_back(c.bulges[i]);
    }
    if (ring.vertices.size() > 1 && ring.vertices.front() == ring.vertices.back()) {
        ring.vertices.pop_back();
        ring.bulges.pop_back();
    }
    return ring;
}

}  // namespace

Result<std::vector<Pot::ArcPath64>> parse_dxf(std::string const& text) noexcept {
    try {
        std::vector<ArcChain> closed, open;
        collect(entities(text), closed, open);
        join(std::move(open), closed, 0.01);
        std::vector<Pot::ArcPath64> rings;
        for (auto const& c : closed) {
            auto ring = to_grid(c);
            if (ring.vertices.size() >= 2)
                rings.push_back(std::move(ring));
        }
        if (rings.empty())
            throw std::runtime_error("no closed shape");
        return Result<std::vector<Pot::ArcPath64>>(std::move(rings));
    } catch (std::exception const& e) {
        return Core::error_result<std::vector<Pot::ArcPath64>>(
            [&] { return dxf_problem(e.what()); });
    }
}

namespace {

void polyline(std::ostringstream& out, Pot::Path64 const& ring, Pot::Point64 shift,
              std::string const& layer) {
    out << "0\nPOLYLINE\n8\n" << layer << "\n66\n1\n70\n1\n";
    for (auto const& p : ring)
        out << "0\nVERTEX\n8\n"
            << layer << "\n10\n"
            << static_cast<double>(p.x + shift.x) / Pot::units_per_mm << "\n20\n"
            << static_cast<double>(p.y + shift.y) / Pot::units_per_mm << "\n";
    out << "0\nSEQEND\n8\n" << layer << "\n";
}

}  // namespace

Result<std::string> to_dxf(std::vector<Pot::Polygon64> const& polygons,
                           std::string const& layer) noexcept {
    try {
        auto const usable = [](Pot::Path64 const& ring) { return ring.size() >= 3; };
        bool const any =
            std::any_of(polygons.begin(), polygons.end(), [&](Pot::Polygon64 const& polygon) {
                return usable(polygon.outer) ||
                       std::any_of(polygon.holes.begin(), polygon.holes.end(), usable);
            });
        if (!any)
            throw std::runtime_error("no ring with at least 3 points");
        Pot::Point64 const unshifted {0, 0};  // the polygons' own frame
        std::ostringstream out;
        out.precision(17);
        out << "0\nSECTION\n2\nENTITIES\n";
        for (auto const& polygon : polygons) {
            if (usable(polygon.outer))
                polyline(out, polygon.outer, unshifted, layer);
            for (auto const& h : polygon.holes)
                if (usable(h))
                    polyline(out, h, unshifted, layer);
        }
        out << "0\nENDSEC\n0\nEOF\n";
        return Result<std::string>(out.str());
    } catch (std::exception const& e) {
        return Core::error_result<std::string>([&] { return dxf_problem(e.what()); });
    }
}

Result<std::string> to_dxf(Pot::Polygon64 const& sheet) noexcept {
    try {
        std::ostringstream out;
        out.precision(17);
        out << "0\nSECTION\n2\nENTITIES\n";
        // the outline's bounding box starts at the origin
        Pot::Point64 low = sheet.outer.empty() ? Pot::Point64 {} : sheet.outer.front();
        for (auto const& v : sheet.outer) {
            low.x = std::min(low.x, v.x);
            low.y = std::min(low.y, v.y);
        }
        polyline(out, sheet.outer, -low, "SHEET");
        for (auto const& h : sheet.holes)
            polyline(out, h, -low, "DEFECT");
        out << "0\nENDSEC\n0\nEOF\n";
        return Result<std::string>(out.str());
    } catch (std::exception const& e) {
        return Core::error_result<std::string>([&] { return dxf_problem(e.what()); });
    }
}

}  // namespace Tools
