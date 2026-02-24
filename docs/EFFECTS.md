# AVS Effects Catalogue

A comprehensive catalogue of all AVS effects with their inputs, outputs, blend modes, and controls.

## Implementation Status

**49 of 51 portable effects implemented (96%)**

### Remaining Effects (1)

| Effect | Source | Status | Notes |
|--------|--------|--------|-------|
| AVI | `r_avi.cpp` | **TODO** | Video file playback (platform-specific) |

### Not Porting

| Effect | Source | Reason |
|--------|--------|--------|
| Transition | `r_transition.cpp` | Not an effect - global preset transition infrastructure requiring dual renderers |
| Text | `r_text.cpp` | Windows GDI font rendering |
| SVP | `r_svp.cpp` | Sonique plugin DLL loading |

## Binary Preset Color Formats

**ALL effects use 0x00RRGGBB format internally** (same byte order as our ARGB, just without alpha).

This was determined by examining `util.cpp` in the original Windows source:
- `GR_SelectColor()` (lines 61-75) swaps R↔B before/after `ChooseColor` API
- `GR_DrawColoredButton()` (lines 77-79) swaps R↔B before drawing

Since ALL effects use these shared utilities for color picking/display, they ALL store colors
in 0x00RRGGBB format. The utilities handle conversion to/from Windows COLORREF (0x00BBGGRR).

**When loading colors from binary presets: NO swap needed, just add alpha:**
```cpp
uint32_t color = reader.read_u32() | 0xFF000000;
```

Evidence from original source:
- `r_dotfnt.cpp` line 132: `RGB(24,107,28); // reverse BGR :)` - confirms RGB() params are BGR
- `r_simple.cpp` line 300: explicit swap before `CreatePen()` (GDI expects COLORREF)
- `r_sscope.cpp` line 435: explicit swap for custom color bar drawing

---

## Original Source File Mapping

For reference when examining original AVS implementation.
Key: ✓ = implemented, **TODO** = not yet implemented, ✗ = not porting

- ✓ **Clear Screen**: `r_clear.cpp` → `clear.cpp`
- ✓ **Simple**: `r_simple.cpp` → `oscilloscope.cpp`
- ✓ **SuperScope**: `r_sscope.cpp` → `superscope.cpp`
- ✓ **Dot Grid**: `r_dotgrid.cpp` → `dot_grid.cpp`
- ✓ **Dot Plane**: `r_dotpln.cpp` → `dot_plane.cpp`
- ✓ **Dot Fountain**: `r_dotfnt.cpp` → `dot_fountain.cpp`
- ✓ **Moving Particle**: `r_parts.cpp` → `moving_particle.cpp`
- ✓ **OnBeat Clear**: `r_nfclr.cpp` → `onbeat_clear.cpp`
- ✓ **Picture/Picture II**: `r_picture.cpp` → `picture.cpp`
- ✓ **Oscilloscope Star**: `r_oscstar.cpp` → `oscstar.cpp`
- ✓ **Ring**: `r_oscring.cpp` → `ring.cpp`
- ✓ **Rotating Stars**: `r_rotstar.cpp` → `rotstar.cpp`
- ✗ **SVP**: `r_svp.cpp` — NOT PORTING (Windows DLL)
- ✗ **Text**: `r_text.cpp` — NOT PORTING (Windows GDI)
- ✓ **Timescope**: `r_timescope.cpp` → `timescope.cpp`
- ✓ **Movement**: `r_trans.cpp` → `movement.cpp`
- ✓ **Dynamic Movement**: `r_dmove.cpp` → `dynamic_movement.cpp`
- ✓ **Dynamic Distance Modifier**: `r_ddm.cpp` → `ddm.cpp`
- ✓ **Dynamic Shift**: `r_shift.cpp` → `shift.cpp`
- ✓ **Roto Blitter**: `r_rotblit.cpp` → `rotoblitter.cpp`
- ✓ **Scatter**: `r_scat.cpp` → `scatter.cpp`
- ✓ **Blur**: `r_blur.cpp` → `blur.cpp`
- ✓ **Brightness**: `r_bright.cpp` → `brightness.cpp`
- ✓ **Fast Brightness**: `r_fastbright.cpp` → `fast_brightness.cpp`
- ✓ **Channel Shift**: `r_chanshift.cpp` → `channel_shift.cpp`
- ✓ **Color Modifier**: `r_dcolormod.cpp` → `color_modifier.cpp`
- ✓ **Color Reduction**: `r_colorreduction.cpp` → `color_reduction.cpp`
- ✓ **Color Clip**: `r_contrast.cpp` → `color_clip.cpp`
- ✓ **Blitter Feedback**: `r_blit.cpp` → `blitter_feedback.cpp`
- ✓ **Grain**: `r_grain.cpp` → `grain.cpp`
- ✓ **Interferences**: `r_interf.cpp` → `interferences.cpp`
- ✓ **Interleave**: `r_interleave.cpp` → `interleave.cpp`
- ✓ **Invert**: `r_invert.cpp` → `invert.cpp`
- ✓ **Mirror**: `r_mirror.cpp` → `mirror.cpp`
- ✓ **Mosaic**: `r_mosaic.cpp` → `mosaic.cpp`
- ✓ **Multi Delay**: `r_multidelay.cpp` → `multi_delay.cpp`
- ✓ **Multiplier**: `r_multiplier.cpp` → `multiplier.cpp`
- ✓ **Unique tone**: `r_onetone.cpp` → `unique_tone.cpp`
- ✓ **Video Delay**: `r_videodelay.cpp` → `video_delay.cpp`
- ✓ **Water**: `r_water.cpp` → `water.cpp`
- ✓ **Water Bump**: `r_waterbump.cpp` → `water_bump.cpp`
- ✓ **Comment**: `r_comment.cpp` → `comment.cpp`
- ✓ **Effect List**: `r_list.cpp` → `effect_list.cpp`
- ✗ **Transition**: `r_transition.cpp` (not an effect - global preset transition infrastructure)
- ✓ **Bump**: `r_bump.cpp` → `bump.cpp`
- ✓ **Fadeout**: `r_fadeout.cpp` → `fadeout.cpp`
- ✓ **Colorfade**: `r_colorfade.cpp` → `color_fade.cpp`
- ✓ **Bass Spin**: `r_bspin.cpp` → `bass_spin.cpp`
- ✓ **Buffer Save**: `r_stack.cpp` → `buffer_save.cpp`
- **TODO** **AVI**: `r_avi.cpp`
- ✓ **Set Render Mode**: `r_linemode.cpp` → `set_render_mode.cpp`
- ✓ **Starfield**: `r_stars.cpp` → `starfield.cpp`
- ✓ **Custom BPM**: `r_bpm.cpp` → `custom_bpm.cpp`

### Extensions (not in original AVS)
- ✓ **Dynamic Movement Extended**: `dynamic_movement_ext.cpp`
- ✓ **Starfield Extended**: `starfield_ext.cpp`
- ✓ **Set Render Mode Extended**: `set_render_mode_ext.cpp`

## Render Effects
Effects that generate new visual content from audio data or clear the screen.

1. ### Clear Screen
- **Purpose**: Clears or blends framebuffer with a color to create trails/feedback
- **Source File**: `r_clear.cpp`
- **Inputs**: 
  - Framebuffer
  - Beat signal
- **Outputs**: Framebuffer (modified in-place)
- **Blend Modes**:
  - Replace: Fill with solid color (no trails)
  - Additive: Add color values (brightening effect)
  - Maximum: Take maximum of each channel
  - Average: Average with color (creates fadeout trails)
  - Subtractive: Subtract color values (darkening effect)
- **Controls**:
  - Enable Clear screen: CHECKBOX, Position(0, 0), Size(79, 10), ID(IDC_CHECK1)
  - Color: COLOR_BUTTON, Position(0, 15), Size(137, 13), ID(IDC_DEFCOL)
  - First frame only: CHECKBOX, Position(0, 30), Size(63, 10), ID(IDC_CLEARFIRSTFRAME)
  - Replace blend mode: RADIO_BUTTON, Position(0, 41), Size(43, 10), ID(IDC_REPLACE)
  - Additive blend mode: RADIO_BUTTON, Position(0, 51), Size(61, 10), ID(IDC_ADDITIVE)
  - Blend 50/50 mode: RADIO_BUTTON, Position(0, 61), Size(55, 10), ID(IDC_5050)
  - Default render blend mode: RADIO_BUTTON, Position(0, 71), Size(99, 10), ID(IDC_DEFRENDBLEND)

2. ### Simple
- **Purpose**: Basic oscilloscope - renders audio waveform or spectrum visualization.
- **Source File**: `r_simple.cpp`
- **Inputs**:
  - Audio waveform (576 samples per channel)
  - Audio spectrum (256 bands)
  - Framebuffer
- **Outputs**: Framebuffer (modified in-place)
- **Blend Modes**: None (direct draw)
- **Controls**:
  - Render Spectrum: RADIO_BUTTON, Position(6, 9), Size(46, 10), ID(IDC_SA)
  - Render Oscilliscope: RADIO_BUTTON, Position(53, 9), Size(53, 10), ID(IDC_OSC)
  - Draw Lines: RADIO_BUTTON, Position(6, 24), Size(33, 10), ID(IDC_LINES)
  - Draw Solid: RADIO_BUTTON, Position(38, 24), Size(31, 10), ID(IDC_SOLID)
  - Draw Dots: RADIO_BUTTON, Position(72, 24), Size(31, 10), ID(IDC_DOT)
  - Left Channel: RADIO_BUTTON, Position(8, 68), Size(56, 10), ID(IDC_LEFTCH)
  - Right Channel: RADIO_BUTTON, Position(8, 78), Size(61, 10), ID(IDC_RIGHTCH)
  - Center Channel: RADIO_BUTTON, Position(8, 88), Size(65, 10), ID(IDC_MIDCH)
  - Top Position: RADIO_BUTTON, Position(86, 68), Size(29, 10), ID(IDC_TOP)
  - Bottom Position: RADIO_BUTTON, Position(86, 78), Size(38, 10), ID(IDC_BOTTOM)
  - Center Position: RADIO_BUTTON, Position(86, 88), Size(37, 10), ID(IDC_CENTER)
  - Number of colors: EDITTEXT, Position(53, 107), Size(19, 12), ID(IDC_NUMCOL)
  - Color selection: COLOR_ARRAY, Position(6, 122), Size(127, 11), ID(IDC_DEFCOL)

3. ### SuperScope
- **Purpose**: Advanced oscilloscope with multi-phase scripting
- **Source File**: `r_sscope.cpp`
- **Inputs**:
  - Audio waveform
  - Audio spectrum
  - Beat signal
  - Frame counter
- **Outputs**: Framebuffer (points/lines drawn)
- **Blend Modes**: Additive or replace
- **Controls**:
  - Init script: EDITTEXT, Position(25, 0), Size(208, 26), ID(IDC_EDIT4)
  - Frame script: EDITTEXT, Position(25, 26), Size(208, 46), ID(IDC_EDIT2)
  - Beat script: EDITTEXT, Position(25, 72), Size(208, 38), ID(IDC_EDIT3)
  - Pixel script: EDITTEXT, Position(25, 110), Size(208, 56), ID(IDC_EDIT1)
  - Source data Waveform: RADIO_BUTTON, Position(5, 178), Size(49, 10), ID(IDC_WAVE)
  - Source data Spectrum: RADIO_BUTTON, Position(55, 178), Size(46, 10), ID(IDC_SPEC)
  - Left Channel: RADIO_BUTTON, Position(5, 189), Size(28, 10), ID(IDC_LEFTCH)
  - Center Channel: RADIO_BUTTON, Position(36, 189), Size(37, 10), ID(IDC_MIDCH)
  - Right Channel: RADIO_BUTTON, Position(76, 189), Size(33, 10), ID(IDC_RIGHTCH)
  - Number of colors: EDITTEXT, Position(47, 202), Size(19, 12), ID(IDC_NUMCOL)
  - Color selection: COLOR_ARRAY, Position(125, 202), Size(108, 11), ID(IDC_DEFCOL)
  - Load example...: PUSHBUTTON, Position(176, 170), Size(57, 14), ID(IDC_BUTTON1)
  - Expression help: PUSHBUTTON, Position(107, 170), Size(67, 14), ID(IDC_BUTTON2)
  - Draw as Dots: RADIO_BUTTON, Position(166, 188), Size(31, 10), ID(IDC_DOT)
  - Draw as Lines: RADIO_BUTTON, Position(200, 188), Size(33, 10), ID(IDC_LINES)

4. ### Dot Grid
- **Purpose**: Renders a grid of dots with color cycling and movement (no audio reactivity)
- **Source File**: `r_dotgrid.cpp`
- **Inputs**:
  - None (purely visual effect, no audio)
- **Outputs**: Framebuffer (dots rendered)
- **Blend Modes**: Replace (0), Additive (1), 50/50 (2), Default/BLEND_LINE (3)
- **Defaults**:
  - num_colors: 1
  - colors[0]: white (0xFFFFFF)
  - x_move: 128
  - y_move: 128
  - spacing: 8
  - blend: 3 (Default render blend mode)
