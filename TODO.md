# TODO

Items are grouped by target version where known, then backlog.

---

## v1.5.0 — VoltSeq (branch: voltseq)

Full spec: `docs/planned/VOLTSEQ.md`

Implementation phases (see spec for detail):

1. **Foundation** — compile-time build flag, `VoltSeq::Data` struct, tag bump
2. **Clock engine** — external clock edge detection, internal BPM timer, inter-tick interval measurement, per-channel division counters, Tap Tempo
3. **Playback** — per-channel playheads + shadow playheads, direction state machines, output calculation (CV/Gate/Trigger), step-lock + arpeggiation, SHIFT+PLAY reset
4. **Step editing UI** — hold page button, page navigation, channel arm state machine, x0x step editing, slider CV recording
5. **Channel Edit + Global Settings** — enc 4 mode selector, phase rotate, all per-channel settings, global settings
6. **Glide / Ratchet editors** — GLIDE modifier, long-press step editors for glide and ratchets
7. **Slider performance page** — port orbit engine, beat repeat, lock + pickup
8. **Infrastructure** — 3-button mode switch (replace `modeswitcher` in both modes), save behavior, `release.yml` three-build passes

---

## v1.4.7

### CatSeq step clock resolution — needs verification

**Observed:** VoltSeq (alpha21) now advances steps at 16th-note rate (bpm_in_ticks / 4 per step via sub-clock). CatSeq advances steps at quarter-note rate (`seqclock.Update()` fires every `bpm_in_ticks` ticks, one step per fire). This is a 4× difference in step rate at the same BPM setting.

**Status:** Needs hardware verification with a reliable MIDI clock source before changing CatSeq. If CatSeq should also be 16th-note resolution, the fix is similar to VoltSeq: add a 16th-note sub-counter in `Sequencer::Interface::Update()` (`src/sequencer.hh`) that drives `per_chan_step` 4× per beat while keeping `seqclock` at quarter-note rate for phase tracking, beat repeat, and ratchet sub-step detection.

**Caution:** CatSeq is at v1.4.6 (hardware verified). The gate track follow, ratchet sub-step detection, and beat repeat all depend on `seqclock` phase — those must remain on the quarter-note clock. Only `per_chan_step` advancement needs to change.

---

### Conditional auto-reset on firmware upgrade


**Area:** `src/sequencer.hh` `Sequencer::Data::validate()`, `src/f401-drivers/saved_settings.hh`

Currently any `current_tag` mismatch wipes all presets on first boot. This is appropriate when `sizeof(Step)` or struct layout changes, but unnecessary for firmware upgrades that only add new fields with safe defaults (e.g. envelope sources, clock follow mode). A user upgrading within 1.4.x should not lose saved patterns.

**Proposed:** Encode the major/minor version into the tag or add a secondary "layout version" field separate from the feature-additions version. If the flash data's layout version matches the current layout version, migrate forward (fill new fields with defaults) rather than wiping. Only wipe on a layout-breaking change.

May require splitting `current_tag` into two values: `layout_tag` (increment only on struct layout breaks) and `feature_tag` (increment on any additive change, used for migration).

---

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

### Step Arpeggio

**Area:** `src/sequencer_step.hh`, `src/app.hh` `Cv()`, `src/sequencer_player.hh`, `src/ui/seq_prob.hh`

Per-step arp on CV tracks. 6 chord types (major 5th, minor 5th, major 7th, minor 7th, octave, minor pentatonic); each has ascending/descending direction controlled independently per step. Mutually exclusive with probability.

**Entry:** CCW in SHIFT+GLIDE (quick edit); or SHIFT+GLIDE+TAP TEMPO enters a persistent hands-free editor where encoders set type and page buttons toggle up/down direction. SHIFT+PAGE navigates pages within the persistent mode.

On ratcheted/repeated steps the arp walks the chord across sub-steps (strum/roll). On plain steps it advances one note per clock pass.

Full spec: `docs/planned/STEP_ARP.md`

Requires `current_tag` bump (coordinate with other v1.4.6 tag bumps). Uses bits 0–3 of `sub_step_mask` on CV channels (otherwise unused).

---

### Gate probability iterative mode

**Area:** `src/sequencer_step.hh`, `src/sequencer.hh`, `src/ui/seq_morph.hh`

