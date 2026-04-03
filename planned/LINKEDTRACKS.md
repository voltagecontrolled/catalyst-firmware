# Linked Tracks — Implementation Spec

Read `PANELMAP.md` and `CHANGELOG.md` before starting. This is **Sequencer mode only**. Do not modify Controller mode behavior.

---

## Overview

A per-channel setting that allows a CV track to follow a gate track's firing events as its clock source, replacing the master sequencer clock. When linked, the CV track advances one step each time a firing sub-step occurs on the target gate track — including ratchet and repeat sub-steps, but excluding silenced sub-steps (mask `o`).

This enables per-step pitch variation driven by the gate track's rhythmic structure, producing generative melodic content from short CV sequences without additional sequencer complexity.

---

## Scope

- **CV track → gate track linking:** A CV track may follow any gate track. Multiple CV tracks may follow the same gate track.
- **Self-linking is disallowed.** Attempting to link a track to itself is ignored silently.
- **Linking to invalid channel types is ignored silently** — e.g. attempting to link a CV track to another CV track is out of scope for this implementation (see Future section).
- **Replaces master clock** for that CV track while linked. Not additive.

---

## Clock Behavior When Linked

- CV track advances **one step per firing sub-step** on the target gate track
- **Silenced sub-steps** (mask `o`) do **not** advance the CV track
- **Repeat ticks** count as firing events and advance the CV track
- **Muted gate track:** CV track does not advance (no firing events produced — falls out naturally, no special handling needed)
- **Pattern reset:** CV track resets with the gate track's pattern cycle

---

## UI — Entering Follow Assign Mode

1. **SHIFT + CHAN** — enter Channel Edit mode on a CV track (existing behavior)
2. **GLIDE** — enter Follow Assign mode for that CV track
3. **Page buttons** act as a radio selector:
   - Each Page button corresponds to one channel (button N = channel N)
   - Only valid gate track channels are selectable; invalid channel types are ignored silently
   - Currently linked track (if any) is lit; all others unlit
   - Tap a lit button to **unlink** (no follow source)
   - Tap an unlit valid gate track button to **link** to that track
4. **SHIFT + CHAN or GLIDE** — exit Follow Assign mode, return to Channel Edit

---

## Visual Behavior

- **In Follow Assign mode:** Page buttons show current link state (radio, one lit or none)
- **In normal operation:** No visual indicator that a CV track is linked — user must enter Follow Assign mode to inspect

---

## Storage

- Gate Track Follow is a **channel-level setting**, not per-step — do not store in `Step` struct
- Proposed: 4-bit field per channel (0 = unlinked, 1–8 = target channel index)
- Identify the appropriate existing per-channel config struct, or propose where to add this field
- Flag if any struct size change requires a settings version tag bump

---

## Future — CV Track → CV Track Linking (Out of Scope)

Flagged for future implementation. Allowing a CV track to follow another CV track's step advances would enable transpose/modulation hierarchies — essentially subsequencer behavior similar to the Five12 Vector Sequencer. Extremely powerful in combination with Gate Track Follow.

The storage model above should accommodate this with minimal changes (target channel index already supports 1–8, covering both gate and CV tracks). Disallow self-linking remains applicable.

---

## Pre-Implementation Checklist

Before writing any code, locate and summarize:

1. Where per-channel configuration is stored — is there an existing channel settings struct separate from `Step`?
2. Where master clock advancement of CV tracks is handled — this is the code path that Gate Track Follow will conditionally replace
3. Where gate track firing events are determined, including sub-step mask evaluation — this is the signal that will clock linked CV tracks
4. Where SHIFT + CHAN channel edit mode is implemented
5. Whether a settings version tag bump is required for any storage changes
6. Any timing/ordering concerns — gate track firing and CV track advancement need to be evaluated in the correct order within the same clock tick

**Summarize findings before writing any code.**
