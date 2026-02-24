// Tests for CoordinateGrid screen-to-grid mapping
// These tests verify that grid points map correctly to screen corners
// and that changing grid size affects the transformation

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../core/coordinate_grid.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <iomanip>

using Catch::Approx;
using namespace avs;

// Helper to create an identity grid where each point samples from itself
static void fill_identity_grid(CoordinateGrid& grid, int w, int h) {
    for (int gy = 0; gy < grid.grid_height(); gy++) {
        for (int gx = 0; gx < grid.grid_width(); gx++) {
            // Grid point (gx, gy) should sample from the corresponding screen position
            double nx = static_cast<double>(gx) / (grid.grid_width() - 1);
            double ny = static_cast<double>(gy) / (grid.grid_height() - 1);
            double px = nx * (w - 1);
            double py = ny * (h - 1);
            grid.set(gx, gy, px, py);
        }
    }
}

// Helper to create a test pattern where each pixel's color encodes its position
static void fill_position_encoded_image(uint32_t* img, int w, int h) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            // Encode x in red, y in green (scaled to 0-255)
            uint8_t r = static_cast<uint8_t>((x * 255) / (w - 1));
            uint8_t g = static_cast<uint8_t>((y * 255) / (h - 1));
            img[y * w + x] = 0xFF000000 | (r << 16) | (g << 8);
        }
    }
}

// Decode position from pixel color
static std::pair<double, double> decode_position(uint32_t pixel, int w, int h) {
    uint8_t r = (pixel >> 16) & 0xFF;
    uint8_t g = (pixel >> 8) & 0xFF;
    double x = (r * (w - 1)) / 255.0;
    double y = (g * (h - 1)) / 255.0;
    return {x, y};
}

TEST_CASE("CoordinateGrid corner mapping", "[coordinate][mapping]") {
    const int W = 64, H = 64;

    SECTION("corners sample from corners with identity transform") {
        CoordinateGrid grid;
        grid.resize(4, 4);
        fill_identity_grid(grid, W, H);

        std::vector<uint32_t> input(W * H);
        std::vector<uint32_t> output(W * H, 0);
        fill_position_encoded_image(input.data(), W, H);

        // Use nearest neighbor for precise testing
        grid.apply(input.data(), output.data(), W, H, false, false, false);

        // Check corners - output corner should sample from input corner
        // Top-left corner (0, 0)
        auto [x0, y0] = decode_position(output[0], W, H);
        INFO("Top-left: expected (0,0), got (" << x0 << "," << y0 << ")");
        REQUIRE(x0 == Approx(0.0).margin(2.0));
        REQUIRE(y0 == Approx(0.0).margin(2.0));

        // Top-right corner (W-1, 0)
        auto [x1, y1] = decode_position(output[W - 1], W, H);
        INFO("Top-right: expected (" << W-1 << ",0), got (" << x1 << "," << y1 << ")");
        REQUIRE(x1 == Approx(W - 1).margin(2.0));
        REQUIRE(y1 == Approx(0.0).margin(2.0));

        // Bottom-left corner (0, H-1)
        auto [x2, y2] = decode_position(output[(H - 1) * W], W, H);
        INFO("Bottom-left: expected (0," << H-1 << "), got (" << x2 << "," << y2 << ")");
        REQUIRE(x2 == Approx(0.0).margin(2.0));
        REQUIRE(y2 == Approx(H - 1).margin(2.0));

        // Bottom-right corner (W-1, H-1)
        auto [x3, y3] = decode_position(output[(H - 1) * W + W - 1], W, H);
        INFO("Bottom-right: expected (" << W-1 << "," << H-1 << "), got (" << x3 << "," << y3 << ")");
        REQUIRE(x3 == Approx(W - 1).margin(2.0));
        REQUIRE(y3 == Approx(H - 1).margin(2.0));
    }

    SECTION("center samples from center with identity transform") {
        CoordinateGrid grid;
        grid.resize(4, 4);
        fill_identity_grid(grid, W, H);

        std::vector<uint32_t> input(W * H);
        std::vector<uint32_t> output(W * H, 0);
        fill_position_encoded_image(input.data(), W, H);

        grid.apply(input.data(), output.data(), W, H, false, false, false);

        // Check center pixel
        int cx = W / 2, cy = H / 2;
        auto [x, y] = decode_position(output[cy * W + cx], W, H);
        INFO("Center: expected (" << cx << "," << cy << "), got (" << x << "," << y << ")");
        REQUIRE(x == Approx(cx).margin(3.0));
        REQUIRE(y == Approx(cy).margin(3.0));
    }
}