- **Controls**:
  - Colors groupbox: GROUPBOX, Position(0, 0), Size(137, 36)
  - "Cycle through" label: LTEXT, Position(7, 8), Size(46, 8)
  - Number of colors: EDITTEXT, Position(53, 6), Size(19, 12), ID(IDC_NUMCOL), Range(1, 16), Default(1)
  - "colors (max 16)" label: LTEXT, Position(77, 8), Size(48, 8)
  - Color selection: COLOR_ARRAY, Position(6, 21), Size(127, 11), ID(IDC_DEFCOL)
  - X movement: SLIDER, Position(16, 47), Size(85, 13), ID(IDC_SLIDER1), Range(-512, 544), Default(128)
    - Note: Slider pos 0-33, value = (pos-16)*32
  - Zero X movement: BUTTON, Position(103, 47), Size(28, 13), ID(IDC_BUTTON1), Text("zero")
  - X Speed label: LTEXT, Position(133, 49), Size(40, 10), Text("X Speed")
  - Y movement: SLIDER, Position(16, 65), Size(85, 13), ID(IDC_SLIDER2), Range(-512, 544), Default(128)
  - Zero Y movement: BUTTON, Position(103, 65), Size(28, 13), ID(IDC_BUTTON3), Text("zero")
  - Y Speed label: LTEXT, Position(133, 67), Size(40, 10), Text("Y Speed")
  - Replace blend mode: RADIO_BUTTON, Position(5, 99), Size(43, 10), ID(IDC_RADIO1), Value(0)
  - Additive blend mode: RADIO_BUTTON, Position(51, 99), Size(41, 10), ID(IDC_RADIO2), Value(1)
  - 50/50 blend mode: RADIO_BUTTON, Position(93, 99), Size(35, 10), ID(IDC_RADIO3), Value(2)
  - Default render blend mode: RADIO_BUTTON, Position(5, 109), Size(99, 10), ID(IDC_RADIO4), Value(3)
  - "Dot spacing:" label: LTEXT, Position(0, 127), Size(43, 8)
  - Dot spacing: EDITTEXT, Position(44, 125), Size(20, 12), ID(IDC_EDIT1), Range(2, ∞), Default(8)
- **Notes**:
  - Color cycling: Smoothly interpolates between colors, 64 frames per color
  - BLEND_LINE (mode 3) is additive blend, same as mode 1
  - Movement uses fixed-point (>>8) for sub-pixel precision

5. ### Dot Plane
- **Purpose**: 3D plane of dots with perspective
- **Source File**: `r_dotpln.cpp`
- **Inputs**:
  - Audio input
  - Beat signal
- **Outputs**: Perspective-projected dots
- **Blend Modes**: Additive or replace
- **Controls**:
  - Rotation speed: SLIDER, Position(6, 17), Size(95, 15), ID(IDC_SLIDER1)
  - Zero rotation speed: BUTTON, Position(103, 15), Size(28, 10), ID(IDC_BUTTON1)
  - Angle/Perspective: SLIDER, Position(6, 45), Size(126, 15), ID(IDC_ANGLE)
  - Top Color: COLOR_BUTTON, Position(7, 79), Size(41, 10), ID(IDC_C1)
  - High Color: COLOR_BUTTON, Position(7, 90), Size(41, 10), ID(IDC_C2)
  - Mid Color: COLOR_BUTTON, Position(7, 101), Size(41, 10), ID(IDC_C3)
  - Low Color: COLOR_BUTTON, Position(7, 112), Size(41, 10), ID(IDC_C4)
  - Bottom Color: COLOR_BUTTON, Position(7, 123), Size(41, 10), ID(IDC_C5)

6. ### Dot Fountain
- **Purpose**: 3D rotating dot fountain particle effect
- **Source File**: `r_dotfnt.cpp`
- **Inputs**:
  - Beat signal
  - Framebuffer
- **Outputs**: 3D particle fountain rendered to Framebuffer
- **Blend Modes**: Direct draw
- **Controls**: (Uses IDD_CFG_DOTPLANE dialog)
  - Rotation speed: SLIDER, Position(5, 11), Size(97, 21), ID(IDC_SPEED)
  - Zero rotation speed: BUTTON, Position(103, 15), Size(28, 10), ID(IDC_BUTTON1)
  - Angle/Perspective: SLIDER, Position(6, 45), Size(126, 15), ID(IDC_ANGLE)
  - Top Color: COLOR_BUTTON, Position(7, 79), Size(41, 10), ID(IDC_C1)
  - High Color: COLOR_BUTTON, Position(7, 90), Size(41, 10), ID(IDC_C2)
  - Mid Color: COLOR_BUTTON, Position(7, 101), Size(41, 10), ID(IDC_C3)
  - Low Color: COLOR_BUTTON, Position(7, 112), Size(41, 10), ID(IDC_C4)
  - Bottom Color: COLOR_BUTTON, Position(7, 123), Size(41, 10), ID(IDC_C5)

7. ### Moving Particle
- **Purpose**: Physics-based bouncing particle that reacts to beats
- **Source File**: `r_parts.cpp`
- **Legacy Index**: 8
- **Inputs**:
  - Beat signal (sets random target position)
- **Outputs**: Circular particle rendered to Framebuffer
- **Blend Modes**: Replace, Additive, 50/50, Default
- **Controls**:
  - Enabled: CHECKBOX, Position(0, 0), Size(41, 10), ID(IDC_LEFT), Default(true)
  - Color: COLOR_BUTTON, Position(43, 0), Size(40, 10), ID(IDC_LC), Default(0xFFFFFF)
  - Distance from center: SLIDER, Position(5, 21), Size(130, 16), ID(IDC_SLIDER1), Range(1, 32), Default(16)
  - Particle size: SLIDER, Position(5, 47), Size(130, 16), ID(IDC_SLIDER3), Range(1, 128), Default(8)
  - Onbeat sizechange enabled: CHECKBOX, Position(9, 75), Size(107, 10), ID(IDC_CHECK1), Default(false)
  - Particle size (onbeat): SLIDER, Position(5, 84), Size(130, 16), ID(IDC_SLIDER4), Range(1, 128), Default(8)
  - Replace blend mode: RADIO_BUTTON, Position(5, 114), Size(43, 10), ID(IDC_RADIO1), Value(0)
  - Additive blend mode: RADIO_BUTTON, Position(51, 114), Size(41, 10), ID(IDC_RADIO2), Value(1)
  - 50/50 blend mode: RADIO_BUTTON, Position(93, 114), Size(35, 10), ID(IDC_RADIO3), Value(2)
  - Default render blend mode: RADIO_BUTTON, Position(5, 124), Size(99, 9), ID(IDC_RADIO4), Value(3)
- **Notes**:
  - On beat: Sets random target position for particle to move toward
  - Physics: Particle accelerates toward target with spring force, velocity damped by 0.991x
  - Size interpolates toward target size over time (s_pos = (s_pos + size) / 2)
  - Draws filled circle using scan-line algorithm

8. ### OnBeat Clear
- **Purpose**: Clears screen on beat detection
- **Source File**: `r_nfclr.cpp`
- **Inputs**:
  - Beat signal
  - Framebuffer
- **Outputs**: Cleared framebuffer on beats
- **Blend Modes**: Replace or 50/50 blend
- **Controls**:
  - Beats groupbox: GROUPBOX, Position(0, 0), Size(137, 36), Text("Clear every N beats")
  - N beats slider: SLIDER, Position(4, 9), Size(128, 13), ID(IDC_SLIDER1), Range(0, 100), Default(1)
  - Min label: LTEXT, Position(7, 24), Size(8, 8), Text("0")
  - Max label: LTEXT, Position(115, 24), Size(13, 8), Text("100")
  - Color: COLOR_BUTTON, Position(0, 40), Size(46, 10), ID(IDC_BUTTON1), Default(0xFFFFFF)
  - Blend to color: CHECKBOX, Position(49, 40), Size(59, 10), ID(IDC_BLEND), Default(false)

9. ### Picture/Picture II
- **Purpose**: Renders bitmap images
- **Source File**: `r_picture.cpp`
- **Inputs**:
  - Image file
  - Beat signal
- **Outputs**: Image rendered to Framebuffer
- **Blend Modes**: Replace/Add/50-50/Every other pixel
- **Controls**:
  - Enable Picture rendering: CHECKBOX, Position(0, 0), Size(93, 10), ID(IDC_ENABLED)
  - Image selection: DROPDOWN, Position(1, 14), Size(232, 223), ID(OBJ_COMBO)
  - Replace blend mode: RADIO_BUTTON, Position(0, 30), Size(43, 10), ID(IDC_REPLACE)
  - Additive blend mode: RADIO_BUTTON, Position(0, 41), Size(61, 10), ID(IDC_ADDITIVE)
  - Blend 50/50 mode: RADIO_BUTTON, Position(0, 52), Size(55, 10), ID(IDC_5050)
  - Blend 50/50 + OnBeat Additive: RADIO_BUTTON, Position(0, 63), Size(115, 10), ID(IDC_ADAPT)
  - Beat persistence: SLIDER, Position(6, 92), Size(125, 19), ID(IDC_PERSIST)
  - Keep aspect ratio: CHECKBOX, Position(2, 121), Size(71, 10), ID(IDC_RATIO)
  - Aspect ratio On X-Axis: RADIO_BUTTON, Position(20, 136), Size(47, 10), ID(IDC_X_RATIO)
  - Aspect ratio On Y-Axis: RADIO_BUTTON, Position(20, 147), Size(47, 10), ID(IDC_Y_RATIO)

10. ### Oscilloscope Star
- **Purpose**: Renders an oscilloscope-like star pattern, reacting to audio
- **Source File**: `r_oscstar.cpp`
- **Inputs**:
  - Audio waveform (mono/stereo)
  - Beat signal
- **Outputs**: Framebuffer (star pattern drawn)
- **Blend Modes**: Standard blend modes (e.g., replace, additive)
- **Controls**:
  - Star size: SLIDER, Position(4, 10), Size(128, 13), ID(IDC_SLIDER1)
  - Rotation: SLIDER, Position(3, 43), Size(128, 13), ID(IDC_SLIDER2)
  - Left Channel: RADIO_BUTTON, Position(8, 68), Size(56, 10), ID(IDC_LEFTCH)
  - Right Channel: RADIO_BUTTON, Position(8, 78), Size(61, 10), ID(IDC_RIGHTCH)
  - Center Channel: RADIO_BUTTON, Position(8, 88), Size(65, 10), ID(IDC_MIDCH)
  - Position Left: RADIO_BUTTON, Position(86, 68), Size(28, 10), ID(IDC_TOP)
  - Position Right: RADIO_BUTTON, Position(86, 78), Size(33, 10), ID(IDC_BOTTOM)
  - Position Center: RADIO_BUTTON, Position(86, 88), Size(37, 10), ID(IDC_CENTER)
  - Number of colors: EDITTEXT, Position(53, 107), Size(19, 12), ID(IDC_NUMCOL)
  - Color selection: COLOR_ARRAY, Position(6, 122), Size(127, 11), ID(IDC_DEFCOL)

11. ### Ring
- **Purpose**: Renders expanding rings from audio
- **Source File**: `r_oscring.cpp`
- **Inputs**:
  - Audio waveform
  - Beat signal
- **Outputs**: Concentric rings
- **Blend Modes**: Additive
- **Controls**:
  - Ring size: SLIDER, Position(4, 10), Size(128, 15), ID(IDC_SLIDER1)
  - Source Oscilloscope: RADIO_BUTTON, Position(8, 56), Size(53, 10), ID(IDC_OSC)
  - Source Spectrum: RADIO_BUTTON, Position(65, 56), Size(46, 10), ID(IDC_SPEC)
  - Left Channel: RADIO_BUTTON, Position(8, 68), Size(56, 10), ID(IDC_LEFTCH)
  - Right Channel: RADIO_BUTTON, Position(8, 78), Size(61, 10), ID(IDC_RIGHTCH)
  - Center Channel: RADIO_BUTTON, Position(8, 88), Size(65, 10), ID(IDC_MIDCH)
  - Position Left: RADIO_BUTTON, Position(86, 68), Size(28, 10), ID(IDC_TOP)
  - Position Right: RADIO_BUTTON, Position(86, 78), Size(33, 10), ID(IDC_BOTTOM)
  - Position Center: RADIO_BUTTON, Position(86, 88), Size(37, 10), ID(IDC_CENTER)
  - Number of colors: EDITTEXT, Position(53, 107), Size(19, 12), ID(IDC_NUMCOL)
  - Color selection: COLOR_ARRAY, Position(6, 122), Size(127, 11), ID(IDC_DEFCOL)

12. ### Rotating Stars
- **Purpose**: Renders rotating star shapes that cycle through colors
- **Source File**: `r_rotstar.cpp`
- **Inputs**:
  - Framebuffer
