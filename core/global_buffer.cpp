// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#include "global_buffer.h"
#include <cstdlib>
#include <cstring>

namespace avs {

static uint32_t* g_buffers[NBUF] = {};
static int g_buffer_w[NBUF] = {};
static int g_buffer_h[NBUF] = {};

uint32_t* get_global_buffer(int w, int h, int n, bool do_alloc) {
    if (n < 0 || n >= NBUF) return nullptr;

    // Check if buffer exists with correct dimensions
    if (g_buffers[n] && g_buffer_w[n] == w && g_buffer_h[n] == h) {
        return g_buffers[n];
    }

    // Buffer doesn't exist or wrong size - free old one if exists
    if (g_buffers[n]) {
        std::free(g_buffers[n]);
        g_buffers[n] = nullptr;
        g_buffer_w[n] = 0;
        g_buffer_h[n] = 0;
    }

    // Allocate new buffer if requested
    if (do_alloc) {
        g_buffers[n] = static_cast<uint32_t*>(std::calloc(w * h, sizeof(uint32_t)));
        if (g_buffers[n]) {
            g_buffer_w[n] = w;
            g_buffer_h[n] = h;
        }
        return g_buffers[n];
    }

    return nullptr;
}

void clear_global_buffers() {
    for (int i = 0; i < NBUF; i++) {
        if (g_buffers[i]) {
            std::free(g_buffers[i]);
            g_buffers[i] = nullptr;
            g_buffer_w[i] = 0;
            g_buffer_h[i] = 0;
        }
    }
}

} // namespace avs
