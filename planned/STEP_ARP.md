# Step Arpeggio

Per-step arpeggio for CV tracks. Each step can be assigned an arp type that walks a
musical interval pattern across its sub-steps (ratchets or repeats) or across successive
playhead passes (plain steps).

---

## Overview

The existing gate clock follow feature lets a CV track advance in sync with a gate track's
ratchets and repeats. Step Arp adds a pitch dimension: instead of playing the same note on
each advance, the CV track walks through a defined chord or scale fragment. On ratcheted
steps this produces a strum effect; on repeated steps a melodic roll; on plain steps a
slow cross-pattern arpeggio that cycles with the clock.

Arp type is per-step, stored in the step data. It is **mutually exclusive with probability**
on CV tracks: setting an arp type clears probability on that step, and setting probability
clears arp type. Gate tracks are unaffected by the arp feature.

---

## Entry and Controls

**Mode:** SHIFT + GLIDE (existing probability mode, `src/ui/seq_prob.hh`)

Behavior when a **CV track** is selected:
- Encoder **CW from 0** → increases probability (existing behavior, unchanged)
- Encoder **CCW from 0** → arp type 1; CCW again → type 2; ... up to type 10
- Turning CW from an arp type decreases toward 0 (off); continuing past 0 enters probability
- Setting arp clears probability on that step; setting probability clears arp

Types 1–5 are ascending; types 6–10 are the descending equivalents of the same chords.
Total of 10 types, one per distinct palette color. The encoder LED shows the arp type
color (see Display section).

**Gate track selected:** unchanged — CW/CCW adjusts gate skip probability as before.

---

## Arp Type Library

Intervals are semitones relative to the step's programmed (quantized) pitch. The scale
quantizer is **bypassed** for arp intervals — offsets are raw semitones so chord shapes
stay musically exact regardless of CV scale mode.

Ascending types (CCW 1–5) walk upward from the root. Descending types (CCW 6–10) walk
downward. Type 0 = off.

| CCW | Direction | Name | Intervals (semitones) |
|-----|-----------|------|-----------------------|
| 1 | Up | Octave | 0, +12 |
| 2 | Up | Perfect 5th | 0, +7 |
| 3 | Up | Major Triad | 0, +4, +7 |
| 4 | Up | Minor Triad | 0, +3, +7 |
| 5 | Up | Major 7th | 0, +4, +7, +11 |
| 6 | Down | Octave | 0, −12 |
| 7 | Down | Perfect 5th | 0, −7 |
| 8 | Down | Major Triad | 0, −4, −7 |
| 9 | Down | Minor Triad | 0, −3, −7 |
| 10 | Down | Major 7th | 0, −4, −7, −11 |

Notes cycle: when the advance count exceeds the arp length, it wraps back to position 0
(root). E.g., Major Triad (3 notes) on a 4x ratchet: root, +4, +7, root.

**Suggested display colors** (to be confirmed against palette during implementation):
- Types 1–5 (ascending): teal, green, yellow, orange, red — cool-to-warm ramp
- Types 6–10 (descending): blue, lavender, magenta, salmon, pink — distinguishable cool set

---

## Playback Behavior

### On ratcheted steps

Each ratchet sub-step plays the next note in the arp. Sub-step 0 → root (the step's
programmed pitch). Sub-step 1 → first interval, etc., wrapping cyclically.

**Masked sub-steps:** a sub-step silenced by the mask produces no output but still
advances the arp position. Timing pattern stays consistent; pitch-order density does not.

### On repeated steps

Same as ratchets: each repeat tick (including the initial step fire) advances the arp
position. Repeat tick 0 → root; subsequent ticks → next intervals.

### On plain steps (no ratchet or repeat)

The arp plays across successive playhead passes. Each time the clock advances through the
step, the arp position increments by one — strictly one advance per pass, no rate
setting. After the last interval the position resets to 0 (root) for the next pass.

This produces a slow arpeggio at pattern rate: a 4-note arp on an 8-step sequence cycles
through all four pitches over four loops.

**Player state required:** a transient per-channel arp position counter (not saved).
Resets to 0 on Play/Reset.

### With gate clock follow

When a CV track is in gate clock follow mode, each clock-follow advance (ratchet sub-step,
repeat tick, or step boundary, per the follow mode) increments the arp position. This is
the primary strum use case: a ratcheted gate track clocks the CV track; the CV track's
arp walks a chord across the ratchet sub-steps in real time.

### Interaction with CV add/replace follow (Blue/Lavender)

Arp intervals are applied to the base step pitch before CV follow is computed. A
Blue-mode transposer therefore shifts the entire arpeggiated output, which is musically
correct (arp + transposition stack additively).

---

## Display and Feedback

### In probability mode (SHIFT + GLIDE), CV track selected

- **Probability active (CW > 0):** existing `Palette::Probability::color()` — dim-to-bright
  cool teal/blue.