- **Outputs**: Rotating star shapes drawn to framebuffer
- **Blend Modes**: Additive (line blend)
- **Controls**:
  - Colors groupbox: GROUPBOX, Position(0, 0), Size(137, 36)
  - Number of colors: EDITTEXT, Position(53, 6), Size(19, 12), ID(IDC_NUMCOL)
  - Color selection: COLOR_ARRAY, Position(6, 21), Size(127, 11), ID(IDC_DEFCOL)

13. ### Starfield
- **Purpose**: Creates a starfield effect with configurable speed, density, and beat-driven changes.
- **Source File**: `r_stars.cpp`
- **Inputs**:
  - Audio input
  - Beat signal
- **Outputs**: Starfield rendered to framebuffer
- **Blend Modes**: Replace, Additive, 50/50
- **Controls**:
  - Enable Starfield: CHECKBOX, Position(0, 0), Size(65, 10), ID(IDC_CHECK1)
  - Speed: SLIDER, Position(0, 13), Size(137, 11), ID(IDC_SPEED)
  - "Slow" label: LTEXT, Position(0, 23), Size(16, 8)
  - "Fast" label: LTEXT, Position(123, 23), Size(14, 8)
  - Number of stars: SLIDER, Position(0, 32), Size(137, 11), ID(IDC_NUMSTARS)
  - "Fewer stars" label: LTEXT, Position(0, 42), Size(37, 8)
  - "More stars" label: LTEXT, Position(103, 42), Size(34, 8)
  - Replace blend mode: RADIO_BUTTON, Position(0, 52), Size(43, 10), ID(IDC_REPLACE)
  - Additive blend mode: RADIO_BUTTON, Position(0, 62), Size(61, 10), ID(IDC_ADDITIVE)
  - Blend 50/50 mode: RADIO_BUTTON, Position(0, 72), Size(55, 10), ID(IDC_5050)
  - Color: COLOR_BUTTON, Position(100, 57), Size(37, 15), ID(IDC_DEFCOL)
  - OnBeat Speed changes: CHECKBOX, Position(0, 82), Size(92, 10), ID(IDC_ONBEAT2)
  - OnBeat Speed Change: SLIDER, Position(0, 93), Size(137, 11), ID(IDC_SPDCHG)
  - "Slower" label: LTEXT, Position(0, 103), Size(22, 8)
  - "Faster" label: LTEXT, Position(117, 103), Size(20, 8)
  - OnBeat Duration: SLIDER, Position(0, 112), Size(137, 11), ID(IDC_SPDDUR)
  - "Shorter" label: LTEXT, Position(0, 122), Size(24, 8)
  - "Longer" label: LTEXT, Position(114, 122), Size(23, 8)

14. ### Starfield (extended) [EXTENSION]
- **Purpose**: Starfield with styled point drawing from Set Render Mode (extended)
- **Source File**: `starfield_ext.cpp` (NOT part of original AVS)
- **Inputs**:
  - Audio input
  - Beat signal
- **Outputs**: Starfield rendered with styled points
- **Blend Modes**: Uses Set Render Mode (extended) styling (no local blend controls)
- **Notes**:
  - Extension effect - uses `draw_point_styled()` for large/circular star points
  - Respects point size and style from Set Render Mode (extended) effect
  - No blend mode radio buttons (blend handled by render mode)
  - Otherwise identical controls to original Starfield
- **Controls**:
  - Enable Starfield: CHECKBOX, Position(0, 0), Size(65, 10)
  - Speed: SLIDER, Position(0, 13), Size(137, 11)
  - "Slow" label: LTEXT, Position(0, 23), Size(16, 8)
  - "Fast" label: LTEXT, Position(123, 23), Size(14, 8)
  - Number of stars: SLIDER, Position(0, 32), Size(137, 11)
  - "Fewer stars" label: LTEXT, Position(0, 42), Size(37, 8)
  - "More stars" label: LTEXT, Position(103, 42), Size(34, 8)
  - Color: COLOR_BUTTON, Position(0, 52), Size(37, 15)
  - OnBeat Speed changes: CHECKBOX, Position(0, 72), Size(92, 10)
  - OnBeat Speed Change: SLIDER, Position(0, 83), Size(137, 11)
  - "Slower" label: LTEXT, Position(0, 93), Size(22, 8)
  - "Faster" label: LTEXT, Position(117, 93), Size(20, 8)
  - OnBeat Duration: SLIDER, Position(0, 102), Size(137, 11)
  - "Shorter" label: LTEXT, Position(0, 112), Size(24, 8)
  - "Longer" label: LTEXT, Position(114, 112), Size(23, 8)
  - Render mode note: LTEXT, Position(0, 125), Size(137, 8), Text="Use Set Render Mode (extended) for point style"

15. ### Bass Spin
- **Purpose**: Creates spinning visual effects synchronized with bass.
- **Source File**: `r_bspin.cpp`
- **Inputs**:
  - Audio waveform (bass frequencies)
- **Outputs**: Spinning lines or triangles
- **Blend Modes**: N/A (draws directly)
- **Controls**:
  - Left enabled: CHECKBOX, Position(0, 0), Size(55, 10), ID(IDC_LEFT)
  - Right enabled: CHECKBOX, Position(0, 11), Size(60, 10), ID(IDC_RIGHT)
  - Left color: COLOR_BUTTON, Position(61, 0), Size(40, 10), ID(IDC_LC)
  - Right color: COLOR_BUTTON, Position(61, 11), Size(40, 10), ID(IDC_RC)
  - Filled Triangles: RADIO_BUTTON, Position(0, 23), Size(63, 10), ID(IDC_TRI)
  - Lines: RADIO_BUTTON, Position(0, 32), Size(33, 10), ID(IDC_LINES)

16. ### AVI
- **Purpose**: Plays AVI video streams as a visual background or overlay.
- **Source File**: `r_avi.cpp`
- **Inputs**:
  - AVI video file
  - Beat signal
- **Outputs**: Video frames rendered to framebuffer
- **Blend Modes**: Replace, Additive, 50/50, 50/50 + OnBeat Additive
- **Controls**:
  - Enable AVI stream: CHECKBOX, Position(0, 0), Size(74, 10), ID(IDC_CHECK1)
  - AVI file selection: DROPDOWN, Position(0, 12), Size(233, 92), ID(OBJ_COMBO)
  - Replace blend mode: RADIO_BUTTON, Position(0, 29), Size(43, 10), ID(IDC_REPLACE)
  - Additive blend mode: RADIO_BUTTON, Position(0, 39), Size(61, 10), ID(IDC_ADDITIVE)
  - Blend 50/50 mode: RADIO_BUTTON, Position(0, 49), Size(55, 10), ID(IDC_5050)
  - Blend 50/50 + OnBeat Additive: CHECKBOX, Position(0, 59), Size(115, 10), ID(IDC_ADAPT)
  - Beat persistence: SLIDER, Position(6, 79), Size(222, 19), ID(IDC_PERSIST)
  - Speed: SLIDER, Position(6, 121), Size(222, 19), ID(IDC_SPEED)

17. ### SVP (Sonique Visual Plugin Loader)
- **Purpose**: Loads external Sonique Visual Plugins (.SVP files) - a plugin format from the Sonique media player (~2000)
- **Source File**: `r_svp.cpp`
- **Port Status**: **NOT PORTING** - Windows-only (requires DLL loading)
- **Inputs**: (Depends on loaded plugin)
- **Outputs**: (Depends on loaded plugin)
- **Blend Modes**: (Depends on loaded plugin)
- **Controls**:
  - SVP/UVS library selection: DROPDOWN, Position(4, 12), Size(225, 222), ID(IDC_COMBO1)

18. ### Text
- **Purpose**: Renders text messages with user-selectable fonts, outline/shadow effects, and dynamic variables
- **Source File**: `r_text.cpp`
- **Port Status**: **NOT PORTING** - Deeply Windows-dependent:
  - Uses Windows GDI for text rendering (`DrawText`, `CreateFontIndirect`, bitmap DCs)
  - Font selection via Windows `ChooseFont` dialog
  - Binary preset format stores raw Windows `LOGFONT` structure - existing presets cannot be loaded on other platforms
  - Dynamic variables (`$(title)`, `$(playpos)`) require Winamp integration
  - A portable implementation would require FreeType/stb_truetype and couldn't load existing presets
- **Inputs**:
  - Text string
  - Beat signal
- **Outputs**: Framebuffer (text rendered)
- **Blend Modes**: Replace or blend
- **Controls**:
  - Enable Text: CHECKBOX, Position(0, 1), Size(51, 10), ID(IDC_CHECK1)
  - OnBeat: CHECKBOX, Position(0, 11), Size(40, 10), ID(IDC_ONBEAT)
  - Speed: SLIDER, Position(57, 1), Size(102, 11), ID(IDC_SPEED)
  - Replace blend mode: RADIO_BUTTON, Position(0, 21), Size(43, 10), ID(IDC_REPLACE)
  - Additive blend mode: RADIO_BUTTON, Position(42, 21), Size(61, 10), ID(IDC_ADDITIVE)
  - Blend 50/50 mode: RADIO_BUTTON, Position(103, 21), Size(55, 10), ID(IDC_5050)
  - Color: COLOR_BUTTON, Position(5, 45), Size(32, 11), ID(IDC_DEFCOL)
  - Outline/Shadow color: COLOR_BUTTON, Position(5, 76), Size(109, 11), ID(IDC_DEFOUTCOL)
  - Choose font: BUTTON, Position(43, 45), Size(57, 11), ID(IDC_CHOOSEFONT)
  - Random position: CHECKBOX, Position(6, 113), Size(43, 10), ID(IDC_RANDOMPOS)
  - Left Horizontal Align: RADIO_BUTTON, Position(14, 133), Size(28, 10), ID(IDC_HLEFT)
  - Center Horizontal Align: RADIO_BUTTON, Position(14, 143), Size(33, 10), ID(IDC_HCENTER)
  - Right Horizontal Align: RADIO_BUTTON, Position(14, 154), Size(29, 10), ID(IDC_HRIGHT)
  - Horizontal shift: SLIDER, Position(8, 165), Size(37, 10), ID(IDC_HSHIFT)
  - Horizontal reset: BUTTON, Position(48, 166), Size(9, 9), ID(IDC_HRESET)
  - Top Vertical Align: RADIO_BUTTON, Position(71, 135), Size(29, 10), ID(IDC_VTOP)
  - Center Vertical Align: RADIO_BUTTON, Position(71, 146), Size(33, 10), ID(IDC_VCENTER)
  - Bottom Vertical Align: RADIO_BUTTON, Position(71, 157), Size(34, 10), ID(IDC_VBOTTOM)
  - Vertical shift: SLIDER, Position(112, 130), Size(9, 35), ID(IDC_VSHIFT)
  - Vertical reset: BUTTON, Position(112, 166), Size(9, 9), ID(IDC_VRESET)
  - Insert blanks: CHECKBOX, Position(165, 191), Size(55, 10), ID(IDC_BLANKS)
  - Outline/Shadow amount: SLIDER, Position(85, 89), Size(42, 9), ID(IDC_OUTLINESIZE)
  - Plain style: RADIO_BUTTON, Position(5, 62), Size(29, 10), ID(IDC_PLAIN)
  - Outlined style: RADIO_BUTTON, Position(37, 62), Size(39, 10), ID(IDC_OUTLINE)
  - Shadowed style: RADIO_BUTTON, Position(81, 62), Size(46, 10), ID(IDC_SHADOW)
  - Random ordering: CHECKBOX, Position(98, 191), Size(67, 10), ID(IDC_RANDWORD)
  - Text content: EDITTEXT, Position(0, 202), Size(233, 12), ID(IDC_EDIT)

19. ### Timescope
- **Purpose**: Scrolling waveform history
- **Inputs**:
  - Audio waveform over time
- **Outputs**: Scrolling waveform display
- **Blend Modes**: Replace
- **Controls**:
  - Enable Timescope: CHECKBOX, Position(0, 0), Size(75, 10), ID(IDC_CHECK1)
  - Color: COLOR_BUTTON, Position(0, 15), Size(137, 13), ID(IDC_DEFCOL)
  - Replace blend mode: RADIO_BUTTON, Position(0, 30), Size(43, 10), ID(IDC_REPLACE)
  - Additive blend mode: RADIO_BUTTON, Position(0, 40), Size(61, 10), ID(IDC_ADDITIVE)
  - Blend 50/50 mode: RADIO_BUTTON, Position(0, 50), Size(55, 10), ID(IDC_5050)
  - Default render blend mode: RADIO_BUTTON, Position(0, 59), Size(80, 10), ID(IDC_DEFAULTBLEND)
  - Number of bands: SLIDER, Position(0, 95), Size(137, 11), ID(IDC_BANDS)
  - Left channel: RADIO_BUTTON, Position(0, 108), Size(55, 10), ID(IDC_LEFT)
  - Right channel: RADIO_BUTTON, Position(0, 118), Size(60, 10), ID(IDC_RIGHT)
  - Center channel: RADIO_BUTTON, Position(0, 128), Size(64, 10), ID(IDC_CENTER)


