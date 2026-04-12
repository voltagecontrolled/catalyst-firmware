# Catalyst VoltSeq — Per-Step Probability & Random Deviation

Stochastic playback layer. Two independent mechanisms:

1. **Step probability** — chance that a step fires at all (Gate/Trigger) or deviates from its recorded value (CV).
2. **Random deviation amount** — how far a CV step can deviate when probability fires (CV only; channel-level).

Both are edited in Armed mode via `SHIFT+Glide+Enc`. Deviation amount is also editable in Channel Edit (enc 7).

---

## Gate / Trigger Channels

### Step Probability

| | |
|---|---|
| **What it does** | Per-step chance of firing. 100% = always fires (default). 0% = never fires. |
| **Gesture** | `SHIFT+Glide+Enc[i]` in armed mode — enc i maps to step i on the current page. |
| **Direction** | CW = more randomness = lower probability. CCW = less randomness = higher probability. |
| **Range** | 100% (deterministic) → 0% (silent). |
| **Default** | 100% (step always fires — no randomness). |

### Channel Edit (enc 7)

Inactive and unlit for Gate/Trigger channels.

---

## CV Channels

### Step Probability (of Deviation)

| | |
|---|---|
| **What it does** | Per-step chance that a random deviation is applied to the recorded step value. 0% = always plays as recorded (default). 100% = always deviates. |
| **Gesture** | `SHIFT+Glide+Enc[i]` in armed mode — enc i maps to step i on the current page. |
| **Direction** | CW = higher probability of deviation. CCW = lower probability. |
| **Range** | 0% (off) → 100% (always deviates). |
| **Default** | 0% (step plays exactly as recorded). |

### Deviation Amount (Channel Edit, enc 7)

Sets the maximum deviation in **volts**, and its type (unipolar or bipolar). Stored as a signed integer — the sign encodes the type; the magnitude encodes the amount.

| Value | Type | Effective range of deviation |
|-------|------|------------------------------|
| `+N` (CW from zero) | **Unipolar (up)** | `[step, step + N]` — deviation only pushes upward |
| `0` | Off | No deviation regardless of probability |
| `−N` (CCW from zero) | **Bipolar (±)** | `[step − N, step + N]` — deviation is symmetric around recorded value |

- Output is clamped to the channel's voltage window `[transpose, transpose + span]` — deviation cannot exceed the window boundaries.
- Quantization and scale are applied after the random offset, same as recorded steps.
- Stored as `int8_t` in whole volts. Range: −15 V to +15 V.

---

## LED Color Maps

### Encoder LEDs — Step Probability (SHIFT+Glide held in armed mode)

The 8 encoder LEDs show the probability of each step on the current page.

| Probability | Color |
|-------------|-------|
| Off (0% for CV, 100% for Gate/Trig) | dim / off |
| Low | violet |
| Mid | grey |
| High | white |

Both channel types use the same violet→grey→white ramp. The direction of "increasing randomness" is opposite between types, but the color meaning is consistent: more saturated/bright = more randomness applied.

### Encoder LED — Deviation Amount (Channel Edit enc 7 / Armed SHIFT enc 7)

| Value | Color |
|-------|-------|
| Zero (off) | off |
| Positive / unipolar, low amount | grey |
| Positive / unipolar, high amount | white (via blue) |
| Negative / bipolar, low amount | salmon |
| Negative / bipolar, high amount | white (via red) |

Ramp: unipolar = grey → blue → white; bipolar = salmon → red → white.

---

## Playback Behavior

On each step advance, for each channel:

1. If `step_probability[step] == 0` (or 100% for Gate/Trig default): play as recorded, no random check.
2. Otherwise draw a random number 0–100. If it is `≤ step_probability[step]` (CV) or `> step_probability[step]` (Gate/Trig):
   - **Gate/Trigger:** suppress the gate/trigger output for this step.
   - **CV:** compute `deviation = random_float_in_range(amount, bipolar)`, apply to recorded step value, clamp to window, then quantize/scale.
3. Random number is drawn once per step advance, not continuously.

---

## Armed Mode UI — SHIFT+Glide held

While SHIFT and Glide are both held in armed mode:

- The 8 encoder LEDs switch from their normal step-color display to **step probability** display (violet→grey→white ramp per step).
- Turning an encoder adjusts that step's probability.
- When SHIFT or Glide is released, the display reverts to normal.
- Gate/Trigger channels: enc active, probability editing enabled.
- CV channels: enc active, deviation probability editing enabled.
- If the armed channel is Gate/Trigger: enc 7 is unlit (no deviation amount for these types).
- If the armed channel is CV: enc 7 shows deviation amount color (same as Channel Edit enc 7).

---

## Data Model Changes

### `ChannelSettings` (in `voltseq.hh`)

`random_amount: float` currently exists as a stub (0–1). Replace with:

```cpp
int8_t random_amount_v = 0; // signed volts; + = unipolar, − = bipolar, 0 = off
```

The existing `float random_amount` is referenced in the Channel Edit LED paint for enc 7
(brightness blend). Update that paint path to use the new signed-volt representation.

### Per-step probability

Add a parallel array to `ChannelData` (separate from `StepValue` to avoid changing
the StepValue encoding):

```cpp
std::array<uint8_t, MaxSteps> step_prob{}; // per-step probability 0–100; default 0 for CV, 100 for Gate/Trig
```

`sizeof(ChannelData)` increases by `MaxSteps` bytes per channel (64 × 8 = 512 bytes total).
Bump `current_tag` to force a clean reset on first boot.

---

## Open Questions

- Should deviation probability and Gate/Trig probability share the same `step_prob` array (with the interpretation flipped by channel type), or be stored separately? A single array with inverted semantics keeps storage compact.
- Resolution: 0–100 (1% steps) or 0–255 (finer, ~0.4% steps)? 0–100 is more intuitive for display. 0–255 is more compact and accurate.
- Should zero-probability steps (gate/trig never fires) show as visually "empty" in the main step display, or only reveal their state in the SHIFT+Glide view?
- Encoder acceleration for probability? Fast spin through 0–100 is useful but may overshoot. Suggest: standard speed, no acceleration for probability values.
