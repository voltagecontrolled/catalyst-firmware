# TODO

---

# Catalyst Sequencer

## v1.6.0

### CatSeq step clock resolution — needs verification

**Observed:** VoltSeq advances steps at 16th-note rate (bpm_in_ticks / 4 per step via sub-clock). CatSeq advances steps at quarter-note rate (`seqclock.Update()` fires every `bpm_in_ticks` ticks, one step per fire). This is a 4× difference in step rate at the same BPM setting.

**Status:** Needs hardware verification with a reliable MIDI clock source before changing CatSeq. If CatSeq should also be 16th-note resolution, the fix is similar to VoltSeq: add a 16th-note sub-counter in `Sequencer::Interface::Update()` (`src/sequencer.hh`) that drives `per_chan_step` 4× per beat while keeping `seqclock` at quarter-note rate for phase tracking, beat repeat, and ratchet sub-step detection.

**Caution:** CatSeq is at v1.5.0 (hardware verified). The gate track follow, ratchet sub-step detection, and beat repeat all depend on `seqclock` phase — those must remain on the quarter-note clock. Only `per_chan_step` advancement needs to change.

---

### Conditional auto-reset on firmware upgrade

**Area:** `src/sequencer.hh` `Sequencer::Data::validate()`, `src/f401-drivers/saved_settings.hh`

Currently any `current_tag` mismatch wipes all presets on first boot. This is appropriate when `sizeof(Step)` or struct layout changes, but unnecessary for firmware upgrades that only add new fields with safe defaults. A user upgrading within a major version should not lose saved patterns.

**Proposed:** Encode the major/minor version into the tag or add a secondary "layout version" field separate from the feature-additions version. If the flash data's layout version matches the current layout version, migrate forward (fill new fields with defaults) rather than wiping. Only wipe on a layout-breaking change.

May require splitting `current_tag` into two values: `layout_tag` (increment only on struct layout breaks) and `feature_tag` (increment on any additive change, used for migration).

---

## Backlog

### Per-track reset settings

**Area:** `src/ui/seq.hh`, `src/sequencer_player.hh`, `src/sequencer_settings.hh`, Advanced Track Settings

SHIFT+PLAY currently fires an immediate reset and exits. Repurposing it as a **persistent toggle mode** would expose 8 unused step encoders and 8 unused page buttons as a natural per-track reset configuration surface — without adding a new entry combo.

**Proposed UX:**
- First SHIFT+PLAY enters the mode (no immediate reset); second SHIFT+PLAY exits. Play/Reset alone still plays/pauses as normal while inside the mode.
- **Page buttons (1 per track):** toggle whether that track auto-resets when the sequence is paused. Lit = resets on pause; unlit = holds position.
- **Step encoders (1 per track):** configure the reset source/mode for that track:
  - `always` — resets on Play/Reset (current behavior)
  - `never` — Play/Reset has no effect; track runs continuously
  - `follow_chan_N` — resets when track N loops back to step 1, not on the button press itself

The `follow_chan_N` mode is the most powerful: one track's natural loop point drives reset timing for others, allowing phasing relationships to develop and re-sync organically.

**Storage:** `reset_on_pause` bool + `reset_mode` enum + optional `reset_source` channel index in `Settings::Channel`. Requires `current_tag` bump (coordinate with conditional auto-reset item above).

---

### Step Arpeggio

**Area:** `src/sequencer_step.hh`, `src/app.hh` `Cv()`, `src/sequencer_player.hh`, `src/ui/seq_prob.hh`

Per-step arp on CV tracks. 6 chord types (major 5th, minor 5th, major 7th, minor 7th, octave, minor pentatonic); each has ascending/descending direction controlled independently per step. Mutually exclusive with probability.

**Entry:** CCW in SHIFT+GLIDE (quick edit); or SHIFT+GLIDE+TAP TEMPO enters a persistent hands-free editor where encoders set type and page buttons toggle up/down direction. SHIFT+PAGE navigates pages within the persistent mode.

On ratcheted/repeated steps the arp walks the chord across sub-steps (strum/roll). On plain steps it advances one note per clock pass.

Full spec: `docs/planned/CATSEQ-STEP-ARP.md`

Requires `current_tag` bump. Uses bits 0–3 of `sub_step_mask` on CV channels (otherwise unused).

---

### Gate probability iterative mode

**Area:** `src/sequencer_step.hh`, `src/sequencer.hh`, `src/ui/seq_morph.hh`