## Trans Effects (Transform)
Effects that move or distort existing pixels.

20. ### Movement
- **Purpose**: Simple preset-based transformations
- **Source File**: `r_trans.cpp`
- **Inputs**:
  - Framebuffer
- **Outputs**: Framebuffer (transformed)
- **Blend Modes**: Optional source blend
- **Controls**:
  - Preset selection: LISTBOX, Position(101, 0), Size(132, 142), ID(IDC_LIST1)
  - User defined code: EDITTEXT, Position(0, 144), Size(233, 70), ID(IDC_EDIT1)
  - Rectangular coordinates for user defined code: CHECKBOX, Position(0, 114), Size(100, 15), ID(IDC_CHECK3)
  - Blend: CHECKBOX, Position(0, 18), Size(34, 10), ID(IDC_CHECK1)
  - Source map: CHECKBOX (3-state), Position(0, 0), Size(54, 10), ID(IDC_CHECK2)
  - Bilinear filtering: CHECKBOX, Position(0, 27), Size(61, 10), ID(IDC_CHECK4)
  - Wrap: CHECKBOX, Position(0, 9), Size(33, 10), ID(IDC_WRAP)
  - Expression help: BUTTON, Position(0, 97), Size(55, 15), ID(IDC_BUTTON2)

21. ### Dynamic Movement
- **Purpose**: Grid-based coordinate transformation with scripting
- **Source File**: `r_dmove.cpp`
- **Inputs**:
  - Framebuffer
  - Audio input for script variables
  - Beat signal
- **Outputs**: Framebuffer (transformed) via coordinate remapping
- **Blend Modes**: Optional max blend
- **Controls**:
  - Init script: EDITTEXT, Position(25, 0), Size(208, 14), ID(IDC_EDIT4)
  - Frame script: EDITTEXT, Position(25, 14), Size(208, 53), ID(IDC_EDIT2)
  - Beat script: EDITTEXT, Position(25, 67), Size(208, 53), ID(IDC_EDIT3)
  - Pixel script: EDITTEXT, Position(25, 120), Size(208, 53), ID(IDC_EDIT1)
  - Rectangular coordinates: CHECKBOX, Position(0, 204), Size(90, 10), ID(IDC_CHECK3)
  - Bilinear filtering: CHECKBOX, Position(94, 204), Size(63, 10), ID(IDC_CHECK2)
  - Grid size X: EDITTEXT, Position(108, 190), Size(18, 12), ID(IDC_EDIT5)
  - Grid size Y: EDITTEXT, Position(136, 190), Size(18, 12), ID(IDC_EDIT6)
  - Blend: CHECKBOX, Position(0, 190), Size(34, 10), ID(IDC_CHECK1)
  - Wrap: CHECKBOX, Position(35, 190), Size(33, 10), ID(IDC_WRAP)
  - Buffer (source): COMBOBOX, Position(25, 175), Size(85, 56), ID(IDC_COMBO1)
  - No movement (just blend): CHECKBOX, Position(113, 176), Size(100, 10), ID(IDC_NOMOVEMENT)
  - Expression help: PUSHBUTTON, Position(158, 200), Size(73, 13), ID(IDC_BUTTON1)
  - Load example...: PUSHBUTTON, Position(158, 186), Size(73, 13), ID(IDC_BUTTON4)

22. ### Dynamic Distance Modifier
- **Purpose**: Distance-based distortion
- **Source File**: `r_ddm.cpp`
- **Inputs**:
  - Framebuffer
  - Audio input
- **Outputs**: Distance-modulated transformation
- **Blend Modes**: None
- **Controls**:
  - Init script: EDITTEXT, Position(25, 0), Size(208, 39), ID(IDC_EDIT4)
  - Frame script: EDITTEXT, Position(25, 38), Size(208, 53), ID(IDC_EDIT2)
  - Beat script: EDITTEXT, Position(25, 91), Size(208, 53), ID(IDC_EDIT3)
  - Pixel script: EDITTEXT, Position(25, 144), Size(208, 53), ID(IDC_EDIT1)
  - Blend: CHECKBOX, Position(0, 204), Size(34, 10), ID(IDC_CHECK1)
  - Bilinear filtering: CHECKBOX, Position(38, 204), Size(63, 10), ID(IDC_CHECK2)
  - Expression help: BUTTON, Position(171, 200), Size(62, 14), ID(IDC_BUTTON1)

23. ### Bump
- **Purpose**: Creates a bump map effect based on light position and depth.
- **Source File**: `r_bump.cpp`
- **Inputs**:
  - Framebuffer
  - Depth buffer (from selected buffer)
  - Beat signal
- **Outputs**: Bump-mapped pixels
- **Blend Modes**: Replace, Additive, 50/50
- **Controls**:
  - Enable Bump: CHECKBOX, Position(0, 0), Size(56, 10), ID(IDC_CHECK1)
  - Invert depth: CHECKBOX, Position(0, 10), Size(54, 10), ID(IDC_INVERTDEPTH)
  - Depth (Flat/Bumpy): SLIDER, Position(58, 11), Size(79, 11), ID(IDC_DEPTH)
  - Show Dot: CHECKBOX, Position(0, 20), Size(47, 10), ID(IDC_DOT)
  - Init script: EDITTEXT, Position(24, 52), Size(209, 33), ID(IDC_CODE3)
  - Frame script: EDITTEXT, Position(24, 87), Size(209, 48), ID(IDC_CODE1)
  - Beat script: EDITTEXT, Position(24, 136), Size(209, 40), ID(IDC_CODE2)
  - Expression help: BUTTON, Position(52, 36), Size(71, 12), ID(IDC_HELPBTN)
  - Depth buffer: DROPDOWN, Position(52, 179), Size(85, 56), ID(IDC_COMBO1)
  - Replace blend mode: RADIO_BUTTON, Position(148, 0), Size(43, 10), ID(IDC_REPLACE)
  - OnBeat: CHECKBOX, Position(0, 198), Size(40, 10), ID(IDC_ONBEAT)
  - Additive blend mode: RADIO_BUTTON, Position(148, 9), Size(61, 10), ID(IDC_ADDITIVE)
  - Blend 50/50 mode: RADIO_BUTTON, Position(148, 19), Size(55, 10), ID(IDC_5050)
  - Beat duration (Shorter/Longer): SLIDER, Position(44, 196), Size(67, 11), ID(IDC_BEATDUR)
  - OnBeat Depth (Flat/Bumpy): SLIDER, Position(114, 196), Size(67, 11), ID(IDC_DEPTH2)

24. ### Shift (Dynamic Shift)
- **Purpose**: Pixel shifting based on user-defined scripts. Shifts entire image by x,y amounts.
- **Source File**: `r_shift.cpp`
- **Legacy Index**: 42
- **Inputs**:
  - Framebuffer
  - Beat signal
- **Outputs**: Shifted pixels
- **Blend Modes**: Optional alpha blend with original
- **Script Variables**:
  - `x` - Horizontal shift in pixels (read/write)
  - `y` - Vertical shift in pixels (read/write)
  - `w` - Frame width in pixels (read-only)
  - `h` - Frame height in pixels (read-only)
  - `b` - Beat flag, 1.0 on beat, 0.0 otherwise (read-only)
  - `alpha` - Blend alpha 0.0-1.0 when blend enabled (read/write, default 0.5)
- **Default Scripts**:
  - Init: `d=0;`
  - Frame: `x=sin(d)*1.4; y=1.4*cos(d); d=d+0.01;`
  - Beat: `d=d+2.0`
- **Controls**:
  - init: LTEXT, Position(0, 20), Size(10, 8)
  - Init script: EDITTEXT, Position(29, 0), Size(204, 52), ID(IDC_EDIT1)
  - frame: LTEXT, Position(0, 83), Size(18, 8)
  - Frame script: EDITTEXT, Position(29, 52), Size(204, 70), ID(IDC_EDIT2)
  - beat: LTEXT, Position(0, 150), Size(15, 8)
  - Beat script: EDITTEXT, Position(29, 123), Size(204, 63), ID(IDC_EDIT3)
  - Blend: CHECKBOX, Position(0, 194), Size(34, 10), ID(IDC_CHECK1), Default(0)
  - Bilinear filtering: CHECKBOX, Position(35, 194), Size(63, 10), ID(IDC_CHECK2), Default(1)
  - Expression help: BUTTON, Position(162, 196), Size(71, 12), ID(IDC_HELPBTN)
- **Help Text** (IDC_HELPBTN):
  ```
  Dynamic Shift
  Variables:
  x,y = amount to shift (in pixels - set these)
  w,h = width, height (in pixels)
  b = isBeat
  alpha = alpha value (0.0-1.0) for blend
  ```
  Plus standard expression function list via `compilerfunctionlist()`.

25. ### Roto Blitter
- **Purpose**: Rotating bitmap blitter with zoom
- **Source File**: `r_rotblit.cpp`
- **Legacy Index**: 9
- **Inputs**:
  - Framebuffer (source and destination)
- **Outputs**: Rotated/scaled framebuffer
- **Blend Modes**: 50/50 blend optional
- **Controls** (from res.rc IDD_CFG_ROTBLT, dialog 137x157):
  - GROUPBOX "Zoom": Position(0, 0), Size(137, 146)
  - Zoom scale: SLIDER, Position(4, 10), Size(128, 15), ID(IDC_SLIDER1), Range(0, 256), Default(31)
  - Enable on-beat change: CHECKBOX, Position(5, 28), Size(90, 10), ID(IDC_CHECK6), Default(false)
  - On beat zoom target: SLIDER, Position(4, 37), Size(128, 13), ID(IDC_SLIDER6), Range(0, 256), Default(31)
  - LABEL "Zooming in": Position(8, 52), Size(36, 8)
  - LABEL "Zooming out": Position(88, 52), Size(40, 8)
  - GROUPBOX "Rotation": Position(0, 69), Size(137, 77)
  - Rotation speed: SLIDER, Position(4, 78), Size(128, 15), ID(IDC_SLIDER2), Range(0, 64), Default(32)
  - LABEL "Rotating Left": Position(7, 96), Size(42, 8)
  - LABEL "Rotating Right": Position(83, 96), Size(46, 8)
  - Enable on-beat reversal: CHECKBOX, Position(7, 107), Size(91, 10), ID(IDC_CHECK1), Default(false)
  - Reverse speed: SLIDER, Position(4, 115), Size(128, 15), ID(IDC_SLIDER5), Range(0, 8), Default(0)
  - LABEL "Fast reversal": Position(8, 132), Size(41, 8)
  - LABEL "Slow reversal": Position(83, 132), Size(43, 8)
  - Blend blitter: CHECKBOX, Position(0, 147), Size(53, 10), ID(IDC_BLEND), Default(false)
  - Bilinear filtering: CHECKBOX, Position(55, 147), Size(63, 10), ID(IDC_CHECK2), Default(true)
- **Notes**:
  - Zoom value 31 = no zoom (zoom = 1.0), formula: `zoom = 1.0 + (value - 31) / 31.0`
  - Rotation value 32 = no rotation, formula: `theta = (value - 32) * direction`
  - Direction smoothly interpolates between -1 and +1 on beat reverse
  - Uses fixed-point 16.16 arithmetic for rotation transform
  - Bilinear filtering (subpixel) provides smoother output

26. ### Scatter
- **Purpose**: Scatters pixels randomly using 8x8 grid offset pattern (±4 pixels)
- **Source File**: `r_scat.cpp`
- **Legacy Index**: 16
- **Inputs**:
  - Framebuffer
- **Outputs**: Scattered pixels (top/bottom 4 rows copied, middle scattered)
- **Blend Modes**: None
- **Controls**:
  - Enable scatter effect: CHECKBOX, Position(0, 0), Size(81, 10), ID(IDC_CHECK1), Default(1)

27. ### Texer/Texer II
- **Purpose**: Scriptable particle system that stamps texture images at calculated positions (like Superscope but with images instead of dots)
- **Source File**: `N/A` - **NOT IN ORIGINAL AVS SOURCE**
- **Port Status**: **NOT IN CODEBASE** - Texer and Texer II were third-party APEs (AVS Plugin Effects) created by Steven Wittens, distributed separately. They were later bundled with Winamp 5 and eventually integrated into the grandchild/vis_avs fork. Source available in grandchild fork as `e_texer.cpp` and `e_texer2.cpp` if porting is desired.

## Trans Effects (Color/Filter)
Effects that modify pixel colors or apply filters.

28. ### Blur
- **Purpose**: Smoothing/blur filter
- **Source File**: `r_blur.cpp`
- **Inputs**:
  - Framebuffer
