# Changelog

All changes relative to upstream 4ms-company/catalyst-firmware v1.3.

**v1.4.1** — Phase Scrub Lock only (stable, hardware verified)
**v1.4.2** — Ratchet expansion, Repeat mode + startup fixes (hardware verified)
**v1.4.3** — Sub-step mask edit mode: user-editable per-step bitmasks via Page buttons (**in progress**)

---

## Implemented

### Phase Scrub Lock (Sequencer mode)
**Combo:** Fine + Glide (Ratchet)

Locks the Phase Scrub slider at its current position. A second press unlocks; the slider resumes control only after physically reaching the locked value (pickup behavior), preventing jumps. Encoder 8 LED blinks red while locked or during pickup, and briefly on toggle.

---

### Ratchet expansion (Sequencer mode, gate channels)
**Combo:** Hold Glide (Ratchet), turn step encoder CW

Ratchet (subdivide) count expanded from **4x max to 8x max**. Each step can subdivide into up to 8 equal sub-steps per clock tick.

Ratchet count displayed in Glide (Ratchet) mode: dim salmon → bright red with increasing count (1x–8x). **Hardware verified.**

---

### Repeat mode (Sequencer mode, gate channels)
**Combo:** Hold Glide (Ratchet), turn step encoder CCW

A new per-step mode (mutually exclusive with ratchet). Instead of subdividing, the step fires an additional N times at the current clock division, **extending the effective pattern length**. An 8-step pattern with one step set to repeat ×3 becomes 11 clock ticks long.

Repeat count displayed in Glide (Ratchet) mode: dim teal → blue with increasing count (1x–8x).

Play/Reset clears all repeat counters cleanly. **Hardware verified.**

---

**Note:** `sizeof(Step)` expanded from 4 to 8 bytes. First boot after flashing v1.4.2 automatically resets seq data — no manual factory reset required.

---

## TODO

### v1.4.2 — Hardware verification notes
Phase scrub lock, ratchet, and repeat are fully verified on hardware.

v1.4.2 bricked the module twice during development. Root causes identified and fixed:
- Startup crash on struct layout mismatch: `Sequencer::Data::SettingsVersionTag` added, `validate()` now rejects incompatible flash data, `ui.hh` resets to defaults on mismatch.
- `IncRatchetRepeat()` CCW from neutral entered ratchet mode instead of repeat mode.
- `GatePattern::Get()` had no bounds check — out-of-range `gate_pattern` values would walk off the table.
- `Step::Validate()` did not check `gate_pattern` range.

---

### Future — Quantized glide / implicit arp (CV channels, CCW)
Currently CW on Glide increases smooth portamento (morph) between step values. Proposed CCW behavior: **quantized glide** — instead of interpolating smoothly, the CV steps through quantized scale notes between the previous and current step's pitch.

Same signed-position model as ratchet/repeat and probability:
- **CW (positive):** smooth glide — current morph behavior
- **CCW (negative):** quantized glide — walk through N intermediate scale notes between the two programmed pitches

When a gate channel on the same physical channel is set to N ratchets, and the CV channel is set to quantized glide, the CV snaps to a new scale note on each sub-step — **producing an arp without a dedicated arp mode**. The ratchet count controls arp speed; the two programmed step pitches define the arp range; the active scale defines which notes are valid.

Implementation notes:
- **Preferred approach:** divide step phase into N equal segments independently on the CV channel, snap to nearest scale note per segment — fully self-contained, no cross-channel state. The arp emerges naturally because both channels subdivide the same step phase; they stay in sync without coordination. Mismatched subdivision counts produce polyrhythmic arp patterns within a step.
- The morph field (4 bits, currently positive-only) would become signed: positive = smooth glide steps, negative = quantized glide steps. Needs 1 extra bit or reuse of another field. Same struct constraint as probability iterative mode.

---

### Future — Gate probability: iterative (CCW) mode
Currently gate probability is percentage-based (random). Proposed: same CW/CCW signed-position model as ratchet/repeat.

- **CW (positive):** percentage probability — current behavior, 15 steps from 100% down to ~6%
- **CCW (negative):** iterative/deterministic — fires on p out of every n steps in a rotating cycle