CCW encoder positions in gate probability mode replace random percentage with a deterministic cycle. CW (1–15) = percentage probability (current behavior). CCW (1–14) = iterative: fires p out of every n steps in rotation (1/2, 2/2, 1/3... up to 5/5).

Requires 1 extra bit in the probability field to represent negative (iterative) values. Candidate: repurpose gate-channel morph bits (morph is unused on gate channels, already partially repurposed for ratchet/repeat display). Player needs a per-channel cycle counter analogous to `repeat_ticks_remaining`.

---

### Sub-step tie/slew in mask edit mode

**Area:** `src/sequencer_step.hh`, `src/ui/seq_substep_mask.hh`

Extends mask edit mode with a third per-sub-step state: tied. A tied sub-step holds the gate open from the previous sub-step rather than retriggering. Requires expanding the sub-step mask from 1 bit per sub-step (`uint8_t`) to 2 bits per sub-step (`uint16_t`). 18 bits available in current `Step` struct; fits with a `current_tag` bump.

---

### Gate Envelope (A/D function generator)

**Area:** `src/app.hh` `Sequencer::App::Gate()`, `src/sequencer_settings.hh`, `src/ui/seq_envelope_assign.hh` (new)

A west coast-style A/D function generator per gate channel. When any envelope parameter is configured, the gate output becomes a shaped voltage envelope. Full spec: `docs/planned/CATSEQ-GATE-ENVELOPE.md`.

Parameters (Follow Assign sub-mode colors):
- Blue = peak amplitude
- Orange = attack time
- Yellow = decay time
- Salmon = curve (shared A+D, log through linear to exponential)

Each parameter independently sourceable: unassigned (fixed default), CV track (page button radio), or gate width (long-press, all buttons lit). Pre-implementation checklist in spec verified — all 8 gate channels are DAC-driven, envelope state belongs in `App`, `last_cv_stepval[]` is accessible in same class.

---

### Advanced Track Settings (rename from Follow Assign)

**Area:** `src/ui/seq_follow_assign.hh`, `src/ui/seq_settings.hh`, `src/ui/seq.hh`, `docs/wiki/Catalyst-Sequencer.md`

The Follow Assign menu currently uses 6 of 9 available FINE sub-mode colors for CV tracks (blue = CV add follow, lavender = CV replace, orange/yellow/salmon = gate clock follow ratchets/repeats/both, teal = gate clock step-only). Gate tracks have no advanced settings menu yet. Both channel types have pending features that need per-track configuration pages (gate envelope, step preview, reset behavior).

Rename and expand to a general Advanced Track Settings menu. Entry combo and persistent-mode behavior unchanged. FINE still cycles sub-modes; with 9 colors available there is room to add pages for both CV and gate tracks without navigation changes.

Planned page layout (subject to revision as features are added):
- CV tracks: blue/lavender = CV follow (existing), orange/yellow/salmon/teal = gate clock follow (existing), remaining colors for new settings
- Gate tracks: blue/orange/yellow/salmon = envelope assign (per `docs/planned/CATSEQ-GATE-ENVELOPE.md`), remaining colors for new settings

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

Per-step clock div/mult allows individual gate steps to fire at a fraction or multiple of the global clock rate. Useful for polyrhythmic patterns without requiring separate clock sources.

Exact UX and storage TBD. Gate tracks only — CV tracks use SHIFT+TAP TEMPO for a different purpose or leave it unassigned.

---

# Catalyst VoltSeq

## v1.5.x

### CV random deviation — refactor to semitone model

**Area:** `src/voltseq.hh` `ComputeDeviation()`, `OnChannelFired()`, `src/app.hh` `CvOutput()`, `src/voltseq.hh` `ChannelSettings::random_amount_v`

Currently `random_amount_v` is stored in whole volts and applied as a raw offset *after* quantization, so the result can land between scale degrees. The unit (volts) also means the minimum non-zero setting (+1) is a full octave of possible deviation — far too coarse for musical use.

**Proposed:** express deviation as a number of semitones and apply the offset *before* quantization. The quantizer then snaps the result to the nearest scale degree. The deviation range is scale-invariant — "±5" always means up to a chromatic 4th regardless of which scale is active — and changing the channel's scale changes only which notes the deviation can land on, not the range.

- **CW from 0 (unipolar):** deviate up by 0–N semitones from the recorded value
- **CCW from 0 (bipolar):** deviate up or down by 0–N semitones symmetric around the recorded value
- Range: ±15 semitones max (same `int8_t` storage as current `random_amount_v`)

