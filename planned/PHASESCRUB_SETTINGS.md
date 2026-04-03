# Phase Scrub Settings — Implementation Spec

Read `PANELMAP.md` and `CHANGELOG.md` before starting. This is **Sequencer mode only**. Do not modify Controller mode behavior.

---

## Overview

Extends the existing Phase Scrub Lock feature with two additional configuration options:

1. **Quantized mode** — Phase Scrub offset only snaps in at step boundaries rather than applying continuously
2. **Per-track ignore** — individual tracks can be excluded from Phase Scrub entirely, behaving as though the slider does not exist

These settings are orthogonal to the existing lock/unlock toggle and do not affect its performance behavior.

---

## Behavioral Model

Two independent axes:

### Lock State (existing, unchanged)
- **Unlocked** — slider moves freely, offset applies per quantization setting
- **Locked** — slider locked at current position, pickup behavior on unlock, encoder 8 blinks red

Fine + Glide toggles between these two states. This combo and its behavior are **unchanged**.

### Quantization (new, persistent global setting)
- **Off** — offset applies continuously as slider moves (current behavior)
- **On** — offset only takes effect at the next step boundary after the slider moves

These two axes are fully orthogonal:
- **Unlocked + Unquantized** — current default behavior
- **Unlocked + Quantized** — slider moves freely but offset snaps in at step boundaries
- **Locked + Unquantized** — existing lock behavior
- **Locked + Quantized** — locked, but when unlocked the new position waits for the next step boundary before taking effect

### Per-Track Ignore (new, persistent per-track setting)
- Tracks with ignore enabled are completely unaffected by Phase Scrub
- Behaves as though the Phase Scrub slider does not exist for that track
- Independent of lock state and quantization setting
- 1 bit per track, 8 tracks = 8 bits total

---

## Settings Page

### Entry
- **COPY + GLIDE held 3 seconds** — enters Phase Scrub Settings mode
- Long hold distinguishes from accidental triggers

### Exit
- **COPY + GLIDE** (any duration) — exits settings page
- **Play/Reset** — exits settings page

### Controls in Settings Mode

**Encoder 1 — Quantization toggle:**
- Red = Quantized Off (default, current behavior)
- Orange = Quantized On
- Turn or press to toggle

**Page buttons — Per-track ignore (radio-style per track):**
- Button N = track N
- Lit = Phase Scrub active for that track (default)
- Unlit = Phase Scrub ignored for that track
- Press to toggle individual tracks

### Visual While in Settings Mode
- Encoder 1 shows quantization state (red/orange)
- Page buttons show per-track ignore state
- All other encoders maintain normal display

---

## Storage

- **Quantization setting:** 1 bit, global
- **Per-track ignore:** 1 bit × 8 tracks = 8 bits = 1 byte
- Both are channel/global-level settings — do not store in `Step` struct
- Store alongside existing Phase Scrub lock state in global settings struct
- Flag if any struct size change requires a settings version tag bump

---

## Interaction with Existing Phase Scrub Lock

- Fine + Glide quick toggle is **completely unchanged**
- Encoder 8 blink behavior on lock/unlock is **completely unchanged**
- Quantization and per-track ignore are additive — they modify how the unlocked state behaves, they do not replace or interfere with lock behavior

---

## Pre-Implementation Checklist

Before writing any code, locate and summarize:

1. Where the existing Phase Scrub lock state is stored and managed
2. Where Phase Scrub offset is applied to track playback — this is where quantization logic will be inserted
3. Where per-track playback is computed — this is where per-track ignore will be applied
4. Where COPY + GLIDE is currently handled, if at all
5. Where global/settings state is stored — confirm appropriate place for quantization bit and per-track ignore byte
6. Whether any struct changes require a settings version tag bump

**Summarize findings before writing any code.**