- **Arp active (CCW > 0):** one of the 10 arp-type colors (see table above). A step with
  arp set lights its encoder in that color at full brightness; a step with no arp is off.
- **Neither (0):** off.

The two color families (cool probability vs. warm/mixed arp) should read as visually
distinct axes.

---

## Data Model

### Step struct — arp type storage

`sub_step_mask` (8 bits) is currently used only by gate channels; all 8 bits are unused
on CV channels. Repurpose the **lower 4 bits** as `arp_type` on CV channels (0 = off,
1–10 used, 11–15 reserved). Upper 4 bits remain 0 for CV channels.

No struct size change. Requires a **`current_tag` bump** because the existing default
`0xFF` would be misread as `arp_type = 15` on CV steps from old saves. Coordinate with
other v1.4.6 tag bumps to do this once.

```
sub_step_mask : 8
  Gate channels:  bits 0–7 = sub-step enable bitmask (unchanged)
  CV channels:    bits 0–3 = arp type (0=off, 1–10=type, 11–15=reserved)
                  bits 4–7 = 0 (reserved, must be masked off at read)
```

New accessors on `Step`:
```cpp
uint8_t ReadArpType() const       { return sub_step_mask & 0x0Fu; }
void    SetArpType(uint8_t t)     { sub_step_mask = (t & 0x0Fu); } // clears upper nibble
bool    HasArp() const            { return ReadArpType() != 0; }
```

### Player / App state (transient, not saved)

```cpp
std::array<uint8_t, NumChans> arp_pos{};  // current index within arp sequence (0 = root)
```

Reset to all-zero in `Reset()`. Increment logic runs in `Interface::Update()` or
`App::Cv()` when the channel advances and the current step has arp set.

---

## Implementation Notes

- **`src/sequencer_step.hh`** — add `ReadArpType()`, `SetArpType()`, `HasArp()`.
  Add `IncArpType(int32_t inc)` analogous to `IncProbability`. Validate: upper nibble
  of `sub_step_mask` must be 0 for CV channels (or simply mask at read).
  Add `constexpr` arp interval table, e.g.:
  ```cpp
  constexpr std::array<std::array<int8_t, 4>, 10> arp_intervals = {{
      { 0, 12,  0,  0 }, // 1: Octave up
      { 0,  7,  0,  0 }, // 2: P5 up
      { 0,  4,  7,  0 }, // 3: Major triad up
      { 0,  3,  7,  0 }, // 4: Minor triad up
      { 0,  4,  7, 11 }, // 5: Major 7th up
      { 0,-12,  0,  0 }, // 6: Octave down
      { 0, -7,  0,  0 }, // 7: P5 down
      { 0, -4, -7,  0 }, // 8: Major triad down
      { 0, -3, -7,  0 }, // 9: Minor triad down
      { 0, -4, -7,-11 }, // 10: Major 7th down
  }};
  // arp_length[type-1] = number of non-sentinel entries (or use a separate lengths array)
  ```

- **`src/app.hh` `Cv()`** — after computing the quantized `stepval`, if the step has arp:
  look up `arp_intervals[type-1][arp_pos[chan]]`, convert semitones to
  `Channel::Cv::type` units (1 semitone = `(max - min) / (voltage_range_in_volts * 12)`),
  add to `stepval` (clamped to DAC range).

- **`src/sequencer.hh` `Interface::Update()`** — after `player.Update()`, for each CV
  channel where `per_chan_step[chan] > 0` and the current step has arp: increment
  `arp_pos[chan]` modulo the arp length for that type. Also handle plain-step advances
  from the master clock tick.

- **`src/sequencer.hh` `Interface::Reset()`** — `arp_pos.fill(0)`.

- **`src/sequencer.hh` `Interface::IncStepProbability()`** — on CV tracks, if the
  resulting probability would go below 0, call `SetArpType` with the magnitude instead,
  and set probability to 0. If transitioning from arp back to probability (positive inc
  from a negative arp state), clear arp type and set probability.

- **`src/ui/seq_prob.hh` `PaintLeds()`** — for CV track, branch on `HasArp()` vs
  probability for color. Arp steps show arp-type color; probability steps show existing
  probability color.

- **`src/sequencer_settings.hh` `current_tag`** — bump.

---

## Pre-Implementation Checklist

- [ ] Confirm semitone-to-DAC scalar from `channel.hh` / `model.hh` voltage range
- [ ] Verify `sizeof(Step) <= 8` still holds after any other v1.4.6 bitfield additions
- [ ] Confirm 10 arp-type colors read as distinct on hardware (palette review)
- [ ] Confirm `IncStepProbability` sign convention aligns with desired CW/CCW feel
- [ ] Decide whether `arp_pos` lives in `Interface` (alongside `repeat_ticks_remaining`)
      or in `App` (alongside `replace_latch`) — both are plausible