CCW encoder positions in gate probability mode replace random percentage with a deterministic cycle. CW (1--15) = percentage probability (current behavior). CCW (1--14) = iterative: fires p out of every n steps in rotation (1/2, 2/2, 1/3... up to 5/5).

Requires 1 extra bit in the probability field to represent negative (iterative) values. Candidate: repurpose gate-channel morph bits (morph is unused on gate channels, already partially repurposed for ratchet/repeat display). Player needs a per-channel cycle counter analogous to `repeat_ticks_remaining`.

---

### Sub-step tie/slew in mask edit mode

**Area:** `src/sequencer_step.hh`, `src/ui/seq_substep_mask.hh`

Extends mask edit mode with a third per-sub-step state: tied. A tied sub-step holds the gate open from the previous sub-step rather than retriggering. Requires expanding the sub-step mask from 1 bit per sub-step (`uint8_t`) to 2 bits per sub-step (`uint16_t`). 18 bits available in current `Step` struct; fits with a `current_tag` bump.

---

### Gate Envelope (A/D function generator)

**Area:** `src/app.hh` `Sequencer::App::Gate()`, `src/sequencer_settings.hh`, `src/ui/seq_envelope_assign.hh` (new)

A west coast-style A/D function generator per gate channel. When any envelope parameter is configured, the gate output becomes a shaped voltage envelope. Full spec: `docs/planned/GATE_ENVELOPE.md`.

Parameters (Follow Assign sub-mode colors):
- Blue = peak amplitude
- Orange = attack time
- Yellow = decay time
- Salmon = curve (shared A+D, log through linear to exponential)

Each parameter independently sourceable: unassigned (fixed default), CV track (page button radio), or gate width (long-press, all buttons lit). Pre-implementation checklist in spec verified -- all 8 gate channels are DAC-driven, envelope state belongs in `App`, `last_cv_stepval[]` is accessible in same class.

---

### Rename Follow Assign to Advanced Track Settings

**Area:** `src/ui/seq_follow_assign.hh`, `src/ui/seq_settings.hh`, `src/ui/seq.hh`, `docs/wiki/NEW_FEATURES.md`

The Follow Assign menu currently uses 6 of 9 available FINE sub-mode colors for CV tracks (blue = CV add follow, lavender = CV replace, orange/yellow/salmon = gate clock follow ratchets/repeats/both, teal = gate clock step-only). Gate tracks have no advanced settings menu yet. Both channel types have pending features that need per-track configuration pages (gate envelope, step preview, reset behavior).

Rename and expand to a general Advanced Track Settings menu. Entry combo and persistent-mode behavior unchanged. FINE still cycles sub-modes; with 9 colors available there is room to add pages for both CV and gate tracks without navigation changes.

Planned page layout (subject to revision as features are added):
- CV tracks: blue/lavender = CV follow (existing), orange/yellow/salmon/teal = gate clock follow (existing), remaining colors for new settings
- Gate tracks: blue/orange/yellow/salmon = envelope assign (per `docs/planned/GATE_ENVELOPE.md`), remaining colors for new settings

---

### CV step encoder preview

**Area:** `src/app.hh` `Sequencer::App::Update()`, `src/ui/seq_morph.hh` or Advanced Track Settings

While the sequencer is stopped, turning a step encoder on a CV track outputs the voltage of the step being edited to the DAC in real time, so pitches can be auditioned without running the sequencer.

**Mechanism:** UI layer sets a per-channel "preview override" value + flag when an encoder is turned while stopped. `App::Update()` checks the flag and substitutes the override value for that channel's normal `Cv()` output for that tick. Flag clears after one tick (or after encoder activity stops).

**Toggle:** on/off per track, stored in `Settings::Channel`. Surfaced as a page in Advanced Track Settings.

---

### Per-step clock divisions / multiplications (gate tracks)

**Area:** `src/sequencer_player.hh`, `src/sequencer_step.hh`, `src/ui/seq.hh`

**Entry combo reserved: SHIFT + TAP TEMPO** (gate tracks only)

Per-step clock div/mult allows individual gate steps to fire at a fraction or multiple of
the global clock rate. Useful for polyrhythmic patterns without requiring separate clock
sources.

Exact UX and storage TBD. Implement after Step Arp (v1.4.6) to avoid `current_tag`
conflicts. Gate tracks only — CV tracks use SHIFT+TAP TEMPO for a different purpose or
leave it unassigned.

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
