# Catalyst VoltSeq — Iterative Probability

Per-step cycle filter for Gate and Trigger channels. Determines which repetition of the pattern a step fires on. Fully deterministic — same result every pass through the cycle — as opposed to the existing `random_amount` which is probabilistic.

---

## Concept

Each Gate or Trigger step can be assigned a cycle filter: fire only on repeat X out of every N passes through the pattern. With invert enabled, the step fires on all passes *except* X.

```
cycle_length N  — how many pattern repetitions define one cycle (1–8)
fire_on      X  — which repetition within the cycle fires this step (1–N)
invert          — if set, fires on all repetitions except X
```

The step fires when:

```
fires = ((pattern_repeat_count % N) + 1 == X) XOR invert
```

Default: N=1, X=1, invert=off → step fires every time (no filtering).

---

## Entry / Exit

**Entry:** Hold Glide + a page button (the target step) on a Gate or Trigger channel for ~1–2 seconds. Per-step iterative probability editor opens for that step.

This is a long-press variant of the existing Glide + page-button gesture. The existing short-press path (Gate ratchet editing while waiting for the long-press timer) is unchanged — the editor only opens on timer fire, not on encoder turn.

**Exit:** Glide press (same as Glide Step Editor) or Play/Reset.

---

## Controls

| Encoder | Parameter | Range | Default | LED |
|---------|-----------|-------|---------|-----|
| 1 | Cycle length (N) | 1–8 | 1 (off) | Brightness = N/8 |
| 2 | Fire on repeat (X) | 1–N | 1 | Position indicator (enc 0=X=1, enc 7=X=8) |
| 3 | Invert | off / on | off | Red = inverted, off = normal |

Encoder 2 should clamp to N if N is reduced below the current X value.

---

## Examples

| N | X | Invert | Effect |
|---|---|--------|--------|
| 1 | 1 | off | Fires every pass (default — no filter) |
| 2 | 1 | off | Fires on passes 1, 3, 5… (every other, starting first) |
| 2 | 2 | off | Fires on passes 2, 4, 6… (every other, starting second) |
| 4 | 4 | off | Fires only on every 4th pass |
| 4 | 4 | on  | Fires on passes 1, 2, 3 — drops on the 4th |
| 8 | 1 | off | Fires once every 8 pattern repetitions |

At 16th note resolution, an 8-step pattern = half a bar. N=2 creates a 1-bar cycle; N=4 = 2-bar cycle; N=8 = 4-bar cycle.

---

## Pattern Repeat Counter

A per-channel `repeat_count` increments every time the channel wraps from its last step back to step 0. It resets to 0 on any channel reset (manual Reset, reset leader, master reset steps). The modulo operation is applied against this counter.

---

## Storage

Gate and Trigger steps currently use 16 bits (`StepValue`). Gate steps use high byte (gate length) + low byte (ratchet count). Trigger steps use only the low byte (signed ratchet/repeat value).

Iterative probability adds 3 fields per step: N (3 bits, 0=off/N=1 through 8), X (3 bits), invert (1 bit) = 7 bits total.

**Options:**

- **Expand StepValue to 32 bits** — clean but requires `current_tag` bump and potentially changes `sizeof(VoltSeq::Data)` significantly.
- **Pack into unused bits** — Gate steps have the full high byte used (gate length) and low byte used (ratchet). Trigger steps only use the low byte (signed int8). Could pack iterative probability into the high byte of Trigger steps (currently unused) or add a parallel array. Needs careful layout review before committing.

**Recommendation:** Design storage before implementing. A per-channel `repeat_flags[64]` parallel array (separate from `StepValue`) costs 64 bytes × 8 channels = 512 bytes and avoids touching StepValue encoding. With a compact bitfield struct (7 bits per step = 56 bytes per channel), total overhead is modest. Requires `current_tag` bump.

---

## Open Questions

- Should N=1, X=1 (default/off) be the LED-off state so the display doesn't clutter normal editing?
- How does the repeat counter interact with pattern length changes mid-sequence?
- Should the editor show which pass of the current cycle we're on (live feedback while playing)?
- Entry combo conflict: the existing Glide + page-button long-press already enters the Glide Step Editor for CV and Gate channels. Iterative probability needs to either share that entry path (and differentiate by channel type or a modifier) or use a distinct gesture.
