# Phase Scrub Performance Page — Spec

This document covers the full design and implementation plan for the Phase Scrub
Performance Page (entered via COPY+GLIDE held 3 seconds). Features marked
**[done]** are implemented as of v1.4.6-alpha3.

---

## Page Layout

| Encoder | Function | Colors | Status |
|---------|----------|--------|--------|
| 1 | Quantize | orange = on, off = off | [done] |
| 2 | Slider Performance Mode | off / green / blue / cyan | [done] |
| 3 | Granular Width | off = 0%, dim→bright as width increases | planned |
| 4 | Granular / Beat Repeat Direction (Spray) | 4 colors, one per mode | planned |
| 8 | Phase Scrub Lock | red = locked, off = unlocked | [done] |

**Page buttons:** per-track scrub ignore (lit = track follows scrub, unlit = ignored) **[done]**

**Exit:** COPY+GLIDE (any duration) or Play/Reset. **[done]**

---

## Encoder 1 — Quantize [done]

When on, the slider phase value snaps to the nearest step boundary:
`apply_phase = round(live_phase / step_size) * step_size`
where `step_size = 1 / pattern_length`.

Snaps immediately and deterministically. No intersection logic, no lag.

---

## Encoder 2 — Slider Performance Mode [done, behavior planned]

Clamped selector. CW increments, CCW decrements. Hard stops at both ends.

| Value | LED | Slider behavior |
|-------|-----|-----------------|
| 0 | off | Standard phase scrub (default) |
| 1 | green | Granular sequencing |
| 2 | blue | Beat repeat (interleaved triplets) |
| 3 | cyan | Reserved (future) |

The mode is stored and displayed in alpha3. Modes 1-2 activate full behavior
once granular and beat repeat are implemented.

---

## Encoder 3 — Granular Width [planned]

Width of the orbit window as a percentage of pattern length (0-100%). 0% = orbit
off (slider falls back to standard phase scrub). Stored in `Shared::Data::orbit_width`.

Display: off at 0; dim orange at narrow widths, brighter as width increases.

---

## Encoder 4 — Spray Direction [planned]

Controls orbit playback direction within the window. Also applies to beat repeat
direction when in beat repeat mode. Potentially reversed envelopes in future.

| Value | LED | Direction |
|-------|-----|-----------|
| 0 | green | Forward |
| 1 | blue | Backward |
| 2 | orange | Ping-pong |
| 3 | lavender | Random |

Stored in `Shared::Data::orbit_direction`.

---

## Granular Mode (Enc 2 = green) [planned]

The slider positions an orbit window within the sequence. The sequencer loops
steps within that window at the normal clock rate. Analogous to granular
synthesis: slider = grain position, enc 3 = grain size, enc 4 = spray direction.

**Slider → orbit center:** `center_step[chan] = round(slider_normalized * length[chan])`

**Window:** `[center_step, center_step + width_steps)` wrapping at pattern end,
where `width_steps = max(1, round(orbit_width_pct / 100.0 * length[chan]))`.

**Orbit advance** (each clock tick per channel):
- Forward: `orbit_pos = (orbit_pos + 1) % window_size`
- Backward: `orbit_pos = (orbit_pos + window_size - 1) % window_size`
- Ping-pong: `orbit_pos` bounces between 0 and window_size-1, flipping
  `orbit_ping_dir` flag at each end
- Random: `orbit_pos = rand() % window_size`

**Slider at minimum (0):** orbit_width_steps rounds to 0 → orbit inactive,
standard phase scrub resumes.

**Morph/glide within orbit:** `orbit_step_prev[chan]` tracks the previous orbit
step so `App::Cv()` can interpolate between consecutive orbit steps normally.

**Per-track ignore:** `scrub_ignore_mask` applies — ignored channels continue
normal playback.

---

## Beat Repeat Mode (Enc 2 = blue) [planned]

The slider sets a beat division at which the sequencer loops a single step.
At slider = 0: off, normal playback. At slider > 0: loops the current step
at the selected subdivision rate.

A brief debounce (~150ms of stability in a zone) before committing a new
division, so the performer can swipe quickly past divisions without accidentally
triggering them. The panel has a printed scale with 32 tick marks; highlighted
marks at every 4th tick (8th-note boundaries) land exactly on zone boundaries.

