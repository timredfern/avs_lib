// avs_lib - Portable Advanced Visualization Studio library
// Copyright (C) 2025 Tim Redfern
// Licensed under MIT License
//
// avs_render_test - Render a single frame from a preset for validation

#include "core/preset.h"
#include "core/renderer.h"
#include "core/builtin_effects.h"
#include "effects/effect_list.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <vector>

// ============================================================================
// FFT implementation from grandchild (3rdparty/md_fft.cpp)
// Copyright 2005-2013 Nullsoft, Inc. - BSD license
// ============================================================================

#define FFT_PI 3.141592653589793238462643383279502884197169399f

class FFT {
public:
    FFT() : m_ready(0), m_samples_in(0), NFREQ(0),
            envelope(nullptr), equalize(nullptr), bitrevtable(nullptr),
            cossintable(nullptr), temp1(nullptr), temp2(nullptr) {}

    ~FFT() { CleanUp(); }

    void Init(int samples_in, int samples_out, int bEqualize = 1, float envelope_power = 1.0f) {
        CleanUp();
        m_samples_in = samples_in;
        NFREQ = samples_out * 2;

        InitBitRevTable();
        InitCosSinTable();
        if (envelope_power > 0) InitEnvelopeTable(envelope_power);
        if (bEqualize) InitEqualizeTable();
        temp1 = new float[NFREQ];
        temp2 = new float[NFREQ];
    }

    void CleanUp() {
        delete[] envelope; envelope = nullptr;
        delete[] equalize; equalize = nullptr;
        delete[] bitrevtable; bitrevtable = nullptr;
        delete[] cossintable; cossintable = nullptr;
        delete[] temp1; temp1 = nullptr;
        delete[] temp2; temp2 = nullptr;
    }

    void time_to_frequency_domain(float* in_wavedata, float* out_spectraldata) {
        if (!bitrevtable || !temp1 || !temp2 || !cossintable) return;

        // 1. set up input to the fft
        if (envelope) {
            for (int i = 0; i < NFREQ; i++) {
                int idx = bitrevtable[i];
                if (idx < m_samples_in)
                    temp1[i] = in_wavedata[idx] * envelope[idx];
                else
                    temp1[i] = 0;
            }
        } else {
            for (int i = 0; i < NFREQ; i++) {
                int idx = bitrevtable[i];
                if (idx < m_samples_in)
                    temp1[i] = in_wavedata[idx];
                else
                    temp1[i] = 0;
            }
        }
        memset(temp2, 0, sizeof(float) * NFREQ);

        // 2. perform FFT
        float* real = temp1;
        float* imag = temp2;
        int dftsize = 2;
        int t = 0;
        while (dftsize <= NFREQ) {
            float wpr = cossintable[t][0];
            float wpi = cossintable[t][1];
            float wr = 1.0f;
            float wi = 0.0f;
            int hdftsize = dftsize >> 1;

            for (int m = 0; m < hdftsize; m += 1) {
                for (int i = m; i < NFREQ; i += dftsize) {
                    int j = i + hdftsize;
                    float tempr = wr * real[j] - wi * imag[j];
                    float tempi = wr * imag[j] + wi * real[j];
                    real[j] = real[i] - tempr;
                    imag[j] = imag[i] - tempi;
                    real[i] += tempr;
                    imag[i] += tempi;
                }
                float wtemp = wr;
                wr = wtemp * wpr - wi * wpi;
                wi = wi * wpr + wtemp * wpi;
            }
            dftsize <<= 1;
            ++t;
        }

        // 3. take the magnitude & equalize it for output
        if (equalize)
            for (int i = 0; i < NFREQ / 2; i++)
                out_spectraldata[i] = equalize[i] * sqrtf(temp1[i] * temp1[i] + temp2[i] * temp2[i]);
        else
            for (int i = 0; i < NFREQ / 2; i++)
                out_spectraldata[i] = sqrtf(temp1[i] * temp1[i] + temp2[i] * temp2[i]);
    }

private:
    int m_ready;
    int m_samples_in;
    int NFREQ;
    float* envelope;
    float* equalize;
    int* bitrevtable;
    float (*cossintable)[2];
    float* temp1;
    float* temp2;

    void InitEnvelopeTable(float power) {
        float mult = 1.0f / (float)m_samples_in * 6.2831853f;
        envelope = new float[m_samples_in];
        if (power == 1.0f)
            for (int i = 0; i < m_samples_in; i++)
                envelope[i] = 0.5f + 0.5f * sinf(i * mult - 1.5707963268f);
        else
            for (int i = 0; i < m_samples_in; i++)
                envelope[i] = powf(0.5f + 0.5f * sinf(i * mult - 1.5707963268f), power);
    }

