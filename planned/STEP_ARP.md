# Step Arpeggio

Per-step arpeggio for CV tracks. Each step can be assigned an arp type that walks a
musical interval pattern across its sub-steps (ratchets or repeats) or across successive
playhead passes (plain steps).

---

## Overview

The existing gate clock follow feature lets a CV track advance in sync with a gate track's
ratchets and repeats. Step Arp adds a pitch dimension: instead of playing the same note on
each advance, the CV track walks a chord or scale fragment. On ratcheted steps this
produces a strum; on repeated steps a melodic roll; on plain steps a slow cross-pattern
arpeggio that cycles with the clock.

Arp type and direction are per-step, stored in the step data. Arp is **mutually exclusive
with probability** on CV tracks: setting an arp type clears the step's probability, and
setting probability clears the arp type. Gate tracks are unaffected.

---

## Entry and Controls

### Quick edit — SHIFT + GLIDE (existing probability mode)

Behavior when a **CV track** is selected:

- Encoder **CW from 0** → increases probability (existing, unchanged)
- Encoder **CCW from 0** → arp type 1; CCW again → type 2; … up to type 6
- Turning CW from an arp type decreases toward 0 (off); continuing past 0 enters
  probability territory
- Setting arp clears probability for that step; setting probability clears arp

This mode requires holding the button combo throughout. Direction is not editable here —
use the persistent mode below.

---

### Persistent mode — SHIFT + GLIDE + TAP TEMPO (toggle)

A persistent step-arp/randomness editor that stays active after releasing all buttons.
Ergonomically equivalent to SHIFT+GLIDE but hands-free, and adds per-step direction
control.

**Entry:** while in probability mode (SHIFT+GLIDE held), tap TAP TEMPO. Mode persists
after releasing all buttons.

**Exit:** TAP TEMPO alone, or PLAY/RESET.

**Inside the mode:**

- **Step encoders:** same CW/CCW behavior as SHIFT+GLIDE — CW adjusts probability, CCW
  cycles arp type. All 8 steps on the current page are editable without holding anything.
- **Page buttons (1 per step):** toggle the arp direction for the corresponding step.
  - **Unlit** = ascending (default)
  - **Lit** = descending
  - Only has effect when the step has an arp type set; ignored (and unlit) for steps with
    probability or neither.
- **SHIFT + PAGE button:** change page of steps without exiting the mode, for sequences
  longer than 8 steps. Same pattern as the planned sub-step page navigation.

---

## Arp Type Library

Six types, each with an ascending and descending variant controlled by the step's
direction flag. Intervals are semitones relative to the step's quantized base pitch. The
scale quantizer is **bypassed** for arp intervals — offsets are raw semitones so chord
shapes stay musically exact regardless of the track's CV scale mode.

| CCW | Name | Ascending intervals | Descending intervals |
|-----|------|---------------------|----------------------|
| 1 | Major 5th (triad) | 0, +4, +7 | 0, −4, −7 |
| 2 | Minor 5th (triad) | 0, +3, +7 | 0, −3, −7 |
| 3 | Major 7th | 0, +4, +7, +11 | 0, −4, −7, −11 |
| 4 | Minor 7th | 0, +3, +7, +10 | 0, −3, −7, −10 |
| 5 | Octave | 0, +12 | 0, −12 |
| 6 | Minor Pentatonic | 0, +3, +5, +7, +10 | 0, −3, −5, −7, −10 |

Notes cycle: when advances exceed the arp length, the sequence wraps to position 0
(root/base pitch). E.g., Major 5th (3 notes) on a 4x ratchet: root, +4st, +7st, root.

**Display colors** — one distinct color per type (direction shown via page button state,
not color). Suggested mapping to be confirmed against hardware during implementation:

| CCW | Type | Suggested color |
|-----|------|-----------------|
| 1 | Major 5th | yellow |
| 2 | Minor 5th | teal |
| 3 | Major 7th | orange |
| 4 | Minor 7th | blue |
| 5 | Octave | lavender |
| 6 | Minor Pentatonic | green |

---

## Playback Behavior

### On ratcheted steps

Each ratchet sub-step plays the next arp note. Sub-step 0 → base pitch. Sub-step 1 →
first interval, etc., wrapping cyclically.

**Masked sub-steps:** a silenced sub-step produces no output but still advances the arp
position. Timing rhythm stays consistent; pitch spacing may have gaps.

### On repeated steps

Same as ratchets: each repeat tick (including the initial step fire) advances the arp
position.

### On plain steps (no ratchet or repeat)

The arp plays across successive playhead passes. Each time the clock advances through
the step, the arp position increments by one. After the last interval the position resets
to 0 (root) for the next pass.

A 4-note arp on an 8-step sequence cycles through all four pitches over four pattern
loops. No rate setting — it follows the clock.

**Player state required:** a transient per-channel arp position counter (not saved to
flash). Resets to 0 on Play/Reset.

### With gate clock follow

When a CV track is in gate clock follow mode, each clock-follow advance increments the
arp position. This is the primary strum use case: a ratcheted gate track clocks the CV
track; the CV track's arp walks a chord across the ratchet sub-steps in real time.

### Interaction with CV follow modes (Blue/Lavender)

Arp intervals are applied to the base step pitch before CV follow is computed. A
Blue-mode transposer shifts the entire arpeggiated output in unison (arp + transpose
stack additively). Lavender intersection logic is evaluated against the arpeggiated
output.

---

## Display and Feedback