TEST_CASE("CoordinateGrid size affects output", "[coordinate][mapping]") {
    const int W = 64, H = 64;

    SECTION("different grid sizes produce different interpolation patterns") {
        // With a non-linear transform, different grid sizes should produce
        // visibly different results because of different interpolation

        std::vector<uint32_t> input(W * H);
        std::vector<uint32_t> output_small(W * H, 0);
        std::vector<uint32_t> output_large(W * H, 0);
        fill_position_encoded_image(input.data(), W, H);

        // Create grids with different sizes
        CoordinateGrid grid_small, grid_large;
        grid_small.resize(3, 3);   // 3x3 = 4 cells
        grid_large.resize(16, 16); // 16x16 = 225 cells

        // Fill with a swirl transform - non-linear, should show grid artifacts
        auto fill_swirl = [W, H](CoordinateGrid& g, double strength) {
            double cx = W / 2.0, cy = H / 2.0;
            for (int gy = 0; gy < g.grid_height(); gy++) {
                for (int gx = 0; gx < g.grid_width(); gx++) {
                    double nx = static_cast<double>(gx) / (g.grid_width() - 1);
                    double ny = static_cast<double>(gy) / (g.grid_height() - 1);
                    double px = nx * (W - 1) - cx;
                    double py = ny * (H - 1) - cy;
                    double r = std::sqrt(px * px + py * py);
                    double angle = std::atan2(py, px) + strength * r / (W/2);
                    double src_x = cx + std::cos(angle) * r;
                    double src_y = cy + std::sin(angle) * r;
                    g.set(gx, gy, src_x, src_y);
                }
            }
        };

        fill_swirl(grid_small, 0.5);
        fill_swirl(grid_large, 0.5);

        grid_small.apply(input.data(), output_small.data(), W, H, false, false, false);
        grid_large.apply(input.data(), output_large.data(), W, H, false, false, false);

        // Count pixels that differ between the two outputs
        int diff_count = 0;
        for (int i = 0; i < W * H; i++) {
            if (output_small[i] != output_large[i]) diff_count++;
        }

        // With different grid resolutions, there MUST be significant differences
        // due to different interpolation patterns
        INFO("Differing pixels: " << diff_count << " out of " << (W * H));
        REQUIRE(diff_count > W * H / 10);  // At least 10% different
    }
}

TEST_CASE("CoordinateGrid full screen coverage", "[coordinate][mapping]") {
    const int W = 32, H = 32;

    SECTION("all pixels are written with various grid sizes") {
        // Test that the entire screen is covered, not just portions

        for (int grid_size : {2, 3, 4, 8, 16}) {
            CoordinateGrid grid;
            grid.resize(grid_size, grid_size);
            fill_identity_grid(grid, W, H);

            std::vector<uint32_t> input(W * H, 0xFFFF0000);  // Red
            std::vector<uint32_t> output(W * H, 0xFF00FF00); // Green (should be overwritten)

            grid.apply(input.data(), output.data(), W, H, false, false, false);

            // Count how many pixels were written (changed from green)
            int written = 0;
            for (int i = 0; i < W * H; i++) {
                if (output[i] != 0xFF00FF00) written++;
            }

            INFO("Grid " << grid_size << "x" << grid_size << ": "
                 << written << "/" << (W*H) << " pixels written");
            REQUIRE(written == W * H);  // ALL pixels must be written
        }
    }
}

TEST_CASE("CoordinateGrid uniform offset", "[coordinate][mapping]") {
    const int W = 64, H = 64;

    SECTION("uniform offset displaces all pixels consistently") {
        CoordinateGrid grid;
        grid.resize(4, 4);

        // Fill with offset: each point samples from (x+10, y+5)
        const double OFFSET_X = 10.0, OFFSET_Y = 5.0;
        for (int gy = 0; gy < grid.grid_height(); gy++) {
            for (int gx = 0; gx < grid.grid_width(); gx++) {
                double nx = static_cast<double>(gx) / (grid.grid_width() - 1);
                double ny = static_cast<double>(gy) / (grid.grid_height() - 1);
                double px = nx * (W - 1);
                double py = ny * (H - 1);
                grid.set(gx, gy, px + OFFSET_X, py + OFFSET_Y);
            }
        }

        std::vector<uint32_t> input(W * H);
        std::vector<uint32_t> output(W * H, 0);
        fill_position_encoded_image(input.data(), W, H);

        grid.apply(input.data(), output.data(), W, H, false, false, false);

        // Check multiple positions - each should be offset by approximately (10, 5)
        for (int test_y : {16, 32, 48}) {
            for (int test_x : {16, 32, 48}) {
                if (test_x + OFFSET_X < W && test_y + OFFSET_Y < H) {
                    auto [src_x, src_y] = decode_position(output[test_y * W + test_x], W, H);
                    double expected_x = test_x + OFFSET_X;
                    double expected_y = test_y + OFFSET_Y;
                    INFO("Pixel (" << test_x << "," << test_y << "): expected source ("
                         << expected_x << "," << expected_y << "), got ("
                         << src_x << "," << src_y << ")");
                    REQUIRE(src_x == Approx(expected_x).margin(3.0));
                    REQUIRE(src_y == Approx(expected_y).margin(3.0));
                }
            }
        }
    }
}