- **Outputs**: Blurred framebuffer
- **Blend Modes**: None (direct filter)
- **Controls**:
  - No blur: RADIO_BUTTON, Position(2, 1), Size(39, 10), ID(IDC_RADIO1)
  - Light blur: RADIO_BUTTON, Position(2, 12), Size(45, 10), ID(IDC_RADIO3)
  - Medium blur: RADIO_BUTTON, Position(2, 23), Size(54, 10), ID(IDC_RADIO2)
  - Heavy blur: RADIO_BUTTON, Position(2, 34), Size(50, 10), ID(IDC_RADIO4)
  - Round down: RADIO_BUTTON, Position(3, 58), Size(57, 10), ID(IDC_ROUNDDOWN)
  - Round up: RADIO_BUTTON, Position(3, 69), Size(47, 10), ID(IDC_ROUNDUP)

29. ### Brightness
- **Purpose**: Adjust image brightness
- **Source File**: `r_bright.cpp`
- **Inputs**:
  - Framebuffer
- **Outputs**: Brightness-adjusted pixels
- **Blend Modes**: Replace, Additive, 50/50
- **Controls**:
  - Enable Brightness filter: CHECKBOX, Position(0, 0), Size(87, 10), ID(IDC_CHECK1)
  - Red label: LTEXT, Position(0, 15), Size(14, 8), Text("Red")
  - Red adjustment: SLIDER, Position(25, 13), Size(97, 13), ID(IDC_RED), Range(0, 8192), Default(4096), TickFreq(256)
  - Red reset: BUTTON, Position(125, 12), Size(12, 14), ID(IDC_BRED), Text("><")
  - Green label: LTEXT, Position(0, 30), Size(20, 8), Text("Green")
  - Green adjustment: SLIDER, Position(25, 28), Size(97, 13), ID(IDC_GREEN), Range(0, 8192), Default(4096), TickFreq(256)
  - Green reset: BUTTON, Position(125, 28), Size(12, 14), ID(IDC_BGREEN), Text("><")
  - Blue label: LTEXT, Position(0, 47), Size(15, 8), Text("Blue")
  - Blue adjustment: SLIDER, Position(25, 44), Size(97, 13), ID(IDC_BLUE), Range(0, 8192), Default(4096), TickFreq(256)
  - Blue reset: BUTTON, Position(125, 44), Size(12, 14), ID(IDC_BBLUE), Text("><")
  - Dissociate RGB values: CHECKBOX, Position(0, 60), Size(89, 10), ID(IDC_DISSOC)
  - Replace blend mode: RADIO_BUTTON, Position(0, 71), Size(43, 10), ID(IDC_REPLACE)
  - Additive blend mode: RADIO_BUTTON, Position(0, 81), Size(61, 10), ID(IDC_ADDITIVE)
  - Blend 50/50 mode: RADIO_BUTTON, Position(0, 92), Size(55, 10), ID(IDC_5050)
  - Exclude color range: CHECKBOX, Position(0, 103), Size(79, 10), ID(IDC_EXCLUDE)
  - Exclude color: COLOR_BUTTON, Position(0, 114), Size(29, 13), ID(IDC_DEFCOL)
  - Exclude distance: SLIDER, Position(31, 114), Size(106, 13), ID(IDC_DISTANCE), Range(0, 255), Default(16)
- **Notes**:
  - Slider value 4096 = 1.0x multiplier (no change)
  - Slider range 0-8192 maps to 0x-17x brightness
  - Labels are separate LTEXT controls positioned to the left of sliders

30. ### Channel Shift
- **Purpose**: Shift color channels independently
- **Source File**: `r_chanshift.cpp`
- **Inputs**:
  - Framebuffer
- **Outputs**: Channel-shifted pixels
- **Blend Modes**: None
- **Controls**:
  - GBR Order: RADIO_BUTTON, Position(15, 81), Size(35, 10), ID(IDC_GBR)
  - BRG Order: RADIO_BUTTON, Position(15, 50), Size(35, 10), ID(IDC_BRG)
  - RBG Order: RADIO_BUTTON, Position(15, 36), Size(35, 10), ID(IDC_RBG)
  - BGR Order: RADIO_BUTTON, Position(15, 65), Size(35, 10), ID(IDC_BGR)
  - GRB Order: RADIO_BUTTON, Position(15, 95), Size(35, 10), ID(IDC_GRB)
  - RGB Order: RADIO_BUTTON, Position(15, 20), Size(31, 10), ID(1023)
  - OnBeat Random: CHECKBOX, Position(15, 120), Size(65, 10), ID(IDC_ONBEAT)

31. ### Color Clip
- **Purpose**: Clip colors to a specified range, with options for input/output color and clipping distance.
- **Source File**: `r_colorreplace.cpp`
- **Inputs**:
  - Framebuffer
- **Outputs**: Clipped colors
- **Blend Modes**: None
- **Controls**:
  - Off: RADIO_BUTTON, Position(0, 0), Size(25, 10), ID(IDC_OFF)
  - Below: RADIO_BUTTON, Position(0, 10), Size(35, 10), ID(IDC_BELOW)
  - Above: RADIO_BUTTON, Position(0, 20), Size(37, 10), ID(IDC_ABOVE)
  - Near: RADIO_BUTTON, Position(0, 30), Size(31, 10), ID(IDC_NEAR)
  - Clipping distance: SLIDER, Position(36, 29), Size(101, 13), ID(IDC_DISTANCE)
  - Input color: COLOR_BUTTON, Position(0, 46), Size(26, 10), ID(IDC_LC)
  - Copy input to output color: BUTTON, Position(28, 46), Size(17, 10), ID(IDC_BUTTON1)
  - Output color: COLOR_BUTTON, Position(47, 46), Size(37, 10), ID(IDC_LC2)

32. ### Color Modifier
- **Purpose**: Modify colors with scripts
- **Source File**: `r_dcolormod.cpp`
- **Inputs**:
  - Framebuffer
- **Outputs**: Modified colors
- **Blend Modes**: None
- **Controls**:
  - Init script: EDITTEXT, Position(25, 0), Size(208, 29), ID(IDC_EDIT4)
  - Frame script: EDITTEXT, Position(25, 29), Size(208, 44), ID(IDC_EDIT2)
  - Beat script: EDITTEXT, Position(25, 73), Size(208, 53), ID(IDC_EDIT3)
  - Level script: EDITTEXT, Position(25, 126), Size(208, 69), ID(IDC_EDIT1)
  - Recompute every frame: CHECKBOX, Position(2, 199), Size(91, 10), ID(IDC_CHECK1)
  - Expression help: BUTTON, Position(164, 200), Size(69, 14), ID(IDC_BUTTON1)
  - Load example...: BUTTON, Position(97, 200), Size(63, 14), ID(IDC_BUTTON4)

33. ### Color Reduction
- **Purpose**: Reduce color bit depth
- **Source File**: `r_colorreduction.cpp`
- **Inputs**:
  - Framebuffer
- **Outputs**: Quantized colors
- **Blend Modes**: None
- **Controls**:
  - "Color levels" groupbox: GROUPBOX, Position(5, 5), Size(220, 35)
  - Color levels: SLIDER, Position(10, 15), Size(185, 20), ID(IDC_LEVELS)
  - Level display: LTEXT, Position(200, 17), Size(20, 10), ID(IDC_LEVELTEXT), Text="Static"

34. ### Blitter Feedback
- **Purpose**: Zoom feedback effect with on-beat changes
- **Source File**: `r_blit.cpp`
- **Inputs**:
  - Framebuffer
- **Outputs**: Zoomed pixels
- **Blend Modes**: Optional 50/50 blend
- **Controls**:
  - "Blitter direction" groupbox: GROUPBOX, Position(0, 0), Size(137, 38)
  - Blitter Scale: SLIDER, Position(4, 10), Size(128, 13), ID(IDC_SLIDER1)
  - "Zooming in" label: LTEXT, Position(8, 26), Size(36, 8)
  - "Zooming out" label: LTEXT, Position(88, 26), Size(40, 8)
  - "Blitter direction (onbeat)" groupbox: GROUPBOX, Position(0, 41), Size(137, 50)
  - Enable on-beat changes: CHECKBOX, Position(5, 51), Size(93, 10), ID(IDC_CHECK1)
  - Blitter Scale (on-beat): SLIDER, Position(4, 63), Size(128, 13), ID(IDC_SLIDER2)
  - "Zooming in" label (onbeat): LTEXT, Position(8, 78), Size(36, 8)
  - "Zooming out" label (onbeat): LTEXT, Position(88, 78), Size(40, 8)
  - Blend blitter: CHECKBOX, Position(0, 95), Size(53, 10), ID(IDC_BLEND)
  - Bilinear filtering: CHECKBOX, Position(0, 104), Size(63, 10), ID(IDC_CHECK2)

35. ### Fast Brightness
- **Purpose**: Optimized brightness adjustment
- **Source File**: `r_fastbright.cpp`
- **Inputs**:
  - Framebuffer
- **Outputs**: Brightness-adjusted pixels
- **Blend Modes**: None
- **Controls**:
  - Brighten by 2x: RADIO_BUTTON, Position(2, 1), Size(61, 10), ID(IDC_RADIO1)
  - Darken by 0.5x: RADIO_BUTTON, Position(2, 12), Size(64, 10), ID(IDC_RADIO2)
  - Do nothing: RADIO_BUTTON, Position(2, 23), Size(51, 10), ID(IDC_RADIO3)

36. ### Fadeout
- **Purpose**: Gradually fades the screen to a specified color.
- **Source File**: `r_fadeout.cpp`
- **Inputs**:
  - Framebuffer
- **Outputs**: Faded framebuffer
- **Blend Modes**: None (direct pixel manipulation)
- **Controls**:
  - Fade velocity: SLIDER, Position(3, 9), Size(129, 13), ID(IDC_SLIDER1)
  - Fade to color: COLOR_BUTTON, Position(0, 51), Size(78, 20), ID(IDC_LC)

37. ### Colorfade
- **Purpose**: Fades colors toward dominant channel, with options for beat-triggered changes.
- **Source File**: `r_colorfade.cpp`
- **Inputs**:
  - Framebuffer
  - Beat signal
- **Outputs**: Color-faded framebuffer
- **Blend Modes**: None (direct pixel manipulation)
- **Controls**:
  - Enable colorfade: CHECKBOX, Position(0, 0), Size(69, 10), ID(IDC_CHECK1), Default(1)
  - Color Fade Red: SLIDER, Position(0, 11), Size(100, 14), ID(IDC_SLIDER1), Range(-32, 32), Default(8)
  - Color Fade Green: SLIDER, Position(0, 28), Size(100, 14), ID(IDC_SLIDER2), Range(-32, 32), Default(-8)
  - Color Fade Blue: SLIDER, Position(0, 46), Size(100, 14), ID(IDC_SLIDER3), Range(-32, 32), Default(-8)
  - OnBeat Change: CHECKBOX, Position(0, 67), Size(67, 10), ID(IDC_CHECK3), Default(0)
  - Onbeat Randomize: CHECKBOX, Position(9, 79), Size(73, 10), ID(IDC_CHECK2), Default(0)
  - Onbeat Color Fade Red: SLIDER, Position(7, 90), Size(100, 14), ID(IDC_SLIDER4), Range(-32, 32), Default(8)
  - Onbeat Color Fade Green: SLIDER, Position(7, 107), Size(100, 14), ID(IDC_SLIDER5), Range(-32, 32), Default(-8)
  - Onbeat Color Fade Blue: SLIDER, Position(7, 125), Size(100, 14), ID(IDC_SLIDER6), Range(-32, 32), Default(-8)
- **Notes**:
  - Slider UI range is 0-64, stored value is slider_pos - 32
  - Algorithm determines dominant color channel and applies different fade values based on which channel dominates
  - c_tab[512][512] lookup table maps (g-b, b-r) to one of 4 fade categories
  - When OnBeat Change disabled: faderpos = faders (instant)
  - When OnBeat Change enabled: faderpos interpolates toward faders (1 step per frame)
  - When OnBeat Randomize enabled: on beat, faderpos is randomized

38. ### Grain
- **Purpose**: Add film grain effect
- **Source File**: `r_grain.cpp`
- **Inputs**:
  - Framebuffer
- **Outputs**: Grainy pixels
- **Blend Modes**: Blend amount
- **Controls**:
  - Enable Grain filter: CHECKBOX, Position(0, 0), Size(71, 10), ID(IDC_CHECK1)
  - Grain amount: SLIDER, Position(0, 12), Size(137, 13), ID(IDC_MAX)
  - Replace blend mode: RADIO_BUTTON, Position(0, 28), Size(43, 10), ID(IDC_REPLACE)
  - Additive blend mode: RADIO_BUTTON, Position(0, 38), Size(61, 10), ID(IDC_ADDITIVE)
  - Blend 50/50 mode: RADIO_BUTTON, Position(0, 49), Size(55, 10), ID(IDC_5050)
  - Static grain: CHECKBOX, Position(0, 60), Size(51, 10), ID(IDC_STATGRAIN)

39. ### Interferences
- **Purpose**: Wave interference patterns by sampling framebuffer at multiple offset positions
- **Source File**: `r_interf.cpp`
- **Legacy Index**: 41
- **Inputs**:
  - Framebuffer
  - Beat signal
