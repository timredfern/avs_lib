// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#pragma once

#include "effect_base.h"
#include "effect_container.h"
#include "effects/effect_list_root.h"
#include "event_bus.h"
#include <memory>
#include <vector>
#include <cstdint>
#include <cstring>

namespace avs {

// Pixel format for different bit depths
// Field order matches ARGB uint32_t memory layout on little-endian: [B,G,R,A]
// This allows safe reinterpret_cast between ARGBPixel* and uint32_t*
template<typename T>
struct ARGBPixel {
    T b, g, r, a;  // Memory order on little-endian matches ARGB uint32_t

    // Constructor - default to black with opaque alpha
    ARGBPixel() : b(0), g(0), r(0), a(0) {}
    ARGBPixel(T red, T green, T blue, T alpha = 0xFF) : b(blue), g(green), r(red), a(alpha) {}

    // Conversion to uint32_t (ARGB format: 0xAARRGGBB)
    explicit operator uint32_t() const {
        return (static_cast<uint32_t>(a) << 24) |
               (static_cast<uint32_t>(r) << 16) |
               (static_cast<uint32_t>(g) << 8) |
               static_cast<uint32_t>(b);
    }

    // Conversion from uint32_t (ARGB format)
    ARGBPixel(uint32_t argb) {
        a = static_cast<T>((argb >> 24) & 0xFF);
        r = static_cast<T>((argb >> 16) & 0xFF);
        g = static_cast<T>((argb >> 8) & 0xFF);
        b = static_cast<T>(argb & 0xFF);
    }
};

// Standard formats
using ARGB8 = ARGBPixel<uint8_t>;
using ARGB16 = ARGBPixel<uint16_t>;
using ARGBFloat = ARGBPixel<float>;

// Verify struct packing matches uint32_t on this platform
static_assert(sizeof(ARGB8) == sizeof(uint32_t), "ARGB8 must be 4 bytes");

}  // namespace avs

namespace avs {

template<typename PixelType = ARGB8>
class Renderer {
public:
    Renderer(int width, int height);
    ~Renderer();

    // Root effect list access
    EffectListRoot* root() { return root_.get(); }
    const EffectListRoot* root() const { return root_.get(); }

    // Effect chain management - delegates to root
    void add_effect(std::unique_ptr<EffectBase> effect) {
        root_->add_child(std::move(effect));
    }
    void insert_effect(size_t index, std::unique_ptr<EffectBase> effect) {
        root_->insert_child(index, std::move(effect));
    }
    void remove_effect(size_t index) {
        root_->remove_child(index);
    }
    void clear_effects() {
        while (root_->child_count() > 0) {
            root_->remove_child(0);
        }
    }
    size_t effect_count() const { return root_->child_count(); }

    // Access effects for UI/parameter modification - delegates to root
    EffectBase* get_effect(size_t index) {
        return root_->get_child(index);
    }
    const EffectBase* get_effect(size_t index) const {
        return root_->get_child(index);
    }

    // Swap two effects in the chain - delegates to root
    void swap_effects(size_t index_a, size_t index_b) {
        root_->swap_children(index_a, index_b);
    }

    // Main render call - output to pixel buffer
    void render(AudioData visdata, bool is_beat, PixelType* output_buffer);

    // Render to uint32_t buffer (ARGB format, forces alpha opaque for OF blending)
    void render(AudioData visdata, bool is_beat, uint32_t* output_buffer);

    // Zero-copy render - renders to internal buffer_a, access via get_buffer()
    void render(AudioData visdata, bool is_beat);

    // Access internal buffer as uint32_t* (valid after render)
    uint32_t* get_buffer() { return reinterpret_cast<uint32_t*>(buffer_a_.data()); }

    // Dimensions
    int width() const { return width_; }
    int height() const { return height_; }
    void resize(int width, int height);

private:
    int width_;
    int height_;

    // Double buffering for effect chain
    std::vector<PixelType> buffer_a_;
    std::vector<PixelType> buffer_b_;

    // Root effect list (the "Main" container)
    std::unique_ptr<EffectListRoot> root_;

    // Helper to get buffer pointers
    PixelType* get_buffer_a() { return buffer_a_.data(); }
    PixelType* get_buffer_b() { return buffer_b_.data(); }

    void allocate_buffers();
};

// Template implementation
template<typename PixelType>
Renderer<PixelType>::Renderer(int width, int height)
    : width_(width), height_(height) {
    allocate_buffers();
    root_ = std::make_unique<EffectListRoot>();
}

template<typename PixelType>
Renderer<PixelType>::~Renderer() = default;

template<typename PixelType>
void Renderer<PixelType>::allocate_buffers() {
    size_t buffer_size = width_ * height_;
    buffer_a_.resize(buffer_size);
    buffer_b_.resize(buffer_size);
}

template<typename PixelType>
void Renderer<PixelType>::resize(int width, int height) {
    width_ = width;
    height_ = height;
    allocate_buffers();
}

template<typename PixelType>
void Renderer<PixelType>::render(AudioData visdata, bool is_beat, PixelType* output_buffer) {
    // Process any pending MIDI events before rendering
    EventBus::instance().process_frame();

    if (!root_ || root_->child_count() == 0) {
        return;
    }

    // Don't auto-clear buffer - preserve previous frame for feedback effects
    // The Clear effect or EffectList clear_each_frame will handle clearing when needed

    // Render the root effect list - it handles buffer swapping for its children
    // Result is always in framebuffer (buffer_a) per EffectList convention
    // Safe reinterpret_cast: ARGBPixel memory layout matches ARGB uint32_t on little-endian
    root_->render(visdata, is_beat ? 1 : 0,
                  reinterpret_cast<uint32_t*>(get_buffer_a()),
                  reinterpret_cast<uint32_t*>(get_buffer_b()),
                  width_, height_);

    // Copy final result to output
    std::memcpy(output_buffer, get_buffer_a(), width_ * height_ * sizeof(PixelType));
}

template<typename PixelType>
void Renderer<PixelType>::render(AudioData visdata, bool is_beat) {
    EventBus::instance().process_frame();

    if (!root_ || root_->child_count() == 0) {
        return;
    }

    root_->render(visdata, is_beat ? 1 : 0,
                  reinterpret_cast<uint32_t*>(get_buffer_a()),
                  reinterpret_cast<uint32_t*>(get_buffer_b()),
                  width_, height_);
}

template<typename PixelType>
void Renderer<PixelType>::render(AudioData visdata, bool is_beat, uint32_t* output_buffer) {
    // Render to internal buffer
    render(visdata, is_beat, get_buffer_a());

    // Copy to output, forcing alpha to opaque
    // Original AVS ignored alpha for display (used BitBlt SRCCOPY)
    // We need opaque pixels for OpenFrameworks alpha blending
    for (size_t i = 0; i < buffer_a_.size(); ++i) {
        output_buffer[i] = static_cast<uint32_t>(buffer_a_[i]) | 0xFF000000;
    }
}

// Default to 8-bit ARGB
using DefaultRenderer = Renderer<ARGB8>;

} // namespace avs