### Division mapping — 8 zones, interleaved triplets (blue)

8 equal zones of 512 counts across 1–4095. 4 physical tick marks per zone.

| Zone | Slider range | Division | Note value | Ticks between advances |
|------|-------------|----------|------------|----------------------|
| 0 | 1 – 512 | 1/2 | half | `beat_period * 2` |
| 1 | 513 – 1024 | 1/4 | quarter | `beat_period` |
| 2 | 1025 – 1536 | 1/4T | quarter triplet | `beat_period * 2 / 3` |
| 3 | 1537 – 2048 | 1/8 | eighth | `beat_period / 2` |
| 4 | 2049 – 2560 | 1/8T | eighth triplet | `beat_period / 3` |
| 5 | 2561 – 3072 | 1/16 | sixteenth | `beat_period / 4` |
| 6 | 3073 – 3584 | 1/16T | sixteenth triplet | `beat_period / 6` |
| 7 | 3585 – 4095 | 1/32 | thirty-second | `beat_period / 8` |

Where `beat_period = 3000 * 60 / GetGlobalDividedBpm()` (ISR ticks per quarter note).

### Beat repeat advance

A `beat_repeat_countdown` counter decrements each ISR tick. When it reaches 0,
the orbit position advances one step (per spray direction) and the countdown
reloads. This is independent of `clock_ticked` — beat repeat runs at its own
rate derived from BPM.

**Enc 4 (direction)** may be repurposed in beat repeat mode for reversed
playback + reversed envelope outputs. Spec TBD.

---

## Implementation Plan

### Data changes (no tag bump — fits in reserved space)

`Shared::Data` — replace `uint16_t _reserved1 = 0` with:
```cpp
uint8_t orbit_width = 0;      // % of pattern length (0-100)
uint8_t orbit_direction = 0;  // 0=fwd, 1=bck, 2=ping-pong, 3=random
```

`Shared::Interface` — add transient fields (not persisted):
```cpp
float orbit_center = 0.f;               // normalized slider pos, set by UI each tick
uint8_t beat_repeat_committed = 0xFF;   // 0xFF = off; 0-6 = active division index
uint8_t beat_repeat_pending = 0xFF;     // candidate zone (debouncing)
uint32_t beat_repeat_pending_since = 0; // tick when candidate zone was entered
```

### `src/sequencer.hh`

New state in `Interface`:
```cpp
bool orbit_active = false;
uint8_t orbit_pos = 0;
bool orbit_ping_dir = true;
std::array<uint8_t, Model::NumChans> orbit_step{};
std::array<uint8_t, Model::NumChans> orbit_step_prev{};
uint32_t beat_repeat_countdown = 0;
```

New accessors:
```cpp
bool OrbitActive() const { return orbit_active; }
uint8_t GetOrbitStep(uint8_t chan) const { return orbit_step[chan]; }
uint8_t GetOrbitStepPrev(uint8_t chan) const { return orbit_step_prev[chan]; }
```

In `Reset()`: clear orbit_active, orbit_pos, orbit_ping_dir, orbit_step, orbit_step_prev.

In `Update()`: after clock tick detection block, add orbit advance logic that:
1. Reads `shared.data.slider_perf_mode` and `shared.orbit_center`
2. Computes orbit window start and width per channel
3. Advances `orbit_pos` per direction mode when appropriate (clock tick for
   granular; beat_repeat_countdown for beat repeat)
4. Fills `orbit_step[]` and updates `orbit_step_prev[]`
5. Sets `orbit_active`

### `src/app.hh`

`Cv()`: when `p.OrbitActive()`:
```cpp
const auto current_step = p.OrbitActive()
    ? p.slot.channel[chan][p.GetOrbitStep(chan)]
    : p.GetRelativeStep(chan, 0);
const auto prev_step = p.OrbitActive()
    ? p.slot.channel[chan][p.GetOrbitStepPrev(chan)]
    : p.GetRelativeStep(chan, -1);
```

`Gate()`: when `p.OrbitActive()`, override the step cluster with orbit steps
(current = orbit_step, prev = orbit_step_prev, next = orbit_step or adjacent).