- **Outputs**: Interference pattern to fbout (blended back to framebuffer)
- **Blend Modes**: Replace, Additive, 50/50
- **Controls**:
  - Enable Interferences: CHECKBOX, Position(0, 0), Size(81, 10), ID(IDC_CHECK1), Default(true)
  - Wave count: SLIDER, Position(0, 13), Size(67, 11), ID(IDC_NPOINTS), Range(0, 8), Default(2)
  - Alpha: SLIDER, Position(70, 12), Size(67, 11), ID(IDC_ALPHA), Range(1, 255), Default(128)
  - Rotation speed: SLIDER, Position(0, 33), Size(67, 11), ID(IDC_ROTATE), Range(-32, 32), Default(0)
    - Note: UI shows 0-64, value = -(slider_pos - 32)
  - Distance: SLIDER, Position(70, 33), Size(67, 11), ID(IDC_DISTANCE), Range(1, 64), Default(10)
  - Separate RGB: CHECKBOX, Position(2, 51), Size(62, 10), ID(IDC_RGB), Default(true)
    - Note: Only enabled when wave count is 3 or 6
  - OnBeat: CHECKBOX, Position(2, 60), Size(40, 10), ID(IDC_ONBEAT), Default(true)
  - Init Rotation: SLIDER, Position(70, 52), Size(67, 11), ID(IDC_INITROT), Range(0, 255), Default(0)
    - Note: UI shows 255 - value
  - OnBeat Speed: SLIDER, Position(4, 78), Size(63, 11), ID(IDC_SPEED), Range(1, 128), Default(20)
    - Note: Stored as speed = slider_pos / 100
  - OnBeat Alpha: SLIDER, Position(70, 78), Size(63, 11), ID(IDC_ALPHA2), Range(1, 255), Default(192)
  - OnBeat Rotation speed: SLIDER, Position(4, 99), Size(63, 11), ID(IDC_ROTATE2), Range(-32, 32), Default(25)
  - OnBeat Distance: SLIDER, Position(70, 99), Size(63, 11), ID(IDC_DISTANCE2), Range(1, 64), Default(32)
  - Replace blend mode: RADIO_BUTTON, Position(2, 123), Size(43, 10), ID(IDC_REPLACE), Value(0)
  - Additive blend mode: RADIO_BUTTON, Position(51, 123), Size(41, 10), ID(IDC_ADDITIVE), Value(1)
  - 50/50 blend mode: RADIO_BUTTON, Position(100, 123), Size(35, 10), ID(IDC_5050), Value(2)
- **Notes**:
  - Creates N "points" arranged in a circle at specified distance
  - Each point samples the framebuffer at an offset position
  - RGB mode (for 3 or 6 points): Each point samples only one color channel
  - On-beat: Smoothly transitions between normal and on-beat parameters using sin(status)
  - Uses pre-computed blend table for fast alpha blending

40. ### Interleave
- **Purpose**: Interlace/scanline effects
- **Source File**: `r_interleave.cpp`
- **Inputs**:
  - Framebuffer
- **Outputs**: Interlaced output
- **Blend Modes**: Replace, 50/50, Additive
- **Controls**:
  - Enable Interleave effect: CHECKBOX, Position(0, 0), Size(91, 10), ID(IDC_CHECK1)
  - X interleave amount: SLIDER, Position(0, 12), Size(137, 13), ID(IDC_X)
  - Y interleave amount: SLIDER, Position(0, 27), Size(137, 13), ID(IDC_Y)
  - Color: COLOR_BUTTON, Position(0, 43), Size(137, 13), ID(IDC_DEFCOL)
  - Replace blend mode: RADIO_BUTTON, Position(0, 58), Size(43, 10), ID(IDC_REPLACE)
  - 50/50 blend mode: RADIO_BUTTON, Position(42, 58), Size(35, 10), ID(IDC_5050)
  - Additive blend mode: RADIO_BUTTON, Position(77, 58), Size(29, 10), ID(IDC_ADDITIVE)
  - OnBeat: CHECKBOX, Position(0, 70), Size(40, 10), ID(IDC_CHECK8)
  - X interleave amount (onbeat): SLIDER, Position(0, 83), Size(137, 13), ID(IDC_X2)
  - Y interleave amount (onbeat): SLIDER, Position(0, 98), Size(137, 13), ID(IDC_Y2)
  - "Duration" label: LTEXT, Position(3, 115), Size(32, 8)
  - Beat duration: SLIDER, Position(41, 112), Size(95, 13), ID(IDC_X3)

41. ### Invert
- **Purpose**: Invert colors
- **Source File**: `r_invert.cpp`
- **Inputs**:
  - Framebuffer
- **Outputs**: Inverted colors
- **Blend Modes**: None
- **Controls**:
  - Enable Invert colors: CHECKBOX, Position(0, 0), Size(79, 10), ID(IDC_CHECK1)

42. ### Mirror
- **Purpose**: Mirror/flip transformations
- **Source File**: `r_mirror.cpp`
- **Inputs**:
  - Framebuffer
- **Outputs**: Mirrored pixels
- **Blend Modes**: None
- **Controls**:
  - Enable Mirror effect: CHECKBOX, Position(0, 0), Size(77, 10), ID(IDC_CHECK1)
  - Static mode: RADIO_BUTTON, Position(0, 20), Size(34, 10), ID(IDC_STAT)
  - OnBeat random mode: RADIO_BUTTON, Position(0, 29), Size(65, 10), ID(IDC_ONBEAT)
  - Copy Top to Bottom: CHECKBOX, Position(3, 44), Size(79, 10), ID(IDC_HORIZONTAL1)
  - Copy Bottom to Top: CHECKBOX, Position(3, 54), Size(88, 9), ID(IDC_HORIZONTAL2)
  - Copy Left to Right: CHECKBOX, Position(3, 63), Size(88, 10), ID(IDC_VERTICAL1)
  - Copy Right to Left: CHECKBOX, Position(3, 73), Size(98, 10), ID(IDC_VERTICAL2)
  - Smooth transitions: CHECKBOX, Position(0, 90), Size(73, 10), ID(IDC_SMOOTH)
  - Transition speed: SLIDER, Position(0, 103), Size(137, 11), ID(IDC_SLOWER)

43. ### Mosaic
- **Purpose**: Pixelate/mosaic effect
- **Source File**: `r_mosaic.cpp`
- **Inputs**:
  - Framebuffer
- **Outputs**: Pixelated output
- **Blend Modes**: Replace, Additive, 50/50
- **Controls**:
  - Enable Mosaic: CHECKBOX, Position(0, 0), Size(65, 10), ID(IDC_CHECK1)
  - Block size: SLIDER, Position(0, 13), Size(137, 11), ID(IDC_QUALITY)
  - "Bigger squares" label: LTEXT, Position(0, 23), Size(48, 8)
  - "Smaller squares" label: LTEXT, Position(87, 23), Size(50, 8)
  - Replace blend mode: RADIO_BUTTON, Position(0, 33), Size(43, 10), ID(IDC_REPLACE)
  - Additive blend mode: RADIO_BUTTON, Position(0, 43), Size(61, 10), ID(IDC_ADDITIVE)
  - Blend 50/50 mode: RADIO_BUTTON, Position(0, 53), Size(55, 10), ID(IDC_5050)
  - OnBeat: CHECKBOX, Position(0, 63), Size(40, 10), ID(IDC_ONBEAT)
  - OnBeat block size: SLIDER, Position(0, 74), Size(137, 11), ID(IDC_QUALITY2)
  - "Bigger" label: LTEXT, Position(0, 84), Size(21, 8)
  - "Smaller" label: LTEXT, Position(113, 84), Size(24, 8)
  - OnBeat duration: SLIDER, Position(0, 93), Size(137, 11), ID(IDC_BEATDUR)
  - "Shorter" label: LTEXT, Position(0, 103), Size(24, 8)
  - "Longer" label: LTEXT, Position(114, 103), Size(23, 8)

44. ### Multi Delay
- **Purpose**: Echo/delay effect
- **Source File**: `r_multidelay.cpp`
- **Inputs**:
  - Framebuffer
  - History buffer
- **Outputs**: Delayed/echoed pixels
- **Blend Modes**: Multiple blend modes
- **Controls**:
  - Disabled Mode: RADIO_BUTTON, Position(12, 18), Size(48, 12), ID(1100)
  - Input Mode: RADIO_BUTTON, Position(72, 18), Size(30, 12), ID(1101)
  - Output Mode: RADIO_BUTTON, Position(132, 18), Size(36, 12), ID(1102)
  - Active Buffer A: RADIO_BUTTON, Position(12, 48), Size(48, 12), ID(1000)
  - Active Buffer B: RADIO_BUTTON, Position(72, 48), Size(48, 12), ID(1001)
  - Active Buffer C: RADIO_BUTTON, Position(132, 48), Size(42, 12), ID(1002)
  - Active Buffer D: RADIO_BUTTON, Position(12, 60), Size(48, 12), ID(1003)
  - Active Buffer E: RADIO_BUTTON, Position(72, 60), Size(48, 12), ID(1004)
  - Active Buffer F: RADIO_BUTTON, Position(132, 60), Size(42, 12), ID(1005)
  - Delay Value Buffer A: EDITTEXT, Position(12, 90), Size(42, 12), ID(1010)
  - Use Beats Buffer A: RADIO_BUTTON, Position(12, 102), Size(42, 12), ID(1020)
  - Use Frames Buffer A: RADIO_BUTTON, Position(12, 114), Size(42, 12), ID(1030)
  - Delay Value Buffer B: EDITTEXT, Position(72, 90), Size(42, 12), ID(1011)
  - Use Beats Buffer B: RADIO_BUTTON, Position(72, 102), Size(42, 12), ID(1021)
  - Use Frames Buffer B: RADIO_BUTTON, Position(72, 114), Size(42, 12), ID(1031)
  - Delay Value Buffer C: EDITTEXT, Position(132, 90), Size(42, 12), ID(1012)
  - Use Beats Buffer C: RADIO_BUTTON, Position(132, 102), Size(42, 12), ID(1022)
  - Use Frames Buffer C: RADIO_BUTTON, Position(132, 114), Size(42, 12), ID(1032)
  - Delay Value Buffer D: EDITTEXT, Position(12, 144), Size(42, 12), ID(1013)
  - Use Beats Buffer D: RADIO_BUTTON, Position(12, 156), Size(42, 12), ID(1023)
  - Use Frames Buffer D: RADIO_BUTTON, Position(12, 168), Size(42, 12), ID(1033)
  - Delay Value Buffer E: EDITTEXT, Position(72, 144), Size(42, 12), ID(1014)
  - Use Beats Buffer E: RADIO_BUTTON, Position(72, 156), Size(42, 12), ID(1024)
  - Use Frames Buffer E: RADIO_BUTTON, Position(72, 168), Size(42, 12), ID(1034)
  - Delay Value Buffer F: EDITTEXT, Position(132, 144), Size(42, 12), ID(1015)
  - Use Beats Buffer F: RADIO_BUTTON, Position(132, 156), Size(42, 12), ID(1025)
  - Use Frames Buffer F: RADIO_BUTTON, Position(132, 168), Size(42, 12), ID(1035)

45. ### Multiplier
- **Purpose**: Multiplier
- **Source File**: `r_multiplier.cpp`
- **Inputs**:
  - Framebuffer
- **Outputs**: Multiplied pixels
- **Blend Modes**: None
- **Controls**:
  - Color x 8: RADIO_BUTTON, Position(10, 29), Size(60, 10), ID(IDC_X8)
  - Color x 4: RADIO_BUTTON, Position(10, 42), Size(60, 10), ID(IDC_X4)
  - Color x 2: RADIO_BUTTON, Position(10, 55), Size(60, 10), ID(IDC_X2)
  - Color x 0.5: RADIO_BUTTON, Position(10, 68), Size(60, 10), ID(IDC_X05)
  - Color x 0.25: RADIO_BUTTON, Position(10, 81), Size(60, 10), ID(IDC_X025)
  - Color x 0.125: RADIO_BUTTON, Position(10, 94), Size(60, 10), ID(IDC_X0125)
  - Infinite root: RADIO_BUTTON, Position(10, 16), Size(60, 10), ID(IDC_XI)
  - Infinite square: RADIO_BUTTON, Position(10, 107), Size(60, 10), ID(IDC_XS)

46. ### Unique tone
- **Purpose**: Posterize with unique colors
- **Source File**: `r_onetone.cpp`
- **Inputs**:
  - Framebuffer
- **Outputs**: Posterized output
- **Blend Modes**: None
- **Controls**:
  - Enable Unique tone: CHECKBOX, Position(0, 0), Size(79, 10), ID(IDC_CHECK1)
  - Color: COLOR_BUTTON, Position(0, 15), Size(137, 13), ID(IDC_DEFCOL)
  - Invert: CHECKBOX, Position(0, 30), Size(34, 10), ID(IDC_INVERT)
  - Replace blend mode: RADIO_BUTTON, Position(0, 41), Size(43, 10), ID(IDC_REPLACE)
  - Additive blend mode: RADIO_BUTTON, Position(0, 51), Size(61, 10), ID(IDC_ADDITIVE)
  - Blend 50/50 mode: RADIO_BUTTON, Position(0, 61), Size(55, 10), ID(IDC_5050)

