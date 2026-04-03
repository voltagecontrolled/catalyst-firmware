# Linked Tracks -- Implementation Notes

Original spec written before implementation. Final implementation differs from spec in several areas noted below.

---

## What Was Built (v1.4.4)

### CV Transpose Follow

A CV track can designate another CV track as a transpose source. The source track's current output (relative to `Channel::Cv::zero`) is added to the target track's pitch each update cycle.

**Storage:** `uint8_t transpose_source` in `Settings::Channel` (0 = unlinked, 1..NumChans = source, 1-based).

**Engine:** `src/app.hh` `Sequencer::App::Cv()`. A `last_cv_stepval[]` array caches the previous tick's pre-scaled stepval per channel. The offset is computed as `int32_t(last_cv_stepval[src]) - int32_t(Channel::Cv::zero)`, clamped, and added to stepval. One-tick delay (~0.33ms, imperceptible).

---

### Gate Track Clock Follow

A CV track can designate a gate track as its clock source, replacing the master clock. Four follow modes control which gate events advance the CV track.

**Storage:** `uint8_t clock_source` (0 = unlinked, 1..NumChans = gate track, 1-based) and `uint8_t clock_follow_mode` (0 = ratchets only, 1 = repeats only, 2 = both) in `Settings::Channel`.

**Engine:** `src/sequencer.hh` `Interface::Update()`. The Gate Track Follow block runs every 3kHz iteration (not just on clock ticks) to catch ratchet sub-step crossings in real time.

Per-gate-channel signals computed each iteration:
- `gate_step_adv` -- 1 when the gate step pointer advances (first tick of any step). Always applied to linked CV tracks regardless of mode.
- `gate_ratchet_adv` -- ratchet sub-step crossing count, detected via `floor(ratchet_count * step_phase)` changing. Applied when `clock_follow_mode != 1`.
- `gate_repeat_adv` -- 1 when a repeat tick fires (set in clock-tick recording block, consumed next iteration). Applied when `clock_follow_mode >= 1`.

`gate_substep_idx[]` tracks the last seen ratchet sub-step index per gate channel, reset on each step boundary.
`gate_repeat_fired[]` is a one-shot flag: set in the post-player recording block on repeat ticks, consumed and cleared by the Gate Track Follow block.

`per_chan_step[]` is `uint8_t` (not bool) to carry multi-step counts. `player.Update()` loops `phaser.Step()` for the given count.

**Timing:** Ratchet sub-step advances are detected with one iteration of latency (~0.33ms). Step boundary and repeat advances also carry one iteration of latency via the recording block. All delays are imperceptible.

---

## Differences from Original Spec

- **Original spec:** gate track follow only, single mode (all firing events advance CV).
- **Final implementation:** four sub-modes (blue/orange/yellow/salmon) selectable per-track, independently controlling whether ratchets and/or repeats advance the CV track.
- **Original spec:** "only valid gate track channels are selectable" in UI.
- **Final implementation:** any non-self track is selectable; invalid combinations are silently ignored in the engine.
- **Original spec:** entry via SHIFT+CHAN+GLIDE hold.
- **Final implementation:** tap GLIDE while holding SHIFT+CHAN enters a persistent mode; tap GLIDE again to exit.
- **CV transpose follow** was described as "out of scope -- future" in the original spec. Implemented in the same release.

---

## Settings Version Tag

Adding `transpose_source`, `clock_source`, and `clock_follow_mode` to `Settings::Channel` changed `Sequencer::Data` layout. `current_tag` was incremented to `5u` to force clean reset on first boot after upgrading.
