# TODO

Items are grouped by target version where known, then backlog.

---

## v1.4.6

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

### Phase Scrub lock persistence

**Area:** `src/shared.hh` `Shared::Data`, `src/ui/seq_common.hh`, `src/f401-drivers/saved_settings.hh`

Phase Scrub lock state is lost on reboot. Add `bool phase_locked` and `uint16_t locked_raw` (4 bytes padded) to `Shared::Data` so the lock state and locked slider position survive power cycles.

On boot with lock restored, `locked_phase` is recomputed from `locked_raw + current CV` (same formula used when locking live). The slider automatically enters pickup mode since its physical position will have drifted. Save `Shared::Data` (via the existing `do_save_shared` flag) whenever lock state toggles.

Requires a `CurrentSharedSettingsVersionTag` bump in `saved_settings.hh`, which triggers the existing migration path -- new fields fill with defaults (unlocked), no preset loss.

---

## Backlog

### Per-track reset settings (SHIFT+PLAY mode)

**Area:** `src/ui/seq.hh`, `src/sequencer.hh`, `src/sequencer_settings.hh` (or `src/sequencer_player.hh`)

SHIFT+PLAY currently fires an immediate reset and exits. Repurposing it as a **persistent toggle mode** would expose 8 unused step encoders and 8 unused page buttons as a natural per-track reset configuration surface -- without adding a new entry combo.

**Proposed UX:**
- First SHIFT+PLAY enters the mode (no immediate reset); second SHIFT+PLAY exits. Play/Reset alone still plays/pauses as normal while inside the mode.
- **Page buttons (1 per track):** toggle whether that track auto-resets when the sequence is paused. Lit = resets on pause; unlit = holds position. Allows mixing: e.g. tracks 1-4 restart clean, tracks 5-8 continue from where they left off.
- **Step encoders (1 per track):** configure the reset source/mode for that track (exact values TBD, e.g. master reset, follow track N loop point, never). This maps to the `reset_mode` / `reset_source` fields from the existing "Per-track reset behavior" backlog item below -- these two items should be implemented together.

**Storage:** `reset_on_pause` bool + `reset_mode` enum per channel in `Settings::Channel`. Requires `current_tag` bump (coordinate with conditional auto-reset TODO in v1.4.6).

**Note:** This resolves the ergonomic pain of needing two button presses (pause + manual reset) every time the sequence is stopped.

---

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

### Rename Follow Assign to Advanced Track Settings

**Area:** `src/ui/seq_follow_assign.hh`, `src/ui/seq_settings.hh`, `src/ui/seq.hh`, `wiki/NEW_FEATURES.md`

The Follow Assign menu currently uses 6 of 9 available FINE sub-mode colors for CV tracks (blue = CV add follow, lavender = CV replace, orange/yellow/salmon = gate clock follow ratchets/repeats/both, teal = gate clock step-only). Gate tracks have no advanced settings menu yet. Both channel types have pending features that need per-track configuration pages (gate envelope, step preview, reset behavior).

Rename and expand to a general Advanced Track Settings menu. Entry combo and persistent-mode behavior unchanged. FINE still cycles sub-modes; with 9 colors available there is room to add pages for both CV and gate tracks without navigation changes.

Planned page layout (subject to revision as features are added):
- CV tracks: blue/lavender = CV follow (existing), orange/yellow/salmon/teal = gate clock follow (existing), remaining colors for new settings
- Gate tracks: blue/orange/yellow/salmon = envelope assign (per `planned/GATE_ENVELOPE.md`), remaining colors for new settings

---

### CV step encoder preview

**Area:** `src/app.hh` `Sequencer::App::Update()`, `src/ui/seq_morph.hh` or Advanced Track Settings

While the sequencer is stopped, turning a step encoder on a CV track outputs the voltage of the step being edited to the DAC in real time, so pitches can be auditioned without running the sequencer.

**Mechanism:** UI layer sets a per-channel "preview override" value + flag when an encoder is turned while stopped. `App::Update()` checks the flag and substitutes the override value for that channel's normal `Cv()` output for that tick. Flag clears after one tick (or after encoder activity stops).

**Toggle:** on/off per track, stored in `Settings::Channel`. Surfaced as a page in Advanced Track Settings.

---

### Per-track reset behavior

**Area:** `src/sequencer_player.hh`, `src/sequencer_settings.hh`, Advanced Track Settings

Customize how each track responds to Play/Reset. Useful for generative patches where you want one primary track to anchor the pattern while others continue freely.

**Proposed modes (per track):**
- `always` -- resets on Play/Reset (current behavior for all tracks)
- `never` -- Play/Reset has no effect; track runs continuously
- `follow_chan_N` -- resets when track N loops back to step 1, not on the button press itself

The `follow_chan_N` variant is the most powerful: one track's natural loop point drives reset timing for others, allowing phasing relationships to develop and re-sync organically.

Stored as a `reset_mode` field + optional `reset_source` channel index in `Settings::Channel`. Requires `current_tag` bump (coordinate with conditional auto-reset TODO to avoid a wipe).