47. ### Video Delay
- **Purpose**: Frame delay/echo
- **Source File**: `r_videodelay.cpp`
- **Inputs**:
  - Framebuffer
  - Frame history
- **Outputs**: Delayed frames
- **Blend Modes**: Blend with current
- **Controls**:
  - Use Beats: RADIO_BUTTON, Position(54, 18), Size(36, 12), ID(IDC_RADIO1)
  - Use Frames: RADIO_BUTTON, Position(90, 18), Size(36, 12), ID(IDC_RADIO2)
  - Enabled: CHECKBOX, Position(6, 7), Size(42, 8), ID(IDC_CHECK1)
  - Delay value: EDITTEXT, Position(6, 18), Size(43, 12), ID(IDC_EDIT1)

48. ### Water
- **Purpose**: Water ripple effect
- **Source File**: `r_water.cpp`
- **Inputs**:
  - Framebuffer
- **Outputs**: Rippled output
- **Blend Modes**: None
- **Controls**:
  - Enable Water effect: CHECKBOX, Position(0, 0), Size(79, 10), ID(IDC_CHECK1)

49. ### Water Bump
- **Purpose**: Water surface simulation
- **Source File**: `r_waterbump.cpp`
- **Inputs**:
  - Framebuffer
  - Height map
- **Outputs**: Refracted pixels
- **Blend Modes**: None
- **Controls**:
  - Enable Water bump effect: CHECKBOX, Position(0, 0), Size(99, 10), ID(IDC_CHECK1)
  - Water density: SLIDER, Position(5, 21), Size(130, 12), ID(IDC_DAMP)
  - Random drop position: CHECKBOX, Position(2, 51), Size(85, 10), ID(IDC_RANDOM_DROP)
  - Drop position Left: RADIO_BUTTON, Position(6, 72), Size(28, 10), ID(IDC_DROP_LEFT)
  - Drop position Center (X): RADIO_BUTTON, Position(46, 72), Size(37, 10), ID(IDC_DROP_CENTER)
  - Drop position Right: RADIO_BUTTON, Position(95, 72), Size(33, 10), ID(IDC_DROP_RIGHT)
  - Drop position Top: RADIO_BUTTON, Position(6, 84), Size(29, 10), ID(IDC_DROP_TOP)
  - Drop position Middle (Y): RADIO_BUTTON, Position(46, 84), Size(37, 10), ID(IDC_DROP_MIDDLE)
  - Drop position Bottom: RADIO_BUTTON, Position(95, 84), Size(38, 10), ID(IDC_DROP_BOTTOM)
  - Drop depth: SLIDER, Position(5, 108), Size(56, 12), ID(IDC_DEPTH)
  - Drop radius: SLIDER, Position(74, 108), Size(58, 12), ID(IDC_RADIUS)


## Misc Effects
Utility and control effects.

50. ### Buffer Save
- **Purpose**: Save/restore framebuffer to/from a buffer slot, with various blending options.
- **Source File**: `r_stack.cpp`
- **Dialog**: IDD_CFG_STACK (233x219)
- **Legacy Index**: 18
- **Inputs**:
  - Current framebuffer
  - Selected buffer slot
- **Outputs**: Framebuffer modified by save/restore and blending
- **Blend Modes**: Replace, 50/50, Additive, Every other pixel, Every other line, Subtractive 1, Subtractive 2, Xor, Maximum, Minimum, Multiply, Adjustable
- **Controls**:
  - Save framebuffer: RADIO_BUTTON, Position(5, 7), Size(70, 10), ID(IDC_SAVEFB)
  - Restore framebuffer: RADIO_BUTTON, Position(5, 17), Size(78, 10), ID(IDC_RESTFB)
  - Alternate Save/Restore: RADIO_BUTTON, Position(5, 28), Size(91, 10), ID(IDC_RADIO1)
  - Alternate Restore/Save: RADIO_BUTTON, Position(5, 38), Size(91, 10), ID(IDC_RADIO7)
  - Nudge parity: BUTTON, Position(100, 31), Size(50, 14), ID(IDC_BUTTON2)
  - Clear buffer: BUTTON, Position(99, 11), Size(50, 14), ID(IDC_BUTTON1)
  - "Buffer:" label: LTEXT, Position(8, 55), Size(22, 8)
  - Buffer slot: DROPDOWN, Position(32, 53), Size(74, 90), ID(IDC_COMBO1)
  - "Blending" groupbox: GROUPBOX, Position(8, 68), Size(172, 138)
  - Replace blend mode: RADIO_BUTTON, Position(12, 79), Size(43, 10), ID(IDC_RSTACK_BLEND1)
  - 50/50 blend mode: RADIO_BUTTON, Position(12, 89), Size(35, 10), ID(IDC_RSTACK_BLEND2)
  - Additive blend mode: RADIO_BUTTON, Position(12, 99), Size(41, 10), ID(IDC_RSTACK_BLEND3)
  - Every other pixel blend mode: RADIO_BUTTON, Position(12, 109), Size(68, 10), ID(IDC_RSTACK_BLEND4)
  - Every other line blend mode: RADIO_BUTTON, Position(12, 118), Size(65, 10), ID(IDC_RSTACK_BLEND6)
  - Subtractive 1 blend mode: RADIO_BUTTON, Position(12, 128), Size(58, 10), ID(IDC_RSTACK_BLEND5)
  - Subtractive 2 blend mode: RADIO_BUTTON, Position(12, 138), Size(58, 10), ID(IDC_RSTACK_BLEND10)
  - Xor blend mode: RADIO_BUTTON, Position(12, 148), Size(27, 10), ID(IDC_RSTACK_BLEND7)
  - Maximum blend mode: RADIO_BUTTON, Position(12, 158), Size(45, 10), ID(IDC_RSTACK_BLEND8)
  - Minimum blend mode: RADIO_BUTTON, Position(12, 168), Size(43, 10), ID(IDC_RSTACK_BLEND9)
  - Multiply blend mode: RADIO_BUTTON, Position(12, 178), Size(39, 10), ID(IDC_RSTACK_BLEND11)
  - Adjustable blend mode: RADIO_BUTTON, Position(12, 188), Size(51, 10), ID(IDC_RSTACK_BLEND12)
  - Adjustable blend value: SLIDER, Position(62, 188), Size(111, 11), ID(IDC_BLENDSLIDE)

51. ### Comment
- **Purpose**: Add comments to preset
- **Source File**: `r_comment.cpp`
- **Legacy Index**: 21
- **Inputs**: None
- **Outputs**: None
- **Blend Modes**: N/A
- **Controls**:
  - Comment text: EDITTEXT, Position(0, 0), Size(233, 214), ID(IDC_EDIT1)

52. ### Custom BPM
- **Purpose**: Override automatic BPM detection
- **Source File**: `r_bpm.cpp`
- **Dialog**: IDD_CFG_BPM (137x137)
- **Legacy Index**: 33
- **Inputs**:
  - Beat signal (isBeat)
- **Outputs**: Modified beat signal (SET_BEAT=0x10000000 or CLR_BEAT=0x20000000)
- **Blend Modes**: N/A
- **Return Values**:
  - `SET_BEAT` (0x10000000): Force a beat this frame
  - `CLR_BEAT` (0x20000000): Cancel/clear any beat this frame
  - `0`: Pass through beat signal unchanged
- **Controls**:
  - Enable BPM Customizer: CHECKBOX, Position(0, 0), Size(137, 10), ID(IDC_CHECK1), Default(true)
  - Arbitrary mode: RADIO_BUTTON, Position(0, 14), Size(41, 10), ID(IDC_ARBITRARY), Default(true)
  - Arbitrary value (ms): SLIDER, Position(38, 14), Size(65, 11), ID(IDC_ARBVAL), Range(200, 10000), Default(500)
  - Skip mode: RADIO_BUTTON, Position(0, 24), Size(30, 10), ID(IDC_SKIP), Default(false)
  - Skip value (beats): SLIDER, Position(38, 24), Size(65, 11), ID(IDC_SKIPVAL), Range(1, 16), Default(1)
  - Reverse/Invert mode: RADIO_BUTTON, Position(0, 34), Size(43, 10), ID(IDC_INVERT), Default(false)
  - "First skip" label: LTEXT, Position(0, 45), Size(28, 8)
  - Skip first N beats: SLIDER, Position(38, 45), Size(65, 11), ID(IDC_SKIPFIRST), Range(0, 64), Default(0)
  - "In" label: LTEXT, Position(0, 57), Size(8, 8)
  - In indicator: SLIDER, Position(13, 56), Size(124, 11), ID(IDC_IN), Range(0, 8), Disabled (visual feedback)
  - "Out" label: LTEXT, Position(0, 67), Size(12, 8)
  - Out indicator: SLIDER, Position(13, 66), Size(124, 11), ID(IDC_OUT), Range(0, 8), Disabled (visual feedback)
  - Dynamic value labels: IDC_ARBTXT ("1s"), IDC_SKIPTXT ("1 beat"), IDC_SKIPFIRSTTXT ("1 beat")
- **Mode Behavior**:
  - **Arbitrary**: Generate beats at fixed intervals (ignores incoming beats)
    - Value is milliseconds between beats (200ms = 300 BPM, 500ms = 120 BPM)
  - **Skip**: Pass through every Nth beat, cancel others
    - Value of 1 = pass every 2nd beat, value of 2 = pass every 3rd beat, etc.
  - **Invert/Reverse**: Swap beats and non-beats
    - Returns CLR_BEAT on beats, SET_BEAT on non-beats
  - **Skip First**: Initial beat suppression
    - First N incoming beats are cancelled regardless of other modes
- **Notes**:
  - Modes are mutually exclusive (arbitrary overrides skip, skip overrides invert)
  - Effect does not render pixels, only modifies beat signal
  - In/Out sliders are visual-only beat indicators that animate on input/output beats

53. ### Effect List
- **Purpose**: Container for grouping effects with optional scripted control and compositing
- **Source File**: `r_list.cpp`
- **Implementation Status**: COMPLETE - all 14 blend modes, beat-gated rendering, and script execution
- **Inputs**:
  - Parent framebuffer (blended into internal buffer via input blend)
  - Audio data (passed to scripts and child effects)
- **Outputs**: Internal buffer (blended back to parent framebuffer via output blend)
- **Internal Buffer**: Allocates separate `thisfb` buffer for compositing
- **Data Flow**:
  1. Parent framebuffer → [INPUT BLEND] → internal buffer
  2. Internal buffer → [CHILD EFFECTS] → internal buffer
  3. Internal buffer → [OUTPUT BLEND] → parent framebuffer
- **Blend Modes** (14 modes, same for input and output):
  | Mode | Name | Description |
  |------|------|-------------|
  | 0 | Ignore | Unused |
  | 1 | Replace | Direct copy (optimization: skip blending if both are Replace) |
  | 2 | 50/50 | Average blend |
  | 3 | Maximum | Per-channel maximum |
  | 4 | Additive | Add pixel values |
  | 5 | Subtractive 1 | Destination minus source |
  | 6 | Subtractive 2 | Source minus destination |
  | 7 | Every other line | Interlaced scanlines |
  | 8 | Every other pixel | Checkerboard pattern |
  | 9 | XOR | Bitwise XOR |
  | 10 | Adjustable | Alpha-controlled blend (uses slider 0-255) |
  | 11 | Multiply | Per-channel multiplication |
  | 12 | Buffer | Uses global buffer as per-pixel alpha mask |
  | 13 | Minimum | Per-channel minimum |
- **Controls (Root List / IDD_CFG_LISTROOT)**:
  - Clear every frame: CHECKBOX, Position(0, 0), Size(71, 10), ID(IDC_CHECK1)
- **Controls (Sub-list / IDD_CFG_LIST)**:
  - Enabled: CHECKBOX, Position(0, 2), Size(56, 10), ID(IDC_CHECK2)
  - Clear every frame: CHECKBOX, Position(0, 13), Size(71, 10), ID(IDC_CHECK1)
  - Input blending mode: DROPDOWN, Position(5, 37), Size(101, 184), ID(IDC_COMBO2)
  - Input blend slide: SLIDER, Position(1, 51), Size(106, 11), ID(IDC_INSLIDE) (Initially hidden)
  - Output blending mode: DROPDOWN, Position(121, 37), Size(101, 168), ID(IDC_COMBO1)
  - Output blend slide: SLIDER, Position(117, 51), Size(106, 11), ID(IDC_OUTSLIDE)
  - Input buffer: DROPDOWN, Position(5, 50), Size(62, 67), ID(IDC_CBBUF1) (Initially hidden)
  - Input invert: CHECKBOX, Position(72, 51), Size(34, 10), ID(IDC_INVERT1) (Initially hidden)
  - Output buffer: DROPDOWN, Position(121, 50), Size(62, 64), ID(IDC_CBBUF2) (Initially hidden)
  - Output invert: CHECKBOX, Position(187, 52), Size(34, 10), ID(IDC_INVERT2) (Initially hidden)
  - Enabled OnBeat for: CHECKBOX, Position(89, 3), Size(78, 9), ID(IDC_CHECK3)
  - Beat render frames: EDITTEXT, Position(167, 1), Size(19, 12), ID(IDC_EDIT1)
  - Use evaluation override: CHECKBOX, Position(0, 78), Size(197, 10), ID(IDC_CHECK4)
  - Init script: EDITTEXT, Position(25, 93), Size(208, 26), ID(IDC_EDIT4)
  - Frame script: EDITTEXT, Position(25, 119), Size(208, 71), ID(IDC_EDIT5)
  - Expression help: BUTTON, Position(25, 193), Size(67, 14), ID(IDC_BUTTON2)
