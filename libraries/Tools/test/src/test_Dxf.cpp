#include <Tools/Tools.h>
// Include 3rd party headers
#include <gtest/gtest.h>
// Include system headers
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <vector>
#include <string>

namespace test_Dxf {

std::string lines(std::initializer_list<char const*> items) {
    std::string out;
    for (auto const* s : items) out += std::string(s) + "\n";
    return out;
}

// A 40 x 30 plate with a circular hole of radius 5 (LWPOLYLINE and CIRCLE),
std::string const plate = lines({"0", "LWPOLYLINE", "8", "0", "90", "4", "70", "1", "10", "0", "20", "0", "10", "40", "20", "0",
                                 "10", "40", "20", "30", "10", "0", "20", "30", "0", "CIRCLE", "8", "0", "10", "20", "20",
                                 "15", "40", "5"});
// a triangle drawn as three LINEs,
std::string const triangle = lines({"0", "LINE", "10", "50", "20", "0", "11", "60", "21", "0", "0", "LINE", "10", "60", "20",
                                    "0", "11", "50", "21", "10", "0", "LINE", "10", "50", "20", "10", "11", "50", "21", "0"});
// and a half disc drawn as an ARC and a LINE.
std::string const half_disc = lines({"0", "ARC", "10", "80", "20", "0", "40", "10", "50", "0", "51", "180", "0", "LINE",
                                     "10", "70", "20", "0", "11", "90", "21", "0"});

std::string drawing(std::string const& body) {
    return lines({"0", "SECTION", "2", "ENTITIES"}) + body + lines({"0", "ENDSEC", "0", "EOF"});
}

TEST(test_Tools_dxf, closed_rings) {
    auto read = Tools::parse_dxf(drawing(plate + triangle + half_disc));
    ASSERT_TRUE(read.isValue()) << read.error();
    auto const& rings = read();
    ASSERT_EQ(rings.size(), 4u);
    // the circle is two vertices with bulges 1, 1
    int circles = 0, triangles = 0, half_discs = 0;
    for (auto const& ring : rings) {
        if (ring.vertices.size() == 2 && ring.bulges == std::vector<double> {1, 1}) circles++;
        if (ring.vertices.size() == 3) triangles++;
        if (ring.vertices.size() == 2 && ring.bulges != std::vector<double> {1, 1}) half_discs++;
    }
    EXPECT_EQ(circles, 1);
    EXPECT_EQ(triangles, 1);
    EXPECT_EQ(half_discs, 1);
    EXPECT_TRUE(Tools::parse_dxf(drawing("")).isError());
}

// The writer draws the sheet and its defect, moved so its bounding box starts at the origin.
TEST(test_Tools_dxf, write_and_read_back) {
    Pot::Polygon64 const sheet {{{10000, 5000}, {60000, 5000}, {60000, 35000}, {10000, 35000}},
                               {{{50000, 25000}, {50000, 30000}, {55000, 30000}, {55000, 25000}}}};
    auto text = Tools::to_dxf(sheet);
    ASSERT_TRUE(text.isValue()) << text.error();
    auto again = Tools::parse_dxf(text());
    ASSERT_TRUE(again.isValue()) << again.error();
    ASSERT_EQ(again().size(), 2u);
    EXPECT_EQ(again()[0].vertices, (Pot::Path64 {{0, 0}, {50000, 0}, {50000, 30000}, {0, 30000}}));
    EXPECT_EQ(again()[1].vertices, (Pot::Path64 {{40000, 20000}, {40000, 25000}, {45000, 25000}, {45000, 20000}}));
    EXPECT_NE(text().find("\nDEFECT\n"), std::string::npos);
}

std::vector<Pot::ArcPath64> read_back(std::string const& text) {
    auto again = Tools::parse_dxf(text);
    EXPECT_TRUE(again.isValue());
    return again.isValue() ? again() : std::vector<Pot::ArcPath64> {};
}

TEST(test_Tools_dxf, PolygonsOnOneLayerReadBack) {
    std::vector<Pot::Polygon64> const polygons {
        {{{10000, 5000}, {60000, 5000}, {60000, 35000}, {10000, 35000}},
         {{{50000, 25000}, {50000, 30000}, {55000, 30000}, {55000, 25000}}}},
        {{{70000, 5000}, {90000, 5000}, {90000, 20000}, {70000, 20000}}, {}}};
    auto text = Tools::to_dxf(polygons, "OBJECT");
    ASSERT_TRUE(text.isValue()) << text.error();
    EXPECT_EQ(read_back(text()).size(), 3u);
    EXPECT_NE(text().find("OBJECT"), std::string::npos);
    EXPECT_EQ(text().find("SHEET"), std::string::npos);
}

// the image mm frame, unshifted: objects.dxf lines up with the photo
TEST(test_Tools_dxf, PolygonsKeepImageFrame) {
    std::vector<Pot::Polygon64> const polygons {{{{10000, 5000}, {60000, 5000}, {60000, 35000}, {10000, 35000}}, {}},
                                                {{{70000, 2000}, {90000, 2000}, {90000, 20000}}, {}}};
    auto text = Tools::to_dxf(polygons, "OBJECT");
    ASSERT_TRUE(text.isValue()) << text.error();
    auto const rings = read_back(text());
    ASSERT_EQ(rings.size(), 2u);
    EXPECT_EQ(rings[0].vertices, polygons[0].outer);
    EXPECT_EQ(rings[1].vertices, polygons[1].outer);
}

TEST(test_Tools_dxf, NoPolygonsIsAnError) {
    EXPECT_TRUE(Tools::to_dxf(std::vector<Pot::Polygon64> {}, "OBJECT").isError());
    EXPECT_TRUE(Tools::to_dxf(std::vector<Pot::Polygon64> {{{{0, 0}, {1, 1}}, {}}}, "OBJECT").isError());
}

}  // namespace test_Dxf
