# Catalyst VoltSeq — Beat Repeat (Performance Page)

Beat repeat mode is partially implemented but non-functional. The UI zone detection, LED display, and debounce machinery are all correct. The engine advance loop does nothing useful.

---

## Current State (Broken)

In beat repeat mode (`slider_perf_mode` = 2 or 3), `window_ref` is always 1, so:

```
orbit_pos_ = (orbit_pos_ + 1) % 1 = 0  (always)
orbit_step_[ch] = center_step          (always)
```

The orbit step never changes regardless of which zone the slider is in. All zones are identical: they hold the output at a fixed step. Blue (8-zone) and cyan (4-zone) behave identically.

The `div_num_8 / div_denom_8` table with proper triplet values (2/3, 1/3, 1/6 etc.) is correct and can be reused.

---

## What It Should Do

Beat repeat subdivides the sequence playback rate for channels that have orbit follow enabled. Moving the slider to a zone selects a subdivision ratio; the channel loops a window of steps at that rate while in the zone.

Zone tables (already implemented in UI):

**Blue — 8 zones:**
| Zone | Subdivision | Notes |
|------|------------|-------|
| 0 | 1/2 | Half time |
| 1 | 1/4 | Quarter |
| 2 | 1/4T | Quarter triplet |
| 3 | 1/8 | Eighth |
| 4 | 1/8T | Eighth triplet |
| 5 | 1/16 | Sixteenth |
| 6 | 1/16T | Sixteenth triplet |
| 7 | 1/32 | Thirty-second |

**Cyan — 4 zones:**
| Zone | Subdivision |
|------|------------|
| 0 | 1/2 |
| 1 | 1/4 |
| 2 | 1/8 |
| 3 | 1/16 |

Slider at minimum (< 32 ADC units) = off; channels revert to normal playback.

---

## Design Options

### Option A — Clock Multiplier

When beat repeat is active, channels following orbit have their step advance rate overridden by `1 / safe_period` ticks per step. `OnChannelFired` would need to be callable at arbitrary rates independent of clock divider, or a separate timer per following-channel that fires at `safe_period`.

This produces actual rhythmic gate/trigger subdivision — notes fire at the subdivided rate — which is musically useful for gate and trigger channels.

### Option B — Metered Window Loop (Preferred)

Keep the granular orbit window concept but advance the window at the beat-repeat subdivision rate rather than every master clock tick:

1. Set `window_ref` to a number of steps driven by the zone index (not hardcoded to 1).
2. `orbit_should_adv` fires at `safe_period` ticks (derived from the zone's subdivision ratio × master BPM).
3. Following channels cycle through that window at the metered rate.

This maps naturally onto Catalyst VoltSeq's step-address model and reuses the existing orbit advance infrastructure. The window loops within the region centered on `orbit_center` (slider position), so moving the slider while in a zone shifts which region loops.

**Implementation sketch:**

```cpp
// In UpdateOrbit() when beat repeat is committed:
const float ratio = div_num_8[zone] / div_denom_8[zone]; // e.g. 1/8 → 0.125
const uint32_t safe_period = static_cast<uint32_t>(master_bpm_ticks * ratio);
const uint8_t window_steps = max(1, round(ratio * channel_length));
window_ref = window_steps;
// orbit_should_adv fires every safe_period ticks (replace bool with timer)
```

---

## Open Questions

- How does `orbit_center` snap to a step on beat repeat entry? (Currently `beat_repeat_snap_pending` handles this — verify it still applies.)
- Does the window loop from `center - window/2` to `center + window/2`, or from `center` forward?
- When the orbit zone changes (slider moves to new zone mid-pattern), does the loop restart immediately or wait for the next window boundary?
- How does beat repeat interact with Reverse / Ping-Pong direction modes on a following channel?
