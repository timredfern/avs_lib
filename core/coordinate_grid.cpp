// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#include "coordinate_grid.h"
#include "parallel.h"
#include "script/script_engine.h"
#include <cmath>
#include <algorithm>

namespace avs {

CoordinateGrid::CoordinateGrid()
    : grid_width_(0), grid_height_(0), output_width_(0), output_height_(0)
{
}

void CoordinateGrid::resize(int grid_width, int grid_height)
{
    grid_width_ = grid_width;
    grid_height_ = grid_height;
    grid_.resize(grid_width * grid_height, {0, 0});
}

void CoordinateGrid::set(int gx, int gy, double src_x, double src_y)
{
    if (gx < 0 || gx >= grid_width_ || gy < 0 || gy >= grid_height_) {
        return;
    }
    grid_[gy * grid_width_ + gx] = {to_fixed(src_x), to_fixed(src_y)};
}

std::pair<double, double> CoordinateGrid::get(int gx, int gy) const
{
    if (gx < 0 || gx >= grid_width_ || gy < 0 || gy >= grid_height_) {
        return {0.0, 0.0};
    }
    auto [x_fixed, y_fixed] = grid_[gy * grid_width_ + gx];
    return {from_fixed(x_fixed), from_fixed(y_fixed)};
}

std::pair<double, double> CoordinateGrid::sample(double norm_x, double norm_y) const
{
    if (grid_.empty() || grid_width_ < 2 || grid_height_ < 2) {
        return {0.0, 0.0};
    }

    // Convert normalized [0,1] to grid coordinates
    double grid_x = norm_x * (grid_width_ - 1);
    double grid_y = norm_y * (grid_height_ - 1);

    // Get integer grid cell and fractional part
    int gx = static_cast<int>(grid_x);
    int gy = static_cast<int>(grid_y);

    // Handle boundary cases
    if (gx >= grid_width_ - 1) {
        gx = grid_width_ - 2;
        grid_x = grid_width_ - 1;
    }
    if (gy >= grid_height_ - 1) {
        gy = grid_height_ - 2;
        grid_y = grid_height_ - 1;
    }
    gx = std::max(0, gx);
    gy = std::max(0, gy);

    // Fixed-point fractional part (8.8 format for interpolation)
    int fx = static_cast<int>((grid_x - gx) * 256);
    int fy = static_cast<int>((grid_y - gy) * 256);

    // Bilinear interpolation of grid points
    auto [x_fixed, y_fixed] = interpolate_grid(gx, gy, fx, fy);

    return {from_fixed(x_fixed), from_fixed(y_fixed)};
}

std::pair<int32_t, int32_t> CoordinateGrid::interpolate_grid(int gx, int gy, int fx, int fy) const
{
    // Get four surrounding grid points
    auto [x00, y00] = grid_[gy * grid_width_ + gx];
    auto [x01, y01] = grid_[gy * grid_width_ + gx + 1];
    auto [x10, y10] = grid_[(gy + 1) * grid_width_ + gx];
    auto [x11, y11] = grid_[(gy + 1) * grid_width_ + gx + 1];

    int fx_inv = 256 - fx;
    int fy_inv = 256 - fy;

    // Bilinear interpolation (matching original AVS fixed-point math)
    // Top edge: interpolate between (0,0) and (1,0)
    int64_t x_top = (static_cast<int64_t>(x00) * fx_inv + static_cast<int64_t>(x01) * fx) >> 8;
    int64_t y_top = (static_cast<int64_t>(y00) * fx_inv + static_cast<int64_t>(y01) * fx) >> 8;

    // Bottom edge: interpolate between (0,1) and (1,1)
    int64_t x_bot = (static_cast<int64_t>(x10) * fx_inv + static_cast<int64_t>(x11) * fx) >> 8;
    int64_t y_bot = (static_cast<int64_t>(y10) * fx_inv + static_cast<int64_t>(y11) * fx) >> 8;

    // Final: interpolate between top and bottom
    int32_t x_result = static_cast<int32_t>((x_top * fy_inv + x_bot * fy) >> 8);
    int32_t y_result = static_cast<int32_t>((y_top * fy_inv + y_bot * fy) >> 8);

    return {x_result, y_result};
}

void CoordinateGrid::generate(int grid_intervals_x, int grid_intervals_y,
                              int output_width, int output_height,
                              const std::string& script,
                              CoordMode mode,
                              AudioData audio)
{
    // Create a temporary engine for standalone generation
    ScriptEngine engine;
    engine.set_audio_context(audio, false);
    generate(engine, grid_intervals_x, grid_intervals_y, output_width, output_height, script, mode, audio);
}

void CoordinateGrid::generate(ScriptEngine& engine,
                              int grid_intervals_x, int grid_intervals_y,
                              int output_width, int output_height,
                              const std::string& script,
                              CoordMode mode,
                              AudioData audio)
{
    // Original AVS: XRES = m_xres + 1 (user specifies intervals, we need points)
    // grid_intervals_x/y is the number of cells, grid points = cells + 1
    int grid_width = grid_intervals_x + 1;
    int grid_height = grid_intervals_y + 1;

    resize(grid_width, grid_height);
    output_width_ = output_width;
    output_height_ = output_height;

    // Update audio context (engine may already have variables set)
    engine.set_audio_context(audio, false);

    // Ensure variables exist before compiling
    double& var_x = engine.var_ref("x");
    double& var_y = engine.var_ref("y");
    double& var_d = engine.var_ref("d");
    double& var_r = engine.var_ref("r");

    // Compile script once, execute many times (major performance win!)
    CompiledScript compiled = engine.compile(script);

    // Calculate max distance for polar mode (diagonal/2, matching original AVS)
    double max_d = std::sqrt(static_cast<double>(output_width * output_width +
                                                  output_height * output_height)) * 0.5;
    double inv_max_d = 1.0 / max_d;

    double dw2 = output_width * 0.5;
    double dh2 = output_height * 0.5;

    for (int gy = 0; gy < grid_height; gy++) {
        for (int gx = 0; gx < grid_width; gx++) {
            // Convert grid coordinates to pixel coordinates
            double pixel_x = (gx * (output_width - 1.0)) / (grid_width - 1);
            double pixel_y = (gy * (output_height - 1.0)) / (grid_height - 1);

            // Set pixel context
            engine.set_pixel_context(static_cast<int>(pixel_x), static_cast<int>(pixel_y),
                                     output_width, output_height);

            double src_x, src_y;

            if (mode == CoordMode::RECTANGULAR) {
                // Rectangular mode: x, y in [-1, 1]
                double x = (pixel_x - dw2) * 2.0 / output_width;
                double y = (pixel_y - dh2) * 2.0 / output_height;

                // Set variables via direct reference access
                var_x = x;
                var_y = y;

                // Execute script
                engine.execute(compiled);

                // Read back modified values
                double new_x = var_x;
                double new_y = var_y;

                // Handle NaN/inf
                if (!std::isfinite(new_x)) new_x = x;
                if (!std::isfinite(new_y)) new_y = y;

                // Convert back to pixel coordinates
                src_x = (new_x + 1.0) * dw2;
                src_y = (new_y + 1.0) * dh2;

            } else {
                // Polar mode: d (distance), r (angle)
                double centered_x = pixel_x - dw2;
                double centered_y = pixel_y - dh2;

                // Calculate polar coordinates matching original AVS
                double x = centered_x * 2.0 / output_width;
                double y = centered_y * 2.0 / output_height;
                double d = std::sqrt(centered_x * centered_x + centered_y * centered_y) * inv_max_d;
                double r = std::atan2(centered_y, centered_x) + M_PI * 0.5;

                // Set variables via direct reference access
                var_x = x;
                var_y = y;
                var_d = d;
                var_r = r;

                // Execute script
                engine.execute(compiled);

                // Read back modified values
                double new_d = var_d;
                double new_r = var_r;

                // Handle NaN/inf
                if (!std::isfinite(new_d)) new_d = d;
                if (!std::isfinite(new_r)) new_r = r;

                // Convert back to cartesian: remove PI/2 offset, scale d back up
                new_r -= M_PI * 0.5;
                src_x = dw2 + std::cos(new_r) * new_d * max_d;
                src_y = dh2 + std::sin(new_r) * new_d * max_d;
            }

            // Store source coordinates
            set(gx, gy, src_x, src_y);
        }
    }
}

void CoordinateGrid::apply(const uint32_t* input, uint32_t* output,
                           int width, int height,
                           bool subpixel, bool wrap, bool blend,
                           bool precise, bool parallel) const
{
    if (grid_.empty() || grid_width_ < 2 || grid_height_ < 2) {
        std::copy(input, input + width * height, output);
        return;
    }

    // Precise mode: per-pixel bilinear interpolation (slower but different character)
    if (precise) {
        parallel_for_rows(height, [&](int y_start, int y_end) {
            for (int dest_y = y_start; dest_y < y_end; dest_y++) {
                for (int dest_x = 0; dest_x < width; dest_x++) {
                    // Normalized position [0, 1]
                    double nx = static_cast<double>(dest_x) / (width - 1);
                    double ny = static_cast<double>(dest_y) / (height - 1);

                    // Grid cell coordinates
                    double gx_f = nx * (grid_width_ - 1);
                    double gy_f = ny * (grid_height_ - 1);

                    int gx = static_cast<int>(gx_f);
                    int gy = static_cast<int>(gy_f);

                    // Clamp to valid grid cell
                    if (gx >= grid_width_ - 1) gx = grid_width_ - 2;
                    if (gy >= grid_height_ - 1) gy = grid_height_ - 2;

                    // Fractional position within cell [0, 1]
                    double fx = gx_f - gx;
                    double fy = gy_f - gy;

                    // Get four corner source coordinates (stored as 16.16 fixed-point)
                    auto [x00_fp, y00_fp] = grid_[gy * grid_width_ + gx];
                    auto [x10_fp, y10_fp] = grid_[gy * grid_width_ + gx + 1];
                    auto [x01_fp, y01_fp] = grid_[(gy + 1) * grid_width_ + gx];
                    auto [x11_fp, y11_fp] = grid_[(gy + 1) * grid_width_ + gx + 1];

                    // Convert fixed-point to double for interpolation
                    double x00 = from_fixed(x00_fp), y00 = from_fixed(y00_fp);
                    double x10 = from_fixed(x10_fp), y10 = from_fixed(y10_fp);
                    double x01 = from_fixed(x01_fp), y01 = from_fixed(y01_fp);
                    double x11 = from_fixed(x11_fp), y11 = from_fixed(y11_fp);

                    // Bilinear interpolation of source coordinates
                    double src_x = (1 - fx) * (1 - fy) * x00 +
                                   fx * (1 - fy) * x10 +
                                   (1 - fx) * fy * x01 +
                                   fx * fy * x11;
                    double src_y = (1 - fx) * (1 - fy) * y00 +
                                   fx * (1 - fy) * y10 +
                                   (1 - fx) * fy * y01 +
                                   fx * fy * y11;

                    // Convert back to fixed point for sampling
                    int32_t xp = to_fixed(src_x);
                    int32_t yp = to_fixed(src_y);

                    uint32_t pixel = sample_pixel(input, width, height, xp, yp, subpixel, wrap);
                    int dest_idx = dest_y * width + dest_x;
                    if (blend) {
                        output[dest_idx] = blend_max(pixel, output[dest_idx]);
                    } else {
                        output[dest_idx] = pixel;
                    }
                }
            }
        });
        return;
    }

    const int XRES = grid_width_;
    const int YRES = grid_height_;

    if (parallel) {
        // Parallel stepping algorithm
        // Each row independently computes its grid interpolation, then steps horizontally.
        // No row-to-row dependency allows parallel execution.

        // Precompute output X position for each grid column
        std::vector<int> gx_to_x(XRES);
        for (int gx = 0; gx < XRES; gx++) {
            gx_to_x[gx] = (gx * (width - 1)) / (XRES - 1);
        }

        parallel_for_rows(height, [&](int y_start, int y_end) {
            for (int out_y = y_start; out_y < y_end; out_y++) {
                // Determine grid row and interpolation factor (8-bit fixed point)
                int gy_scaled = (out_y * ((YRES - 1) << 8)) / (height - 1);
                int gy = gy_scaled >> 8;
                int fy = gy_scaled & 0xFF;

                // Clamp to valid grid interval, preserving full interpolation at boundary
                if (gy >= YRES - 1) {
                    gy = YRES - 2;
                    fy = 256;
                }
                int fy_inv = 256 - fy;

                const auto* row_top = &grid_[gy * XRES];
                const auto* row_bot = &grid_[(gy + 1) * XRES];

                uint32_t* out_row = output + out_y * width;

                // Process each grid column segment
                int out_x = 0;
                for (int gx = 0; gx < XRES - 1 && out_x < width; gx++) {
                    // Interpolate grid column positions for this row
                    int32_t xp_left = (static_cast<int64_t>(row_top[gx].first) * fy_inv +
                                       static_cast<int64_t>(row_bot[gx].first) * fy) >> 8;
                    int32_t yp_left = (static_cast<int64_t>(row_top[gx].second) * fy_inv +
                                       static_cast<int64_t>(row_bot[gx].second) * fy) >> 8;
                    int32_t xp_right = (static_cast<int64_t>(row_top[gx + 1].first) * fy_inv +
                                        static_cast<int64_t>(row_bot[gx + 1].first) * fy) >> 8;
                    int32_t yp_right = (static_cast<int64_t>(row_top[gx + 1].second) * fy_inv +
                                        static_cast<int64_t>(row_bot[gx + 1].second) * fy) >> 8;

                    // Calculate segment width
                    int x_end = (gx == XRES - 2) ? width : gx_to_x[gx + 1];
                    int seg_width = x_end - out_x;
                    if (seg_width < 1) seg_width = 1;

                    // Calculate horizontal deltas
                    int32_t d_x = (xp_right - xp_left) / seg_width;
                    int32_t d_y = (yp_right - yp_left) / seg_width;

                    int32_t xp = xp_left;
                    int32_t yp = yp_left;

                    // Process pixels in this segment
                    for (; out_x < x_end; out_x++) {
                        uint32_t pixel = sample_pixel(input, width, height, xp, yp, subpixel, wrap);

                        if (blend) {
                            out_row[out_x] = blend_max(pixel, out_row[out_x]);
                        } else {
                            out_row[out_x] = pixel;
                        }

                        xp += d_x;
                        yp += d_y;
                    }
                }
            }
        });
    } else {
        // Sequential stepping algorithm (original AVS port from r_dmove.cpp)
        // Instead of computing bilinear interpolation per-pixel, we:
        // 1. For each grid row segment: compute vertical step deltas
        // 2. For each output row: step the values down
        // 3. For each grid column segment: compute horizontal step deltas
        // 4. For each pixel: just add the delta (no recalculation)
        //
        // This is mathematically equivalent to bilinear interpolation but faster
        // because we only do addition per-pixel instead of 8 multiplies.

        // Stepping values for each grid column (x position, y position, x delta, y delta)
        // These are stepped down as we move through output rows
        struct ColStep {
            int32_t xp, yp;   // Current interpolated position (16.16 fixed)
            int32_t dx, dy;   // Delta per output row (16.16 fixed)
        };
        std::vector<ColStep> col_steps(XRES);

        // Process grid row by row
        int yc_pos = 0;
        int yc_dpos = (height << 16) / (YRES - 1);  // Fixed-point step per grid row
        int rows_remaining = height;

        for (int gy = 0; gy < YRES - 1 && rows_remaining > 0; gy++) {
            // Calculate how many output rows in this grid segment
            int next_yc = yc_pos + yc_dpos;
            int yseek = (next_yc >> 16) - ((yc_pos) >> 16);
            if (yseek < 1) yseek = 1;
            // Ensure last segment covers remaining rows
            if (gy == YRES - 2 || yseek > rows_remaining) {
                yseek = rows_remaining;
            }

            // Setup column stepping for this grid row segment
            const auto* row_top = &grid_[gy * XRES];
            const auto* row_bot = &grid_[(gy + 1) * XRES];

            for (int gx = 0; gx < XRES; gx++) {
                col_steps[gx].xp = row_top[gx].first;   // Start at top row
                col_steps[gx].yp = row_top[gx].second;
                col_steps[gx].dx = (row_bot[gx].first - row_top[gx].first) / yseek;
                col_steps[gx].dy = (row_bot[gx].second - row_top[gx].second) / yseek;
            }

            // Process each output row in this grid segment
            int out_y = height - rows_remaining;
            for (int row_in_seg = 0; row_in_seg < yseek; row_in_seg++, out_y++, rows_remaining--) {
                uint32_t* out_row = output + out_y * width;
                const uint32_t* blend_row = blend ? (output + out_y * width) : nullptr;

                // Process grid column by column
                int xc_pos = 0;
                int xc_dpos = (width << 16) / (XRES - 1);
                int cols_remaining = width;

                for (int gx = 0; gx < XRES - 1 && cols_remaining > 0; gx++) {
                    // Calculate how many output pixels in this grid segment
                    int next_xc = xc_pos + xc_dpos;
                    int xseek = (next_xc >> 16) - (xc_pos >> 16);
                    if (xseek < 1) xseek = 1;
                    // Ensure last segment covers remaining columns
                    if (gx == XRES - 2 || xseek > cols_remaining) {
                        xseek = cols_remaining;
                    }

                    // Get current interpolated positions from column steps
                    int32_t xp = col_steps[gx].xp;
                    int32_t yp = col_steps[gx].yp;

                    // Calculate horizontal deltas to next grid column
                    int32_t d_x = (col_steps[gx + 1].xp - xp) / xseek;
                    int32_t d_y = (col_steps[gx + 1].yp - yp) / xseek;

                    // Step the column values down for next row
                    col_steps[gx].xp += col_steps[gx].dx;
                    col_steps[gx].yp += col_steps[gx].dy;

                    // Process each pixel in this grid segment
                    int out_x = width - cols_remaining;
                    for (int px = 0; px < xseek; px++, out_x++, cols_remaining--) {
                        uint32_t pixel = sample_pixel(input, width, height, xp, yp, subpixel, wrap);

                        if (blend) {
                            out_row[out_x] = blend_max(pixel, blend_row[out_x]);
                        } else {
                            out_row[out_x] = pixel;
                        }

                        xp += d_x;
                        yp += d_y;
                    }

                    xc_pos = next_xc;
                }

                // Step the last column too (needed for next row's rightmost segment)
                col_steps[XRES - 1].xp += col_steps[XRES - 1].dx;
                col_steps[XRES - 1].yp += col_steps[XRES - 1].dy;
            }

            yc_pos = next_yc;
        }
    }
}

uint32_t CoordinateGrid::sample_pixel(const uint32_t* input, int width, int height,
                                       int32_t x_fixed, int32_t y_fixed,
                                       bool subpixel, bool wrap) const
{
    // Maximum coordinate values in 16.16 fixed-point
    // Use (width-1) for subpixel mode to allow interpolation at edges
    int32_t w_max = (width - 1) << 16;
    int32_t h_max = (height - 1) << 16;

    // Handle wrap or clamp
    if (wrap) {
        // Wrap coordinates using width/height (not width-1)
        int32_t w_wrap = width << 16;
        int32_t h_wrap = height << 16;

        // Use modulo-style wrapping
        if (x_fixed < 0) {
            x_fixed = w_wrap - ((-x_fixed) % w_wrap);
            if (x_fixed >= w_wrap) x_fixed = 0;
        } else if (x_fixed >= w_wrap) {
            x_fixed = x_fixed % w_wrap;
        }

        if (y_fixed < 0) {
            y_fixed = h_wrap - ((-y_fixed) % h_wrap);
            if (y_fixed >= h_wrap) y_fixed = 0;
        } else if (y_fixed >= h_wrap) {
            y_fixed = y_fixed % h_wrap;
        }
    } else {
        // Clamp coordinates
        x_fixed = std::clamp(x_fixed, 0, w_max);
        y_fixed = std::clamp(y_fixed, 0, h_max);
    }

    if (subpixel) {
        // Bilinear sampling from source image
        int x0 = x_fixed >> 16;
        int y0 = y_fixed >> 16;
        int fx = (x_fixed >> 8) & 0xFF;  // 8-bit fractional part
        int fy = (y_fixed >> 8) & 0xFF;

        // Clamp to valid range for bilinear (need x0+1, y0+1 to be valid)
        if (x0 >= width - 1) {
            x0 = width - 2;
            fx = 255;
        }
        if (y0 >= height - 1) {
            y0 = height - 2;
            fy = 255;
        }
        if (x0 < 0) {
            x0 = 0;
            fx = 0;
        }
        if (y0 < 0) {
            y0 = 0;
            fy = 0;
        }

        uint32_t p00 = input[y0 * width + x0];
        uint32_t p01 = input[y0 * width + x0 + 1];
        uint32_t p10 = input[(y0 + 1) * width + x0];
        uint32_t p11 = input[(y0 + 1) * width + x0 + 1];

        return blend_pixels(p00, p01, p10, p11, fx, fy);
    } else {
        // Nearest neighbor sampling
        int x = (x_fixed + 0x8000) >> 16;  // Round to nearest
        int y = (y_fixed + 0x8000) >> 16;

        x = std::clamp(x, 0, width - 1);
        y = std::clamp(y, 0, height - 1);

        return input[y * width + x];
    }
}

uint32_t CoordinateGrid::blend_pixels(uint32_t p00, uint32_t p01, uint32_t p10, uint32_t p11,
                                       int fx, int fy) const
{
    int fx_inv = 256 - fx;
    int fy_inv = 256 - fy;

    auto interp_channel = [fx, fy, fx_inv, fy_inv](uint32_t p00, uint32_t p01, uint32_t p10, uint32_t p11, int shift) -> uint32_t {
        int c00 = (p00 >> shift) & 0xFF;
        int c01 = (p01 >> shift) & 0xFF;
        int c10 = (p10 >> shift) & 0xFF;
        int c11 = (p11 >> shift) & 0xFF;

        int c0 = (c00 * fx_inv + c01 * fx) >> 8;
        int c1 = (c10 * fx_inv + c11 * fx) >> 8;
        int result = (c0 * fy_inv + c1 * fy) >> 8;

        return result & 0xFF;
    };

    uint32_t a = interp_channel(p00, p01, p10, p11, 24);
    uint32_t r = interp_channel(p00, p01, p10, p11, 16);
    uint32_t g = interp_channel(p00, p01, p10, p11, 8);
    uint32_t b = interp_channel(p00, p01, p10, p11, 0);

    return (a << 24) | (r << 16) | (g << 8) | b;
}

uint32_t CoordinateGrid::blend_max(uint32_t a, uint32_t b) const
{
    uint32_t r = std::max((a >> 16) & 0xFF, (b >> 16) & 0xFF);
    uint32_t g = std::max((a >> 8) & 0xFF, (b >> 8) & 0xFF);
    uint32_t blue = std::max(a & 0xFF, b & 0xFF);
    uint32_t alpha = std::max((a >> 24) & 0xFF, (b >> 24) & 0xFF);

    return (alpha << 24) | (r << 16) | (g << 8) | blue;
}

} // namespace avs