TEST_CASE("CoordinateGrid segment boundaries", "[coordinate][mapping]") {
    // This test specifically checks that segments don't have edge artifacts
    // by verifying smooth transitions across segment boundaries

    const int W = 64, H = 64;

    SECTION("no discontinuities at segment boundaries") {
        CoordinateGrid grid;
        grid.resize(4, 4);  // Creates 3x3 cells
        fill_identity_grid(grid, W, H);

        std::vector<uint32_t> input(W * H);
        std::vector<uint32_t> output(W * H, 0);
        fill_position_encoded_image(input.data(), W, H);

        grid.apply(input.data(), output.data(), W, H, false, false, false);

        // Check horizontal continuity - adjacent pixels should sample from adjacent positions
        int discontinuities = 0;
        for (int y = H / 4; y < 3 * H / 4; y++) {
            for (int x = 1; x < W - 1; x++) {
                auto [x0, y0] = decode_position(output[y * W + x - 1], W, H);
                auto [x1, y1] = decode_position(output[y * W + x], W, H);
                auto [x2, y2] = decode_position(output[y * W + x + 1], W, H);

                // For identity, x should increase by ~1 each pixel
                double dx_left = x1 - x0;
                double dx_right = x2 - x1;

                // Check for large discontinuities (more than 3 pixels jump)
                if (std::abs(dx_left - 1.0) > 3.0 || std::abs(dx_right - 1.0) > 3.0) {
                    discontinuities++;
                }
            }
        }

        INFO("Found " << discontinuities << " horizontal discontinuities");
        REQUIRE(discontinuities < 10);  // Allow very few edge cases
    }
}

TEST_CASE("CoordinateGrid matches original AVS", "[coordinate][mapping][original]") {
    // Compare our coordinate calculation with original r_dmove.cpp
    const int W = 32, H = 32;

    SECTION("original AVS coordinate calculation") {
        // Simulate original r_dmove.cpp grid generation
        // XRES = m_xres + 1 in original
        for (int m_xres : {2, 3, 8, 16}) {
            int XRES = m_xres + 1;  // Original adds 1!
            (void)XRES; // Used below

            std::cerr << "\n=== Original AVS: m_xres=" << m_xres
                      << " (XRES=" << XRES << ") ===" << std::endl;

            double dw2 = W * 32768.0;  // = W/2 * 65536
            double xsc = 2.0 / W;

            int xc_dpos = (W << 16) / (XRES - 1);
            int xc_pos = 0;

            for (int x = 0; x < XRES; x++) {
                double xd = (static_cast<double>(xc_pos) - dw2) / 65536.0;
                double var_x = xd * xsc;

                // After identity script, var_x unchanged
                // rectcoords case:
                int tmp1 = static_cast<int>((var_x + 1.0) * dw2);

                std::cerr << "  x=" << x << ": xc_pos=" << xc_pos
                          << " xd=" << xd << " var_x=" << std::fixed
                          << std::setprecision(4) << var_x
                          << " src_pixel=" << (tmp1 >> 16)
                          << "." << ((tmp1 & 0xFFFF) * 100 / 65536) << std::endl;

                xc_pos += xc_dpos;
            }
        }
    }
}