**Implementation:** in `OnChannelFired`, compute a random semitone offset, convert to a voltage delta (`offset * (1.f / 12.f)`), add to the pre-quantized step value, then pass through `Quantizer::Process()`. `random_amount_v` reinterpreted as semitones (no struct layout change). Requires `current_tag` bump.

**UX improvement:** the Shift-held sub-modal in armed CV mode currently uses only Enc 5 (Range) and Enc 7 (Transpose), leaving Enc 8 dark. Add **Shift + Enc 8** in armed mode to adjust the channel deviation amount directly — consistent with Range and Transpose being accessible from the same gesture, without requiring a trip to Channel Edit. Probability (Shift+Glide) remains separate as it is per-step rather than per-channel.

**Display:** consider marking octave boundaries (every 12 semitones) in the `RandomAmountColor` ramp with a contrasting color so the user has a tactile landmark for ±1 and ±2 octave deviation.

---

### Reset punch-in timing (#9)

Reset during playback is close but not perfectly in-time. The `primed=false` approach adds ~1-step latency; calling `ResetExternal()` directly from the SHIFT+PLAY handler when playing could eliminate it. Low priority — behavior is usable, not sample-accurate.

---

### Gate track follow does nothing (#3)

Track follow assign mode allows assignments from gate tracks to other gate tracks, but no functionality is defined. Options: block gate→gate assignments in the UI, or implement a useful behavior (e.g. gate clock follow). No milestone set.

---

### Live encoder recording in normal view

In normal view (unarmed), turning an encoder while NOT holding a page button should live-record a value into the current playing step. Coarse increments (range/32 per detent) unless Fine held. Debounce interaction needs tuning.

---

### Global channel length / clock div sync

A combo in Global Settings that sets ALL channels to the same length and/or clock division simultaneously. UX TBD (enc hold, Shift+enc combo, or "apply to all" page-button gesture in Channel Edit).

---

### Channel length — view without editing

Brief passive display on Channel Edit entry before the first enc detent registers as an edit. Possibly just a minimum 2-detent threshold before first change.

---

### CV step fire probability

**Area:** `src/voltseq.hh` `OnChannelFired()`, `src/ui/voltseq.hh` `UpdateArmedCV()`

On CV tracks, `step_prob` currently only gates deviation — it has no effect when `random_amount_v` is 0 and never suppresses the step from outputting. Add step fire probability: SHIFT+GLIDE+ENC CCW on an armed CV track should decrease the probability that the step fires at all (holding the previous CV value instead), consistent with how gate/trigger tracks already treat probability. CW behavior (deviation gating) remains unchanged.

Requires deciding how a single `step_prob` field encodes two distinct behaviors (deviation gate vs. fire suppression), or splitting into two fields. Requires `current_tag` bump if storage changes.

---

### Global BPM — 1 BPM steps + snap points

Store BPM as integer; snap to common tempos (60, 80, 90, 100, 120, 130, 140, 150, 160, 180, 200 BPM) with a 2-detent magnetic pull. Update wiki with snap values.

---

## Known Quirks

- **GLIDE inaccessible while armed Trigger** — pulse width adjustment requires disarming. Document as intentional or add a Glide-held sub-state to `UpdateArmedTrigger`.
- **Perf Page step-edit block is dead code** — `AnyStepHeld()` is always false in Perf Page; the encoder block never fires. Perf Page is a settings modal, not a step editor — remove the dead block.
- **`current_page_` bleeds between modes** — navigating in Perf Page or Glide Editor affects what Normal mode shows on return. Low-severity; SHIFT+Page recovers. Fix: reset `current_page_` on mode transitions where it matters.
- **Wholetone scale and unquantized CV show the same grey** — color collision, needs a distinct color for wholetone.
- **Phase rotate (Channel Edit enc 4) is destructive with no undo** — consider a "rotate pending" visual before committing.
- **Trigger pulse width (Channel Edit enc 5) has no display feedback** — consider a brightness ramp.

---

## Backlog

### Clock division redesign — multipliers + triplets

Full design in `docs/planned/VOLTSEQ-CLOCK-DIVISIONS.md`.

---

### Presets / scene save-load

See `docs/planned/VOLTSEQ-PRESETS.md`.

---

### Iterative probability — per-step cycle filter

See `docs/planned/VOLTSEQ-ITERATIVE-PROBABILITY.md`.
