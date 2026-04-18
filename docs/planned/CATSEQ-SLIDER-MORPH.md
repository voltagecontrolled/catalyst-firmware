# Scene Morph — Feature Spec

A new Performance Page mode (`slider_perf_mode = 4`) that turns the Phase Scrub slider into a bipolar live-offset gesture for per-channel step values. Named for its conceptual similarity to Octatrack scenes — two defined states with the slider as the crossfader.

---

## Concept

- **Slider center** = zero offset; the sequence plays exactly as programmed.
- **Slider left of center** → lerp toward each channel's **low anchor** (negative offset).
- **Slider right of center** → lerp toward each channel's **high anchor** (positive offset).
- Returning the slider to center returns all offsets to zero — no artificial release needed.
- Anchors are offsets, not absolute values. The stored step data is never modified.
- All offsets are clamped to valid ranges at output time. Anchor editing is unconstrained.
- Anchors persist with the preset (saved via the Glide+Chan. gesture).

---

## What Is Offset Per Channel Type

| Channel type | Parameter | Notes |
|---|---|---|
| CV | Output voltage | Additive voltage offset on the current step output, applied before quantization. Clamped to hardware output range. |
| Gate | Gate length per step | Additive % offset on each step's gate length. Clamped 0–100%. |
| Trigger | Ratchet/repeat count per step | Additive count offset on each step's ratchet/repeat value. Clamped to valid range. |

---

## Anchor Editing

While in the Performance Page with this mode active, modifier keys repurpose the encoders for anchor editing:

| Modifier | Input | Action |
|---|---|---|
| Shift held | Encoder N | Adjust channel N **low anchor** (negative offset) |
| Chan. held | Encoder N | Adjust channel N **high anchor** (positive offset) |

One modifier at a time. Increment per detent:
- CV: ±1V
- Gate: ±5% per detent
- Trigger: ±1 count per detent

Encoder LEDs during anchor editing:
- CV: transpose color scheme (same as Channel Edit enc 7)
- Gate: brightness = anchor magnitude
- Trigger: ratchet color scheme

---

## Perf Settings Parameters

### Enc 2 — Curve

Response curve shaping how aggressively slider position maps to the output offset. Reuses the enc 2 slot used by orbit_width (granular) and beat_repeat_debounce (beat-repeat).

- Center (0): linear — unlit
- CW: logarithmic — most of the range change near center, diminishing toward extremes. LED: blue, dim at +1 to half-brightness at max
- CCW: exponential — subtle near center, dramatic near extremes. LED: red, dim at −1 to half-brightness at max
- Stored as `morph_curve_idx` (signed, 0 = linear)

### Enc 3 — Dead Zone

Symmetric region around center where the slider reads as zero offset. Prevents accidental morphing from small hand movements or vibration. Outside the dead zone, the offset ramps normally from zero toward the anchor. Reuses the enc 3 slot used by orbit_direction (granular).

- Range: 0 (no dead zone) to ~40% of half-travel in each direction
- LED: brightness = dead zone width (unlit = none, bright = wide)
- Stored as `morph_deadzone_idx`

---

## Shared Behavior

### Page buttons

Toggle the same `scrub_ignore_mask` used by all Perf Page modes. Lit = channel is morphed by the slider; unlit = channel plays straight.

### Shift to disengage

Holding Shift freezes the current morph offset — slider movement has no effect while Shift is held. Consistent with the same behavior in beat-repeat mode. Useful for locking a morph position during a performance moment without physically holding the slider still.

### Phase Scrub Lock

Same behavior as other modes: lock freezes all slider input; pickup mode applies on unlock.

### Pickup on disarm

On disarm (exit armed mode) while this mode is active, the slider home position for pickup is **center** (ADC midpoint) rather than fully left, since center = zero offset.

### Motion gate

Reuses `record_slider_prev_` / `record_active_until_` from `Common()`. No change to detection logic — the output destination changes based on `slider_perf_mode`.

---

## Data Model

### New fields in `Shared::Data`

```cpp
struct ValuesMorphAnchor {
    int16_t cv_low,    cv_high;   // millivolts
    int8_t  gate_low,  gate_high; // gate length offset %
    int8_t  trig_low,  trig_high; // ratchet/repeat count offset
};

ValuesMorphAnchor morph_anchors[NumChans];
int8_t            morph_curve_idx;     // 0 = linear, >0 = log (blue), <0 = exp (red)
uint8_t           morph_deadzone_idx;
```

### Tag bumps required

- Bump `CurrentSharedSettingsVersionTag` and add a migration case for the new fields.
- `slider_perf_mode` valid range extends from 0–3 to 0–4; clamp on load.

---

## UI / LED

### Perf Settings enc 1 — mode selector

| Value | Mode | LED color |
|---|---|---|
| 0 | Disabled | Off |
| 1 | Granular | Green |
| 2 | Beat-repeat ×8 | Blue |
| 3 | Beat-repeat ×4 | Cyan |
| 4 | Channel Values Morph | Magenta |

### Perf Page (morph mode active)

- **Encoder LEDs:** current output step color per channel (same as existing Perf Page normal).
- **Enc 2:** off = linear; blue dim→half-bright = log (CW); red dim→half-bright = exp (CCW).
- **Enc 3:** brightness = dead zone width (unlit = none, bright = wide).
- **Slider moving:** encoders briefly show offset magnitude per channel, then revert to step color when motion stops.
- **Page buttons:** scrub_ignore_mask (lit = morphed, unlit = straight).