Iterative sequence (CCW 1–14): `1/2, 2/2, 1/3, 2/3, 3/3, 1/4, 2/4, 3/4, 4/4, 1/5, 2/5, 3/5, 4/5, 5/5`

14 iterative values + 15 percentage values fits in a signed 5-bit field (−14..0..+15). Current `probability` is 4 bits (0–15 unsigned) — would need 1 extra bit, which means either repacking the Step struct (version tag bump) or repurposing the morph bits on gate channels (morph is currently unused on gate channels — already partially repurposed for ratchet/repeat display).

Player would need a per-channel cycle counter to track position within the n-step window, similar to `repeat_ticks_remaining`.

---

### Future — Mask pattern table redesign
Current 6 patterns include Mute (all off) which is redundant with setting gate width to 0. Proposed replacement set focused on musical utility:

| # | Name | 4x example | Notes |
|---|------|-----------|-------|
| 0 | All on | `xxxx` | default, unchanged |
| 1 | All tied | `x---` | one long gate |
| 2 | First on | `xooo` | accent first sub-step |
| 3 | Last on | `ooox` | accent last sub-step |
| 4 | Alternating | `xoxo` | even sub-steps only |
| 5 | Alternating tied | `x-x-` | pairs of tied sub-steps |

Drops Mute (index 5) in favor of Last on (new). **Requires `Sequencer::Data::current_tag` increment** — stored pattern index 5 currently means Mute; after the change it means Alternating tied, so existing saved patterns would be misread.

---

### v1.4.3 — Sub-step mask edit mode (in progress)

Replaces the v1.4.2 preset mask pattern system with user-editable per-step bitmasks. The preset GatePattern table and RatchetMask UI are removed; each step stores an 8-bit bitmask where each bit controls whether that sub-step fires or is silent.

**Entry/exit:** Glide + Tap Tempo toggles the mode (changed from momentary to toggle). Play/Reset also exits.

**Editing:** Touching a step encoder focuses it. Page buttons 1–8 show the sub-step mask for the focused step — lit = fires, unlit = silent. Press any Page button to toggle that sub-step. Sub-step 0 (Button 1) is always forced on. Only buttons up to the current ratchet/repeat count are active.

**Count adjustment:** Continuing to turn the encoder within 300ms of touching it adjusts ratchet (CW) or repeat (CCW) count using existing behavior and feedback. After 300ms idle the encoder is locked to focus-only.

**Mask storage:** `uint8_t sub_step_mask` added to `Step` struct (8 bits, 26 bits were free). Default `0xFF` (all sub-steps fire) preserves existing behavior on unedited steps. `sizeof(Step)` unchanged at 8 bytes; `current_tag` bumped to `2u` to force clean reset (old flash would read mask bits as 0x00 = all silent).

**Scope:** Ratchet sub-steps only. Repeat step mask application is a separate future item (see below).

---

### Future — Sub-step tie/slew in mask edit mode

In mask edit mode, encoders currently only adjust ratchet/repeat count. A future enhancement could allow encoder turns to cycle a sub-step through states: silent → fire → tied (gate held open from previous sub-step). Tied sub-steps were prototyped in v1.4.2 (`SubStepState::Tied`, `GatePattern` table) but removed in v1.4.3. Re-introducing them as a per-sub-step state requires expanding the mask from 1 bit per sub-step to 2 bits — growing `sub_step_mask` from `uint8_t` to `uint16_t` (16 bits). At 46 bits currently used in the Step struct after v1.4.3, that's 18 bits remaining in the 8-byte struct: a 16-bit mask fits without changing `sizeof(Step)`, but would require another `current_tag` bump.

---

### Future — Repeat step mask application
Currently mask has no effect on repeat steps. Fix requires using the repeat tick index as the sub-step index when `s.IsRepeat()`.

Files to change:
- `src/sequencer.hh` — add `GetRepeatTicksRemaining(uint8_t chan) const` getter
- `src/app.hh` — in `Gate()`, branch on `s.IsRepeat()` and use `s.ReadRatchetRepeatCount() - p.GetRepeatTicksRemaining(chan)` as the sub-step index