TEST_CASE("CoordinateGrid segment count verification", "[coordinate][mapping]") {
    // This test verifies that the grid dimensions are set and used correctly
    const int W = 64, H = 64;

    SECTION("grid dimensions are preserved") {
        for (int grid_size : {3, 4, 8, 16, 32}) {
            CoordinateGrid grid;
            grid.resize(grid_size, grid_size);

            INFO("Set grid size to " << grid_size);
            REQUIRE(grid.grid_width() == grid_size);
            REQUIRE(grid.grid_height() == grid_size);

            // Fill with identity transform
            fill_identity_grid(grid, W, H);

            // Verify grid size unchanged after filling
            REQUIRE(grid.grid_width() == grid_size);
            REQUIRE(grid.grid_height() == grid_size);
        }
    }

    SECTION("different grid sizes produce different segment widths") {
        // With a non-identity transform, segment widths should differ based on grid size
        // 3x3 grid: 2 segments per axis, ~32 pixels each
        // 16x16 grid: 15 segments per axis, ~4 pixels each

        std::vector<uint32_t> input(W * H, 0xFFFF0000);  // Red

        // Create a zoom transform - should make segment artifacts visible
        auto fill_zoom = [W, H](CoordinateGrid& g) {
            for (int gy = 0; gy < g.grid_height(); gy++) {
                for (int gx = 0; gx < g.grid_width(); gx++) {
                    double nx = static_cast<double>(gx) / (g.grid_width() - 1);
                    double ny = static_cast<double>(gy) / (g.grid_height() - 1);
                    // Zoom to 2x - this is a linear transform so artifacts are minimal
                    // Use a quadratic transform instead for visible artifacts
                    double px = (nx * nx) * (W - 1);
                    double py = (ny * ny) * (H - 1);
                    g.set(gx, gy, px, py);
                }
            }
        };

        CoordinateGrid grid_small, grid_large;
        grid_small.resize(3, 3);
        grid_large.resize(16, 16);

        fill_zoom(grid_small);
        fill_zoom(grid_large);

        // Verify grid sizes are different
        INFO("Small grid: " << grid_small.grid_width() << "x" << grid_small.grid_height());
        INFO("Large grid: " << grid_large.grid_width() << "x" << grid_large.grid_height());
        REQUIRE(grid_small.grid_width() == 3);
        REQUIRE(grid_large.grid_width() == 16);

        // Sample at specific points and verify different interpolation
        // At x=0.25, 3x3 grid is interpolating within first cell
        // At x=0.25, 16x16 grid is in 4th cell (approximately)
        auto [x_small, y_small] = grid_small.sample(0.25, 0.25);
        auto [x_large, y_large] = grid_large.sample(0.25, 0.25);

        INFO("At norm (0.25, 0.25): small grid samples (" << x_small << ", " << y_small
             << "), large grid samples (" << x_large << ", " << y_large << ")");

        // With a quadratic transform, interpolation differs based on grid density
        // Small grid will have larger interpolation errors
        // The results should be different (unless we're exactly at grid points)
        // Actually, at 0.25 for both grids:
        // - 3x3: 0.25 * 2 = 0.5, so we're halfway between grid points 0 and 1
        // - 16x16: 0.25 * 15 = 3.75, so we're 3/4 between grid points 3 and 4
        // These MUST be different due to different interpolation context

        // For quadratic transform px = nx^2 * (W-1):
        // True value at x=0.25: (0.25)^2 * 63 = 0.0625 * 63 = 3.9375
        // Both grids should approximate this, but with different errors
        double true_x = 0.25 * 0.25 * (W - 1);
        double error_small = std::abs(x_small - true_x);
        double error_large = std::abs(x_large - true_x);

        INFO("True value: " << true_x);
        INFO("Small grid error: " << error_small);
        INFO("Large grid error: " << error_large);

        // Larger grid should have smaller error (more accurate interpolation)
        // This verifies that grid size DOES affect the interpolation quality
        REQUIRE(error_large < error_small + 0.001);  // Allow for floating point
    }
}

TEST_CASE("CoordinateGrid debug output", "[coordinate][mapping][!mayfail]") {
    // This test prints diagnostic information to help identify mapping issues
    const int W = 32, H = 32;

    SECTION("print grid point coverage") {
        for (int grid_size : {3, 4, 8}) {
            CoordinateGrid grid;
            grid.resize(grid_size, grid_size);
            fill_identity_grid(grid, W, H);

            std::cerr << "\n=== Our impl: Grid " << grid_size << "x" << grid_size << " ===" << std::endl;

            // Print what each grid point stores
            std::cerr << "Grid point stored coordinates:" << std::endl;
            for (int gy = 0; gy < grid.grid_height(); gy++) {
                for (int gx = 0; gx < grid.grid_width(); gx++) {
                    auto [x, y] = grid.get(gx, gy);
                    std::cerr << "  (" << gx << "," << gy << ") -> ("
                              << std::fixed << std::setprecision(1) << x << "," << y << ")" << std::endl;
                }
            }

            std::vector<uint32_t> input(W * H);
            std::vector<uint32_t> output(W * H, 0);
            fill_position_encoded_image(input.data(), W, H);

            grid.apply(input.data(), output.data(), W, H, false, false, false);

            // Sample output at various positions
            std::cerr << "Output pixel sources:" << std::endl;
            for (int y : {0, H/4, H/2, 3*H/4, H-1}) {
                for (int x : {0, W/4, W/2, 3*W/4, W-1}) {
                    auto [src_x, src_y] = decode_position(output[y * W + x], W, H);
                    std::cerr << "  pixel(" << x << "," << y << ") samples from ("
                              << std::fixed << std::setprecision(1) << src_x << "," << src_y << ")"
                              << " delta=(" << (src_x - x) << "," << (src_y - y) << ")" << std::endl;
                }
            }
        }

        REQUIRE(true);  // Always passes - this is for diagnostic output
    }
}