    void InitEqualizeTable() {
        float inv_half_nfreq = 1.0f / (float)(NFREQ / 2);
        equalize = new float[NFREQ / 2];
        for (int i = 0; i < NFREQ / 2; i++) {
            equalize[i] = 0.1f * ((float)i * inv_half_nfreq) * expf((float)i * inv_half_nfreq);
        }
    }

    void InitBitRevTable() {
        bitrevtable = new int[NFREQ];
        for (int i = 0; i < NFREQ; i++) bitrevtable[i] = i;
        for (int i = 0, j = 0; i < NFREQ; i++) {
            if (j > i) {
                int temp = bitrevtable[i];
                bitrevtable[i] = bitrevtable[j];
                bitrevtable[j] = temp;
            }
            int m = NFREQ >> 1;
            while (m >= 1 && j >= m) {
                j -= m;
                m >>= 1;
            }
            j += m;
        }
    }

    void InitCosSinTable() {
        int dftsize = 2;
        int tabsize = 0;
        while (dftsize <= NFREQ) {
            ++tabsize;
            dftsize <<= 1;
        }
        cossintable = new float[tabsize][2];
        dftsize = 2;
        int i = 0;
        while (dftsize <= NFREQ) {
            float theta = -2.0f * FFT_PI / (float)dftsize;
            cossintable[i][0] = cosf(theta);
            cossintable[i][1] = sinf(theta);
            ++i;
            dftsize <<= 1;
        }
    }
};

// ============================================================================
// Audio generation - exact copy of grandchild's audio.cpp pipeline
// ============================================================================

constexpr int AUDIO_BUFFER_LEN = 576;
constexpr int SAMPLE_RATE = 44100;
constexpr int SAMPLES_PER_FRAME = SAMPLE_RATE / 60;  // ~735 samples at 60fps
constexpr size_t RING_BUFFER_LEN = 1024;  // grandchild default

struct AudioChannels {
    float left[AUDIO_BUFFER_LEN];
    float right[AUDIO_BUFFER_LEN];
    float center[AUDIO_BUFFER_LEN];

    void average_center() {
        for (int i = 0; i < AUDIO_BUFFER_LEN; ++i) {
            center[i] = (left[i] + right[i]) / 2.0f;
        }
    }
};

// Exact copy of grandchild's RingIter from audio.cpp
struct RingIter {
    RingIter(size_t length, size_t from, bool reverse = false)
        : length(static_cast<int64_t>(length)),
          ring(static_cast<int64_t>(from % length)),
          linear(reverse ? length - 1 : 0),
          reverse(reverse) {}

    int64_t length;
    int64_t ring;
    size_t linear;
    bool reverse;

    RingIter& operator++() {
        if (this->length == 0) {
            return *this;
        }
        if (this->reverse) {
            --this->ring;
            while (this->ring < 0) {
                this->ring += this->length;
            }
            --this->linear;
        } else {
            ++this->ring;
            while (this->ring >= this->length) {
                this->ring -= this->length;
            }
            ++this->linear;
        }
        return *this;
    }
};

// Ring buffer - exact match of grandchild's Audio class state
struct AudioFrame { float left; float right; };
static std::vector<AudioFrame> g_buffer;
static size_t g_write_head = 0;
static int64_t g_latest_sample_time = 0;
static size_t g_samples_per_second = 0;

// Global FFT instance
static FFT g_fft;
static bool g_audio_initialized = false;

// FFT output size (must be power of 2 per md_fft.cpp requirements)
constexpr int FFT_SAMPLES_OUT = 512;

// Exact copy of grandchild's Audio::set
void audio_set(const float* audio_left, const float* audio_right,
               size_t audio_length, size_t samples_per_second,
               int64_t end_time_samples) {
    if (audio_length != 0 && g_latest_sample_time < end_time_samples) {
        if (audio_left == nullptr || audio_right == nullptr || samples_per_second == 0) {
            return;
        }
        size_t source_audio_offset = 0;
        if (audio_length > g_buffer.capacity()) {
            source_audio_offset = audio_length - g_buffer.capacity();
        }
        if (g_samples_per_second != samples_per_second) {
            g_write_head = 0;
            g_buffer.clear();
            g_buffer.resize(g_buffer.capacity(), {0, 0});
        }
        g_samples_per_second = samples_per_second;
        for (RingIter it(g_buffer.capacity(), g_write_head);
             it.linear < audio_length;
             ++it, ++g_write_head) {
            g_buffer[it.ring].left = fminf(1.0f, fmaxf(-1.0f, audio_left[source_audio_offset + it.linear]));
            g_buffer[it.ring].right = fminf(1.0f, fmaxf(-1.0f, audio_right[source_audio_offset + it.linear]));
        }
        g_latest_sample_time = end_time_samples;
    }
}