### In SHIFT+GLIDE (quick edit) and persistent mode

Encoder LEDs per step:
- **Arp active:** arp-type color at full brightness (see color table above)
- **Probability active:** existing `Palette::Probability::color()` teal/blue scale
- **Neither:** off

### In persistent mode — page buttons

- **Lit:** step has arp set AND direction = descending
- **Unlit:** step has arp set AND direction = ascending, OR step has no arp

---

## Data Model

### Step struct changes

`sub_step_mask` (8 bits) is unused on CV channels. Assign:

```
sub_step_mask : 8  (CV channels)
  bits 0–2:  arp type (0 = off, 1–6 = type index, 7 = reserved)
  bit  3:    arp direction (0 = ascending, 1 = descending)
  bits 4–7:  reserved, must be 0
```

No struct size change. Requires a **`current_tag` bump**: old saves have `sub_step_mask`
defaulting to `0xFF` on CV channels, which would misread as `arp_type = 7` + direction
descending. Coordinate with other v1.4.6 tag bumps.

New accessors on `Step`:
```cpp
uint8_t ReadArpType() const    { return sub_step_mask & 0x07u; }
bool    ReadArpDown() const    { return (sub_step_mask >> 3) & 0x01u; }
bool    HasArp() const         { return ReadArpType() != 0; }
void    SetArpType(uint8_t t)  { sub_step_mask = (sub_step_mask & 0x08u) | (t & 0x07u); }
void    ToggleArpDir()         { sub_step_mask ^= 0x08u; }
void    ClearArp()             { sub_step_mask = 0; }
```

### Player / Interface state (transient, not saved)

```cpp
std::array<uint8_t, NumChans> arp_pos{};  // current index within arp sequence (0 = root)
```

Reset to all-zero in `Reset()`.

---

## Implementation Notes

- **`src/sequencer_step.hh`** — add accessors above. Add `IncArpType(int32_t inc)` that
  clamps to 0–6. Add interval table:
  ```cpp
  // [type_index 0..5][note_index 0..4], sentinel = 0 at end
  constexpr int8_t arp_intervals[6][5] = {
      { 0,  4,  7,  0,  0 }, // 1: Major 5th
      { 0,  3,  7,  0,  0 }, // 2: Minor 5th
      { 0,  4,  7, 11,  0 }, // 3: Major 7th
      { 0,  3,  7, 10,  0 }, // 4: Minor 7th
      { 0, 12,  0,  0,  0 }, // 5: Octave
      { 0,  3,  5,  7, 10 }, // 6: Minor Pentatonic
  };
  constexpr uint8_t arp_lengths[6] = { 3, 3, 4, 4, 2, 5 };
  ```
  Apply direction in `App::Cv()` by negating the interval when `ReadArpDown()`.

- **`src/app.hh` `Cv()`** — after computing `stepval`, if step has arp:
  ```cpp
  const auto arp_type = current_step.ReadArpType();
  if (arp_type > 0) {
      const auto pos = arp_pos[chan] % arp_lengths[arp_type - 1];
      auto semitones = arp_intervals[arp_type - 1][pos];
      if (current_step.ReadArpDown()) semitones = -semitones;
      stepval += semitones_to_cv(semitones); // see below
  }
  ```
  `semitones_to_cv`: `static_cast<int32_t>((Channel::Cv::max - Channel::Cv::min) /
  (voltage_range * 12.f) * semitones)` — verify scalar from `model.hh`.

- **`src/sequencer.hh` `Interface::Update()`** — after `player.Update()`, for CV
  channels where `per_chan_step[chan] > 0` and current step has arp, increment
  `arp_pos[chan]` (no modulo here; modulo applied at read so wrapping is always
  relative to the current step's arp length even if the step type changed).

- **`src/sequencer.hh` `Interface::Reset()`** — `arp_pos.fill(0)`.

- **`src/sequencer.hh` `Interface::IncStepProbability()`** — on CV tracks, CCW when
  probability == 0: call `SetArpType` instead (magnitude of inc, clamped 0–6). CW from
  arp type > 0: decrement arp type toward 0; continuing past 0 enters probability.
  When transitioning from arp to probability, call `ClearArp()` first.

- **`src/ui/seq_prob.hh`** — add `PaintLeds` branch for arp-type colors. Update `Update()`
  to handle the TAP TEMPO combo entering the persistent mode.

- **`src/ui/seq_step_arp.hh`** (new) — persistent step-arp editor, analogous to
  `seq_substep_mask.hh`:
  - Encoder turns → `IncStepProbability` (CW) / `IncArpType` (CCW)
  - Page button press → `ToggleArpDir` for that step (only if step has arp)
  - SHIFT+PAGE → change page without exiting
  - TAP TEMPO / PLAY+RESET → exit

- **`src/sequencer_settings.hh` `current_tag`** — bump; coordinate with v1.4.6.

---

## Pre-Implementation Checklist

- [ ] Confirm semitone-to-CV scalar from `model.hh` voltage range constants
- [ ] Verify `sizeof(Step) <= 8` after all v1.4.6 bitfield additions
- [ ] Confirm 6 arp-type colors read as visually distinct on hardware LEDs
- [ ] Confirm entry combo (SHIFT+GLIDE held, then TAP TEMPO) doesn't conflict with
      existing substep mask entry (GLIDE+TAP TEMPO on gate tracks)
- [ ] Decide whether `arp_pos[]` lives in `Interface` (alongside `repeat_ticks_remaining`)
      or in `App` (alongside `replace_latch`)
