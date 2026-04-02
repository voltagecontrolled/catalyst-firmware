# Sub-Step Mask Edit Mode — Implementation Spec

Read `PANELMAP.md` and `CHANGELOG.md` before starting. This is **Sequencer mode only**. Do not modify Controller mode behavior.

---

## Overview

A new toggled mode for editing per-step sub-step masks — controlling which ratchet/repeat sub-steps fire or are silent. Entered and exited via the existing Glide + Tap Tempo combo, changed from momentary to **toggle**.

Sub-steps are binary: fire (`x`) or silent (`o`). Tied sub-steps (`-`) are out of scope for this implementation.

---

## Entry / Exit

- **Glide + Tap Tempo** toggles sub-step edit mode on/off
- Previously this was momentary — change to toggle
- **Play/Reset** also exits sub-step edit mode and clears focus state

---

## Step Focus

- Touching any step encoder immediately **focuses** that step
- Focused step's sub-step mask displays on Page buttons:
  - Lit = sub-step fires
  - Unlit = sub-step silent
- Focus persists until another encoder is touched or mode is exited
- If no step has been focused yet this session, Page buttons display all lit

---

## Count Adjustment (300ms Window)

- Continuing to turn a just-touched encoder **within 300ms** adjusts count:
  - CW: ratchet count — existing behavior and feedback
  - CCW: repeat count — existing behavior and feedback
- After 300ms of no turning, the encoder is considered idle
- Subsequent touch re-focuses the step without changing its count

---

## Visual Behavior

| Element | State | Display |
|---|---|---|
| Focused step encoder | Idle in mode | Blinking yellow |
| Focused step encoder | Turning CW within 300ms | Ratchet feedback (red, existing) |
| Focused step encoder | Turning CCW within 300ms | Repeat feedback (blue, existing) |
| Unfocused step encoders | Any | Normal state (green/red/blue) |
| Page buttons | In mode, step focused | Sub-step mask for focused step |
| Page buttons | In mode, no step focused | All lit |
| Page buttons | Count changing | Update in real time as count expands/contracts |

---

## Page Button Behavior

- Each Page button corresponds to one sub-step:
  - Button 1 = sub-step 0, Button 2 = sub-step 1, ... Button 8 = sub-step 7
- Press toggles that sub-step on or off
- **Sub-step 0 (Button 1) is always forced on** — cannot be toggled off
- Only Page buttons up to the current ratchet/repeat count are active
- Remaining buttons (beyond current count) are unlit and non-interactive
- Page button behavior outside of sub-step edit mode is unchanged

---

## Mask Storage

- Per-step, stored as a `uint8_t` bitmask (8 bits = 8 sub-steps)
- Bit 0 always set (sub-step 0 always fires)
- Default value: `0xFF` (all sub-steps fire) — preserves existing behavior on unedited steps
- Mask is ignored on steps with ratchet/repeat count of 1 (only one sub-step, always fires)

---

## Out of Scope

- Controller mode — no changes
- Tied sub-steps (`-`) — x and o only
- Mask application to repeat steps — tracked separately as v1.4.3

---

## Pre-Implementation Checklist

Before writing any code, locate and summarize:

1. Where the existing Glide + Tap Tempo momentary behavior is implemented
2. Where ratchet/repeat count display feedback is handled
3. Where Page button handling lives in Sequencer mode
4. Whether the `Step` struct has room for a `uint8_t` mask field, or propose where to pack it
5. Whether `sizeof(Step)` would change — flag if a `SettingsVersionTag` bump is required
6. Any other state that needs to be added to track focus and the 300ms window

**Summarize findings before writing any code.**
