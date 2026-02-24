// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#pragma once

#include <cstdint>

namespace avs {

/**
 * Color format utilities for AVS
 *
 * Internal format: ARGB (0xAARRGGBB)
 * - Alpha in bits 24-31
 * - Red in bits 16-23
 * - Green in bits 8-15
 * - Blue in bits 0-7
 *
 * This format is required for OpenFrameworks/OpenGL texture uploads.
 * All colors throughout avs_lib use this format.
 *
 * Legacy format: Windows AVS used ABGR (0xAABBGGRR)
 * Binary .avs preset files store colors in ABGR format.
 * When loading presets, use swap_rb() to convert ABGR → ARGB.
 * This conversion happens ONCE during preset loading - nowhere else.
 */
namespace color {

// Standard colors in internal ARGB format
constexpr uint32_t RED   = 0xFFFF0000;  // A=FF, R=FF, G=00, B=00
constexpr uint32_t GREEN = 0xFF00FF00;  // A=FF, R=00, G=FF, B=00
constexpr uint32_t BLUE  = 0xFF0000FF;  // A=FF, R=00, G=00, B=FF
constexpr uint32_t WHITE = 0xFFFFFFFF;
constexpr uint32_t BLACK = 0xFF000000;

// ============================================================================
// Normalized extraction (0-255 range)
// ============================================================================
//
// USE WHEN: You need actual 0-255 values for comparisons, thresholds,
// external APIs, or when mixing with other color sources.
//
// Example:
//   int r = red(pixel);
//   if (r > 128) { ... }  // Compare against threshold
//   uint32_t out = make(r, g, b);
//
inline uint8_t red(uint32_t color)   { return (color >> 16) & 0xFF; }
inline uint8_t green(uint32_t color) { return (color >> 8) & 0xFF; }
inline uint8_t blue(uint32_t color)  { return color & 0xFF; }
inline uint8_t alpha(uint32_t color) { return (color >> 24) & 0xFF; }

// Build internal ARGB color from normalized RGB(A) components (0-255 each)
inline uint32_t make(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return (static_cast<uint32_t>(a) << 24) |
           (static_cast<uint32_t>(r) << 16) |
           (static_cast<uint32_t>(g) << 8) |
           static_cast<uint32_t>(b);
}

// ============================================================================
// In-place extraction (channels stay in native bit positions)
// ============================================================================
//
// USE WHEN: Performance-critical inner loops doing arithmetic on channels.
// No shifts on extract or reconstruct - 6 fewer shifts per pixel.
//
// Channels keep their bit positions:
//   red_i   -> 0x00RR0000 (bits 16-23, range 0 to 0xFF0000)
//   green_i -> 0x0000GG00 (bits 8-15,  range 0 to 0xFF00)
//   blue_i  -> 0x000000BB (bits 0-7,   range 0 to 0xFF)
//
// Arithmetic works the same regardless of bit position:
//   r + r = correct (just at higher magnitude)
//   r / 2 = correct
//
// Example:
//   int r = red_i(pixel);
//   int g = green_i(pixel);
//   int b = blue_i(pixel);
//   r = (r + red_i(other)) / 2;  // Average, no shifts needed
//   r = std::clamp(r, 0, 0xFF0000);  // Clamp to valid range
//   uint32_t out = make_i(r, g, b);
//
// CLAMP RANGES (use these constants):
constexpr int RED_MAX_I   = 0xFF0000;  // 255 << 16
constexpr int GREEN_MAX_I = 0xFF00;    // 255 << 8
constexpr int BLUE_MAX_I  = 0xFF;      // 255

inline int red_i(uint32_t color)   { return color & 0xFF0000; }
inline int green_i(uint32_t color) { return color & 0xFF00; }
inline int blue_i(uint32_t color)  { return color & 0xFF; }

// Build ARGB from in-place channel values (already in correct bit positions)
inline uint32_t make_i(int r, int g, int b) {
    return (r & 0xFF0000) | (g & 0xFF00) | (b & 0xFF);
}

// ============================================================================
// Format conversions
// ============================================================================

/**
 * Swap Red and Blue channels in a color value.
 *
 * Use when loading colors from binary AVS presets:
 *   - Legacy format: ABGR (0xAABBGGRR) - R in bits 0-7, B in bits 16-23
 *   - Internal format: ARGB (0xAARRGGBB) - B in bits 0-7, R in bits 16-23
 *
 * This is the ONE conversion that happens during preset loading.
 * After this, all colors are ARGB - no further conversions needed.
 *
 * Note: Alpha is preserved, Green is unchanged.
 */
inline uint32_t swap_rb(uint32_t color) {
    uint32_t a = (color >> 24) & 0xFF;
    uint32_t x = (color >> 16) & 0xFF;  // R in ARGB, B in ABGR
    uint32_t g = (color >> 8) & 0xFF;
    uint32_t y = color & 0xFF;          // B in ARGB, R in ABGR
    return (a << 24) | (y << 16) | (g << 8) | x;
}

// Legacy aliases for compatibility - these are the same operation
inline uint32_t abgr_to_argb(uint32_t abgr) { return swap_rb(abgr); }
inline uint32_t argb_to_abgr(uint32_t argb) { return swap_rb(argb); }

// Deprecated - use swap_rb() instead
inline uint32_t bgra_to_rgba(uint32_t color) { return swap_rb(color) | 0xFF000000; }

} // namespace color
} // namespace avs
