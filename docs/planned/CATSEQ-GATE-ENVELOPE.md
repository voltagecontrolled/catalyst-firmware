# Gate Envelope -- Implementation Spec

Read `PANELMAP.md` and `CHANGELOG.md` before starting. This is **Sequencer mode only**, **gate channels only**.

---

## Overview

A west coast-style A/D function generator per gate channel. When any envelope parameter is configured, the gate output becomes a shaped voltage envelope rather than a binary on/off signal. Attack and decay times and peak amplitude are independently sourceable. Curve applies to both attack and decay phases simultaneously.

---

## Parameters

| Parameter | Sub-mode color | Description |
|-----------|---------------|-------------|
| Amplitude | Blue | Peak output voltage (0V to range max) |
| Attack | Orange | Time from 0V to peak (0 to max_attack) |
| Decay | Yellow | Time from peak to 0V (0 to max_decay) |
| Curve | Salmon | Shape applied to both A and D phases |

**Curve:** a single value from log through linear to exponential. Applied identically to attack and decay. Log = fast initial change, gradual tail (percussive pluck character). Exponential = slow initial change, fast end (reversed, swell character). Linear = straight ramp.

**Time ranges:** TBD during implementation based on what feels useful for percussion/modulation. Suggested: attack 0--500ms, decay 0--2000ms. Define as constants.

---

## Source Assignment

Each parameter has an independent source. Three source types:

**Unassigned (default):** parameter uses a fixed default value:
- Amplitude: 10V (full range)
- Attack: 0ms (instant, effectively a trigger)
- Decay: 50ms (short percussive tail)
- Curve: linear

**CV track source:** the source CV track's current output voltage (0--10V range, clamped) linearly maps to the parameter's range. Source is assigned via page button radio selector (same interaction as CV track Follow Assign).

**Gate width source:** the step's existing `gate_width` field (0--1) maps linearly to the parameter's range. When assigned, all page buttons light simultaneously as a visual indicator. Gate width becomes the parameter source rather than controlling gate duration (gate duration is fixed at the trigger pulse length when in envelope mode).

---

## UI -- Envelope Assign Mode

### Entry

Same entry path as Follow Assign for CV tracks, but targeting a gate channel:

1. **SHIFT + CHAN** -- enter Channel settings, select a gate channel
2. **Tap GLIDE** -- enters Envelope Assign mode (persistent, same as Follow Assign)
3. **FINE** -- cycles through the four parameter pages (Blue / Orange / Yellow / Salmon)
4. **Tap GLIDE** or **Play/Reset** -- exits

### Page Button Behavior

**Single press:** assigns that CV track as the source for the active parameter. Radio select -- pressing the lit button unlinks (returns to default). Self-selection is ignored.

**Long press (~500ms) on any button:** assigns gate width as the source. All page buttons light to indicate gate width mode. Long press again clears back to unassigned default.

**Display:** selected channel encoder blinks in the active parameter's color. Other encoder LEDs show channel type colors (same as Chan. hold).

### Envelope Active Indicator

When any parameter has a non-default source assigned, the channel has an active envelope configuration. No special persistent visual indicator during normal playback -- user must enter Envelope Assign mode to inspect.

---

## Envelope Behavior

### Trigger and Retrigger

- Gate fires (any ratchet sub-step or repeat tick) -- envelope immediately attacks from current level (retrigger, not from zero). Allows natural ratchet stacking.
- Gate width no longer controls gate duration when in envelope mode. Gate output is driven entirely by the envelope level.

### Phase Progression

```
Attack phase: level rises from current_level to amplitude over attack_time
              shape applied: log / linear / exponential per curve setting
Decay phase:  level falls from amplitude to 0V over decay_time
              same curve shape as attack
Idle:         level = 0V
```

Attack and decay fire sequentially on every trigger. There is no sustain or hold. This is a one-shot A/D generator.

### Standard Gate (no envelope configured)

If all parameters are at default (no sources assigned), the channel behaves identically to current gate behavior -- binary on/off at the channel's range max. No performance difference from existing firmware.

---

## Ratchet and Repeat Interaction

- **Ratchets:** each ratchet sub-step retriggering the envelope from current level produces natural stacking at ratchet speed. With a short decay, fast ratchets produce a machine-gun effect. With a slow decay, ratchets blend.
- **Repeats:** same -- each repeat tick retriggers.
- **Gate width as source:** all ratchet sub-steps within a step share the same gate width value (per-step, not per-sub-step). Per-sub-step variation requires a CV track source.

---

## Implementation Notes

### Envelope State (per gate channel)

New state in `Sequencer::Interface`:
```cpp
struct EnvelopeState {
    float level = 0.f;        // current output level 0..1
    bool in_attack = false;   // false = decay or idle
};
std::array<EnvelopeState, Model::NumChans> envelope_state{};
```

Reset in `Reset()`.

### Output Path

`App::Gate()` currently returns a binary `gate_high` or `gate_off`. With envelope:
- Detect trigger (gate fires this sub-step)
- If trigger: set `in_attack = true`; advance attack phase
- Else: advance decay phase
- Return `Channel::Output::ScaleCv(level * amplitude, range)` instead of binary gate values

The DAC path is already capable of variable voltage on gate channels -- `Model::Output::type` is the same type for both CV and gate channels.

### Curve Implementation

A power curve applied to a 0--1 linear ramp `t`:

```
curve_exponent > 1: t^curve_exponent (exponential -- slow start, fast end)
curve_exponent = 1: t (linear)
curve_exponent < 1: t^curve_exponent (logarithmic -- fast start, slow end)
```

Attack: `level = t^(1/curve)` where t goes 0→1
Decay:  `level = (1-t)^curve` where t goes 0→1

CV/gate-width source maps to curve_exponent range TBD (e.g. 0.2--5.0, with 0V/0 = 1.0 linear as center).

### Parameter Source Lookup (per tick)

For each active gate channel, each tick:
1. Determine amplitude, attack_time, decay_time, curve from their respective sources
2. If source = CV track: read `last_cv_stepval[source - 1]`, convert to parameter range
3. If source = gate width: read current step's `gate_width`, map to parameter range
4. If source = default: use fixed default constants

### Storage

Four new fields per channel in `Settings::Channel`:
```cpp
uint8_t env_amplitude_source = 0;  // 0=default, 1..8=CV track, 9=gate width
uint8_t env_attack_source = 0;
uint8_t env_decay_source = 0;
uint8_t env_curve_source = 0;
```

`current_tag` bump required.

---

## Pre-Implementation Checklist

Before writing code:

1. Confirm all 8 gate channels output variable voltage through the same DAC path as CV channels (no separate digital gate-only output that bypasses the DAC).
2. Determine time constant representation: seconds stored as float, or ticks-per-step stored as uint16_t? Float is simpler. Confirm no performance issue at 3kHz.
3. Decide exact time ranges and curve exponent range based on what feels right with target modules (Nord Drum 1, MSW SY0.5, Syncussion).
4. Confirm `last_cv_stepval[]` is accessible from the gate envelope computation path (currently in `App`, not `Interface`).
5. Determine whether envelope state lives in `Interface` (alongside `repeat_ticks_remaining`) or in `App` (alongside `Gate()`). `App` is preferable since that is where gate output is computed and where retrigger detection naturally lives.

**Summarize findings before writing any code.**
