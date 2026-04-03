# TODO

Items are grouped by target version where known, then backlog.

---

## v1.4.5

### Sub-step page navigation

**Area:** Sub-step mask edit mode (`src/ui/seq_substep_mask.hh`)

Sequences longer than 8 steps span multiple pages. There is currently no way to change pages while inside sub-step edit mode -- the only exit path is Tap Tempo or Play/Reset, which destroys the edit context.

**Proposed:** SHIFT + PAGE button changes page without exiting the mode. The focused step should follow or clear on page change; exact behavior TBD during implementation.

---

### Conditional auto-reset on firmware upgrade

**Area:** `src/sequencer.hh` `Sequencer::Data::validate()`, `src/f401-drivers/saved_settings.hh`

Currently any `current_tag` mismatch wipes all presets on first boot. This is appropriate when `sizeof(Step)` or struct layout changes, but unnecessary for firmware upgrades that only add new fields with safe defaults (e.g. envelope sources, clock follow mode). A user upgrading within 1.4.x should not lose saved patterns.

**Proposed:** Encode the major/minor version into the tag or add a secondary "layout version" field separate from the feature-additions version. If the flash data's layout version matches the current layout version, migrate forward (fill new fields with defaults) rather than wiping. Only wipe on a layout-breaking change.

May require splitting `current_tag` into two values: `layout_tag` (increment only on struct layout breaks) and `feature_tag` (increment on any additive change, used for migration).

---

## Backlog

### Gate probability iterative mode

**Area:** `src/sequencer_step.hh`, `src/sequencer.hh`, `src/ui/seq_morph.hh`

CCW encoder positions in gate probability mode replace random percentage with a deterministic cycle. CW (1--15) = percentage probability (current behavior). CCW (1--14) = iterative: fires p out of every n steps in rotation (1/2, 2/2, 1/3... up to 5/5).

Requires 1 extra bit in the probability field to represent negative (iterative) values. Candidate: repurpose gate-channel morph bits (morph is unused on gate channels, already partially repurposed for ratchet/repeat display). Player needs a per-channel cycle counter analogous to `repeat_ticks_remaining`.

---

### Phase Scrub Settings

**Area:** `src/ui/seq_common.hh`, `src/ui/seq.hh`, global settings storage

Extends Phase Scrub Lock with two persistent options. Full spec: `planned/PHASESCRUB_SETTINGS.md`.

- **Quantized mode (global):** Phase Scrub offset only takes effect at step boundaries.
- **Per-track ignore (per-track):** Individual tracks excluded from Phase Scrub entirely.

**Entry:** COPY + GLIDE held 3 seconds.

---

### Sub-step tie/slew in mask edit mode

**Area:** `src/sequencer_step.hh`, `src/ui/seq_substep_mask.hh`

Extends mask edit mode with a third per-sub-step state: tied. A tied sub-step holds the gate open from the previous sub-step rather than retriggering. Requires expanding the sub-step mask from 1 bit per sub-step (`uint8_t`) to 2 bits per sub-step (`uint16_t`). 18 bits available in current `Step` struct; fits with a `current_tag` bump.

---

### Gate Envelope (A/D function generator)

**Area:** `src/app.hh` `Sequencer::App::Gate()`, `src/sequencer_settings.hh`, `src/ui/seq_envelope_assign.hh` (new)

A west coast-style A/D function generator per gate channel. When any envelope parameter is configured, the gate output becomes a shaped voltage envelope. Full spec: `planned/GATE_ENVELOPE.md`.

Parameters (Follow Assign sub-mode colors):
- Blue = peak amplitude
- Orange = attack time
- Yellow = decay time
- Salmon = curve (shared A+D, log through linear to exponential)

Each parameter independently sourceable: unassigned (fixed default), CV track (page button radio), or gate width (long-press, all buttons lit). Pre-implementation checklist in spec verified -- all 8 gate channels are DAC-driven, envelope state belongs in `App`, `last_cv_stepval[]` is accessible in same class.

---

### Follow Assign: switching gate clock modes quirk

**Area:** `src/ui/seq_follow_assign.hh`

When a gate track is assigned as clock source under one mode (e.g. Orange) and the user presses FINE to switch to a different mode (e.g. Yellow), the page button for the previously assigned track remains lit. The stored assignment is the Orange-mode one -- it is not automatically re-applied under the new mode.

**Fix:** when FINE cycles to a new clock-follow mode and a clock source is already assigned, carry the source assignment forward to the new mode (call `SetClockFollowMode` and re-apply) and clear it from the previous mode. Documented as a known quirk in `wiki/NEW_FEATURES.md`.