// Exact copy of grandchild's Audio::get
void audio_get(AudioChannels& osc, AudioChannels& spec, int64_t until_time_samples) {
    size_t dest_back_offset = 0;
    if (until_time_samples == 0) {
        until_time_samples = g_latest_sample_time;
    } else if (until_time_samples > g_latest_sample_time) {
        auto silence_samples = until_time_samples - g_latest_sample_time;
        dest_back_offset += silence_samples;
        auto dest_silence_start = AUDIO_BUFFER_LEN - silence_samples;
        memset(&osc.left[dest_silence_start], 0, silence_samples * sizeof(float));
        memset(&osc.right[dest_silence_start], 0, silence_samples * sizeof(float));
        memset(&spec.left[dest_silence_start], 0, silence_samples * sizeof(float));
        memset(&spec.right[dest_silence_start], 0, silence_samples * sizeof(float));
    }
    for (RingIter it(g_buffer.size() - dest_back_offset, g_write_head, true);
         it.linear > dest_back_offset;
         ++it) {
        osc.left[it.linear - dest_back_offset] = g_buffer[it.ring].left;
        osc.right[it.linear - dest_back_offset] = g_buffer[it.ring].right;
    }
    g_fft.time_to_frequency_domain(osc.left, spec.left);
    g_fft.time_to_frequency_domain(osc.right, spec.right);
    osc.average_center();
    spec.average_center();
}

// Generate synthetic audio for one frame (matches grandchild's generate_audio in Rust)
void generate_frame_audio(int frame, float* left, float* right) {
    const float freq = 220.0f;
    const float phase = frame * 0.1f;

    for (int i = 0; i < SAMPLES_PER_FRAME; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(SAMPLE_RATE);
        float sine_val = std::sin(2.0f * static_cast<float>(M_PI) * freq * t + phase);
        left[i] = sine_val;
        right[i] = sine_val;
    }
}

// Initialize audio system (matches grandchild's Audio constructor)
void audio_init() {
    if (!g_audio_initialized) {
        g_buffer.resize(RING_BUFFER_LEN, {0, 0});
        g_fft.Init(RING_BUFFER_LEN, RING_BUFFER_LEN, 1, 1.0f);
        g_audio_initialized = true;
    }
}

// Convert render time_ms to samples (matches grandchild's internal conversion)
int64_t time_ms_to_samples(int64_t time_ms) {
    return (time_ms * SAMPLE_RATE) / 1000;
}

// Generate audio for a frame and add to ring buffer
// Called BEFORE render_frame (matches grandchild's Rust main.rs loop order)
void audio_add_frame(int frame) {
    audio_init();

    // Generate this frame's audio samples (735 samples)
    float frame_left[SAMPLES_PER_FRAME];
    float frame_right[SAMPLES_PER_FRAME];
    generate_frame_audio(frame, frame_left, frame_right);

    // Add to ring buffer with end_sample = (frame+1) * 44100 / 60
    int64_t end_sample = ((static_cast<int64_t>(frame) + 1) * SAMPLE_RATE) / 60;
    audio_set(frame_left, frame_right, SAMPLES_PER_FRAME, SAMPLE_RATE, end_sample);
}

// Get audio for rendering at a specific time
// Called during render_frame (time_ms = frame * 1000 / 60)
void audio_to_visdata(avs::AudioData& audio, int64_t time_ms) {
    // Convert render time to samples
    int64_t until_time_samples = time_ms_to_samples(time_ms);

    // Retrieve from ring buffer at the render time
    AudioChannels osc{};
    AudioChannels spec{};
    audio_get(osc, spec, until_time_samples);

    // Convert to legacy visdata format (matches grandchild's to_legacy_visdata())
    for (int i = 0; i < AUDIO_BUFFER_LEN; ++i) {
        auto ileft = static_cast<int8_t>(osc.left[i] * 127.0f);
        auto iright = static_cast<int8_t>(osc.right[i] * 127.0f);
        audio[avs::AUDIO_WAVEFORM][avs::AUDIO_LEFT][i] = ileft;
        audio[avs::AUDIO_WAVEFORM][avs::AUDIO_RIGHT][i] = iright;
    }
    for (int i = 0; i < AUDIO_BUFFER_LEN; ++i) {
        auto ileft = static_cast<uint8_t>(spec.left[i] * 255.0f);
        auto iright = static_cast<uint8_t>(spec.right[i] * 255.0f);
        audio[avs::AUDIO_SPECTRUM][avs::AUDIO_LEFT][i] = static_cast<int8_t>(ileft);
        audio[avs::AUDIO_SPECTRUM][avs::AUDIO_RIGHT][i] = static_cast<int8_t>(iright);
    }
}

