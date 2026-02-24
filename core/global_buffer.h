// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#pragma once

#include <cstdint>
#include <cstddef>

namespace avs {

// Number of global buffers (matches original AVS NBUF)
constexpr int NBUF = 8;

// Get a global buffer, optionally allocating if needed
// Returns nullptr if buffer doesn't exist and do_alloc is false,
// or if n is out of range
uint32_t* get_global_buffer(int w, int h, int n, bool do_alloc);

// Clear all global buffers (call when switching presets)
void clear_global_buffers();

} // namespace avs