### `src/ui/seq_common.hh`

In `Usual::Common()`, when `p.shared.data.slider_perf_mode > 0`:
- Set `p.shared.orbit_center = slider_now / 4095.f`
- For beat repeat: decode slider zone, run debounce, commit to
  `p.shared.beat_repeat_committed`
- Pass `phase = 0` to `p.Update()` (orbit overrides phase scrub entirely)

### `src/ui/seq_scrub_settings.hh`

Wire enc 3 → `orbit_width` (0-100, CW increases), enc 4 → `orbit_direction`
(0-3 cycling).

---

## Post-implementation Notes

### v1.4.6-alpha1 calibration corruption bug

**Symptom:** ~12 cents flat per octave on all CV pitch tracks after upgrading from v1.4.5. Flashing back to v1.4.5 appears to fix it.

**Root cause:** The v1.4.5→v1.4.6 migration in `saved_settings.hh` used a local `V1_4_5_SharedData` overlay struct to read flash. In alpha1, this struct was missing `alignas(4)` on its `Model::Mode saved_mode` member. Because `Mode` is `enum class : uint8_t` (1 byte), without `alignas(4)` the compiler placed `dac_calibration` at offset 6 instead of the correct offset 8 (where 3 bytes of padding exist in the actual struct). The overlay therefore read calibration from the wrong position in flash — picking up 2 bytes of padding (0x0000) as channel[0].offset, and then reading each subsequent calibration field one field too early.

The garbled values happened to pass `Calibration::Dac::Data::Validate()`, so alpha1 wrote the corrupted calibration to flash under tag+2. After that, all subsequent alpha firmware reads tag+2 directly without re-migrating, so the corruption persists across all upgrades.

The specific pitch error arises because each channel's slope was set to the value of the preceding channel's offset. `ApplySlope` multiplies the distance from 0V by `(1 + slope/max_slope * max_adjustment_volts)`. If the corrupted slope is ~10% of max_slope (e.g. an offset value of ~44 DAC counts out of 437 max), the result is ~10% of 120 cents = ~12 cents per octave of systematic pitch drift.

**Why flash-back appears to fix it:** v1.4.5-rc1 uses `CurrentSharedSettingsVersionTag = tag+1`. It does not recognize tag+2, falls through all migration paths to `return false`, and boots with default calibration (offset=0, slope=0). If the hardware tracks acceptably without correction, uncalibrated output appears correct compared to the corrupted-calibrated output.

**Fix (implemented in alpha6/v1.4.6):** `CurrentSharedSettingsVersionTag` bumped to tag+3. A new migration branch for tag+2 (`V1_4_6_Alpha_SharedSettingsVersionTag`) reads all fields except `dac_calibration`, which is left at defaults. Users affected by the alpha1/2 bug must recalibrate from the DAC calibration menu. Users who only ran alpha3+ and had correct calibration in tag+2 also need to recalibrate (there is no way to distinguish correct from corrupted tag+2 data).

**Lesson for future migrations:** When writing a local overlay struct for flash migration, always verify that `alignas` specifiers on members match those in the original struct. Pay particular attention to `enum class : uint8_t` fields declared `alignas(4)` — the underlying type is 1 byte, so without explicit alignment the compiler packs the next member tighter than the original. Always add a `static_assert(sizeof(LocalOverlay) == sizeof(OriginalStruct))` (or a size check against the expected value) before merging any migration that uses a locally-defined overlay type.

---

## Pre-Implementation Checklist

- [ ] Confirm `Clock::BpmToTicks()` or equivalent exists for beat period calculation
- [ ] Verify orbit step override in `App::Cv()` handles morph interpolation correctly
- [ ] Confirm `App::Gate()` cluster override doesn't break trig delay overlap logic
- [ ] Test: orbit at width=100% should be audibly identical to normal playback (fwd)
- [ ] Test: slider to 0 cleanly resumes normal playback without click or step skip
- [ ] `orbit_pos` does NOT reset when slider moves to a new center — the window
      moves under the continuously-advancing position counter (granular-synth
      behavior). If quantize is on, snap `orbit_center_step` to the nearest step
      boundary before computing `orbit_start[chan]`.