- **Script Variables** (when "Use evaluation override" is enabled):
  | Variable | Type | Description |
  |----------|------|-------------|
  | `beat` | R/W | Beat flag (read incoming, write to pass/block to children) |
  | `enabled` | R/W | Enable/disable entire effect list (0=disabled, non-zero=enabled) |
  | `clear` | R/W | Clear framebuffer before rendering children |
  | `alphain` | R/W | Input blend alpha (0.0-1.0, maps to 0-255) |
  | `alphaout` | R/W | Output blend alpha (0.0-1.0, maps to 0-255) |
  | `w` | R | Frame width in pixels |
  | `h` | R | Frame height in pixels |
- **Script Execution**:
  - Init script runs once when effect list is created or code changes
  - Frame script runs every frame before child effects render
  - Scripts execute before blending/clear decisions are applied
- **Example Use Cases**:
  - Beat gating: `beat = beat * (count % 4 == 0); count = count + beat;`
  - Pulsing visibility: `enabled = sin(t) > 0; t = t + 0.1;`
  - Beat-reactive fade: `alphaout = beat ? 1.0 : alphaout * 0.95;`
  - Conditional clear: `clear = beat;`

54. ### Transition (NOT PORTING)
- **Purpose**: Define global preset transition behaviors and settings.
- **Source File**: `r_transition.cpp`
- **Status**: Not porting - this is not an effect but global infrastructure for transitioning between presets.
- **Why**: Requires dual renderer instances running in parallel, Windows threading APIs, and tight integration with preset loading system. Would need to be implemented as renderer infrastructure in ofxAVS rather than as an avs_lib effect.
- **Transition Modes** (for reference):
  - Cross dissolve
  - L/R Push, R/L Push, T/B Push, B/T Push
  - 9 Random Blocks
  - Split L/R Push
  - L/R to Center Push, L/R to Center Squeeze
  - L/R Wipe, R/L Wipe, T/B Wipe, B/T Wipe
  - Dot Dissolve

55. ### Beat Detector
- **Purpose**: Detect beats from audio input for use by effects
- **Source File**: `bpm.cpp`
- **Dialog**: IDD_GCFG_BPM (137x137)
- **Inputs**:
  - Audio waveform data
- **Outputs**: Beat signal (isBeat flag passed to effects)
- **Blend Modes**: N/A
- **Controls**: (Configured in Global BPM Settings / IDD_GCFG_BPM)
  - "Beat detection settings" groupbox: GROUPBOX, Position(0, 0), Size(137, 130)
  - Standard mode: RADIO_BUTTON, Position(4, 11), Size(45, 10), ID(IDC_BPMSTD)
  - Advanced mode: RADIO_BUTTON, Position(4, 21), Size(49, 10), ID(IDC_BPMADV)
  - In indicator: SLIDER, Position(55, 11), Size(79, 11), ID(IDC_IN), Range(0, 8), Disabled (visual feedback)
  - Out indicator: SLIDER, Position(55, 21), Size(79, 11), ID(IDC_OUT), Range(0, 8), Disabled (visual feedback)
  - "Advanced status" groupbox: GROUPBOX, Position(4, 33), Size(128, 92)
  - "Current BPM :" label: LTEXT, Position(8, 43), Size(45, 8), ID(IDC_CURBPM)
  - BPM value: LTEXT, Position(55, 43), Size(70, 8), ID(IDC_BPM) (dynamic)
  - "Confidence :" label: LTEXT, Position(8, 52), Size(41, 8), ID(IDC_CURCONF)
  - Confidence value: LTEXT, Position(55, 52), Size(71, 8), ID(IDC_CONFIDENCE) (dynamic)
  - Auto-keep: CHECKBOX, Position(8, 61), Size(49, 10), ID(IDC_STICKY)
  - Predict only if bpm has been found: CHECKBOX, Position(8, 70), Size(121, 10), ID(IDC_ONLYSTICKY)
  - Reset: BUTTON, Position(7, 80), Size(23, 10), ID(IDC_RESET)
  - Keep: BUTTON, Position(34, 80), Size(29, 10), ID(IDC_STICK)
  - Readapt: BUTTON, Position(66, 80), Size(29, 10), ID(IDC_UNSTICK)
  - "New Song" groupbox: GROUPBOX, Position(8, 90), Size(121, 32)
  - Adapt from known BPM: RADIO_BUTTON, Position(13, 99), Size(91, 10), ID(IDC_NEWADAPT)
  - Restart learning from scratch: RADIO_BUTTON, Position(13, 109), Size(106, 10), ID(IDC_NEWRESET)
  - 2x: BUTTON (not in original, added for convenience)
  - /2: BUTTON (not in original, added for convenience)
- **Mode Behavior**:
  - **Standard mode**: Raw energy-based beat detection. The `isBeat` signal is passed
    directly to effects based on audio energy crossing a threshold. No BPM tracking,
    no prediction, no sticky behavior.
  - **Advanced mode**: Full BPM tracking with prediction. Detected beats are refined
    using tempo history, allowing prediction of beats even during quiet passages.
    Sticky mode locks to a detected tempo. All controls are enabled.
- **Implementation Notes**:
  - In/Out sliders are visual-only indicators that animate on input/output beats
  - The "On new song" feature requires Winamp playlist integration (song change detection)

56. ### Set Render Mode
- **Purpose**: Control rendering pipeline
- **Source File**: `r_linemode.cpp`
- **Inputs**:
  - Mode selection
- **Outputs**: Rendering configuration
- **Blend Modes**: N/A
- **Controls**:
  - Enable mode change: CHECKBOX, Position(0, 2), Size(83, 10), ID(IDC_CHECK1)
  - Set blend mode to: DROPDOWN, Position(5, 25), Size(128, 184), ID(IDC_COMBO1)
  - Adjustable Blend Alpha: SLIDER, Position(1, 39), Size(132, 11), ID(IDC_ALPHASLIDE)
  - Line width (pixels): EDITTEXT, Position(58, 53), Size(20, 12), ID(IDC_EDIT1)

---

## Global Settings

These are host application settings, not effects. They configure the AVS renderer behavior.

### Display Settings (IDD_GCFG_DISP)
- **Purpose**: Configure windowed display behavior
- **Controls**:
  - Wait for retrace: CHECKBOX, Position(5, 10), Size(65, 10), ID(IDC_CHECK6)
  - Pixel doubling (half resolution): CHECKBOX, Position(5, 20), Size(115, 10), ID(IDC_CHECK1)
  - Windowed performance (Framerate): SLIDER, Position(4, 40), Size(127, 14), ID(IDC_SLIDER1)
  - Suppress status text in windowed: CHECKBOX, Position(5, 67), Size(121, 10), ID(IDC_CHECK3)
  - Suppress title text in windowed: CHECKBOX, Position(5, 77), Size(113, 10), ID(IDC_CHECK5)
  - Reuse image on window resize: CHECKBOX, Position(5, 88), Size(113, 10), ID(IDC_CHECK2)
  - Enable transparency (Win2k/XP only): CHECKBOX, Position(5, 122), Size(99, 10), ID(IDC_TRANS_CHECK)
  - Transparency slider: SLIDER, Position(6, 140), Size(119, 10), ID(IDC_TRANS_SLIDER)
  - Overlay color: COLOR_BUTTON, Position(146, 23), Size(40, 10), ID(IDC_OVERLAYCOLOR)
  - Default overlay color: BUTTON, Position(190, 23), Size(40, 10), ID(IDC_DEFOVERLAYCOLOR)
  - Set desktop to color: CHECKBOX, Position(146, 76), Size(82, 11), ID(IDC_SETDESKTOPCOLOR)
  - Thread priority: DROPDOWN, Position(144, 138), Size(86, 65), ID(IDC_THREAD_PRIORITY)

### Fullscreen Settings (IDD_GCFG_FS)
- **Purpose**: Configure fullscreen display behavior
- **Controls**:
  - Use fullscreen overlay mode: CHECKBOX, Position(0, 0), Size(214, 10), ID(IDC_USE_OVERLAY)
  - Resolution: DROPDOWN, Position(2, 42), Size(199, 180), ID(IDC_COMBO1)
  - Go fullscreen: BUTTON, Position(2, 57), Size(23, 13), ID(IDC_BUTTON1)
  - Fullscreen timeout (minutes): EDITTEXT, Position(134, 72), Size(20, 12), ID(IDC_EDIT1)
  - Keep fullscreen when deactivated: CHECKBOX, Position(0, 155), Size(186, 10), ID(IDC_CHECK8)
  - Pixel doubling: CHECKBOX, Position(0, 84), Size(59, 10), ID(IDC_CHECK1)
  - Wait for retrace: CHECKBOX, Position(0, 94), Size(65, 10), ID(IDC_CHECK7)
  - Page flipping (slower, smoother): CHECKBOX, Position(0, 104), Size(115, 10), ID(IDC_CHECK4)
  - Fast software bpp conversion: CHECKBOX, Position(0, 114), Size(105, 10), ID(IDC_BPP_CONV)
  - Display framerate counter: CHECKBOX, Position(0, 125), Size(96, 10), ID(IDC_CHECK2)
  - Suppress status text in fullscreen: CHECKBOX, Position(0, 135), Size(119, 10), ID(IDC_CHECK3)
  - Suppress title text in fullscreen: CHECKBOX, Position(0, 145), Size(111, 10), ID(IDC_CHECK5)
  - Rendering performance (Framerate): SLIDER, Position(0, 177), Size(137, 14), ID(IDC_SLIDER1)

### Preset Settings (IDD_GCFG_PRESET)
- **Purpose**: Configure preset hotkeys and random switching
- **Controls**:
  - Hotkey slot: DROPDOWN, Position(5, 11), Size(64, 203), ID(IDC_COMBO1)
  - Save to hotkey: BUTTON, Position(5, 26), Size(56, 14), ID(IDC_BUTTON1)
  - Load from hotkey: BUTTON, Position(66, 26), Size(56, 14), ID(IDC_BUTTON2)
  - Randomly switch presets: CHECKBOX, Position(0, 180), Size(93, 10), ID(IDC_CHECK3)
  - Every N seconds: EDITTEXT, Position(38, 191), Size(21, 12), ID(IDC_EDIT1)
  - Preset directory: BUTTON, Position(0, 161), Size(233, 15), ID(IDC_BUTTON3)
  - Prompt to save unsaved presets: CHECKBOX, Position(0, 124), Size(204, 10), ID(IDC_CHECK1)

### Transition Settings (IDD_GCFG_TRANSITIONS)
- **Purpose**: Configure preset transition effects
- **Controls**:
  - Enable pre-init on random: CHECKBOX, Position(0, 0), Size(96, 10), ID(IDC_CHECK3)
  - Enable pre-init on next/prev: CHECKBOX, Position(0, 10), Size(103, 10), ID(IDC_CHECK11)
  - Enable pre-init on load preset: CHECKBOX, Position(0, 20), Size(107, 10), ID(IDC_CHECK10)
  - Low priority pre-init: CHECKBOX, Position(0, 31), Size(74, 10), ID(IDC_CHECK4)
  - Only pre-init when in fullscreen: CHECKBOX, Position(0, 41), Size(111, 10), ID(IDC_CHECK5)
  - Enable transition on random: CHECKBOX, Position(0, 79), Size(137, 10), ID(IDC_CHECK8)
  - Enable transition on next/prev: CHECKBOX, Position(0, 89), Size(137, 10), ID(IDC_CHECK1)
  - Enable transition on load preset: CHECKBOX, Position(0, 99), Size(126, 10), ID(IDC_CHECK2)
  - Keep rendering old effect: CHECKBOX, Position(0, 111), Size(126, 10), ID(IDC_CHECK9)
  - Transition type: DROPDOWN, Position(0, 133), Size(137, 392), ID(IDC_TRANSITION)
  - Transition speed: SLIDER, Position(0, 149), Size(137, 11), ID(IDC_SPEED)
- **Transition Types**:
  - Cross dissolve
  - L/R Push, R/L Push, T/B Push, B/T Push
  - 9 Random Blocks
  - Split L/R Push
  - L/R to Center Push, L/R to Center Squeeze
  - L/R Wipe, R/L Wipe, T/B Wipe, B/T Wipe
  - Dot Dissolve