// ============================================================================
// Main program
// ============================================================================

void print_usage(const char* program) {
    std::cerr << "Usage: " << program << " --preset <file> --frame <N> --output <image.ppm> [options]\n";
    std::cerr << "\nRender a specific frame from an AVS preset to an image file.\n";
    std::cerr << "\nRequired arguments:\n";
    std::cerr << "  --preset <file>    AVS preset file (binary .avs or .json)\n";
    std::cerr << "  --frame <N>        Frame number to render (0-based)\n";
    std::cerr << "  --output <file>    Output image file (PPM format)\n";
    std::cerr << "\nOptional arguments:\n";
    std::cerr << "  --width <W>        Output width (default: 320)\n";
    std::cerr << "  --height <H>       Output height (default: 240)\n";
    std::cerr << "  -h, --help         Show this help message\n";
}

bool write_ppm(const char* path, const avs::ARGB8* pixels, int width, int height) {
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "Error: Cannot open output file: " << path << "\n";
        return false;
    }

    // PPM header
    out << "P6\n" << width << " " << height << "\n255\n";

    // Write RGB data (drop alpha)
    for (int i = 0; i < width * height; ++i) {
        out.put(static_cast<char>(pixels[i].r));  // Red
        out.put(static_cast<char>(pixels[i].g));  // Green
        out.put(static_cast<char>(pixels[i].b));  // Blue
    }

    out.close();
    return out.good();
}

int main(int argc, char* argv[]) {
    // Default values
    const char* preset_path = nullptr;
    const char* output_path = nullptr;
    int frame_num = -1;
    int width = 320;
    int height = 240;

    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "--preset") == 0 && i + 1 < argc) {
            preset_path = argv[++i];
        }
        else if (strcmp(argv[i], "--frame") == 0 && i + 1 < argc) {
            frame_num = std::atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            output_path = argv[++i];
        }
        else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
            width = std::atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            height = std::atoi(argv[++i]);
        }
        else {
            std::cerr << "Unknown argument: " << argv[i] << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    // Validate required arguments
    if (!preset_path) {
        std::cerr << "Error: --preset is required\n";
        print_usage(argv[0]);
        return 1;
    }
    if (frame_num < 0) {
        std::cerr << "Error: --frame is required\n";
        print_usage(argv[0]);
        return 1;
    }
    if (!output_path) {
        std::cerr << "Error: --output is required\n";
        print_usage(argv[0]);
        return 1;
    }
    if (width <= 0 || height <= 0) {
        std::cerr << "Error: Invalid dimensions\n";
        return 1;
    }

    // Fixed RNG seed for deterministic output
    std::srand(12345);

    // Register built-in effects
    avs::register_builtin_effects();

    // Create renderer
    avs::Renderer<avs::ARGB8> renderer(width, height);

    // Load preset
    if (!avs::Preset::load(preset_path, *renderer.root(), avs::PresetFormat::AUTO)) {
        std::cerr << "Error loading preset: " << avs::Preset::last_error() << "\n";
        return 1;
    }

    std::cerr << "Loaded preset: " << preset_path << "\n";
    std::cerr << "Rendering frame " << frame_num << " at " << width << "x" << height << "\n";

    // Create output buffer
    std::vector<avs::ARGB8> output(width * height);

    // Render frames up to target frame with synthetic audio
    // Matches grandchild's Rust main.rs loop order exactly:
    // 1. generate_audio(f) and audio_set() with end_sample = (f+1) * 44100 / 60
    // 2. render_frame() with time_ms = f * 1000 / 60
    avs::AudioData audio = {};
    for (int f = 0; f <= frame_num; ++f) {
        // Add this frame's audio to ring buffer
        audio_add_frame(f);

        // Get audio at render time and convert to visdata
        int64_t time_ms = (static_cast<int64_t>(f) * 1000) / 60;
        audio_to_visdata(audio, time_ms);

        // Render
        renderer.render(audio, false, output.data());
    }

    // Write output
    if (!write_ppm(output_path, output.data(), width, height)) {
        std::cerr << "Error writing output file\n";
        return 1;
    }

    std::cerr << "Wrote: " << output_path << "\n";
    return 0;
}
