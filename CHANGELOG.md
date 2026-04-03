# Changelog

All changes relative to upstream 4ms-company/catalyst-firmware v1.3.

| Version | Summary | Status |
|---------|---------|--------|
| v1.4.1 | Phase Scrub Lock | Hardware verified |
| v1.4.2 | Ratchet expansion, Repeat mode | Hardware verified |
| v1.4.3 | Sub-step mask edit mode | Hardware verified |
| v1.4.4 | Linked Tracks: CV transpose follow + gate track clock follow | Testing |

**Note on preset compatibility:** v1.4.2 expanded `sizeof(Step)` from 4 to 8 bytes. Presets saved under v1.3 are not compatible and are discarded on first boot. The firmware detects the mismatch via a version tag and resets to defaults automatically -- no manual factory reset needed.

---

## Implemented

### Phase Scrub Lock (v1.4.1)

**Combo:** Fine + Glide (Ratchet)

Locks the Phase Scrub slider at its current position. A second press unlocks with pickup behavior: the slider resumes control only after physically reaching the locked value, preventing position jumps. Encoder 8 LED blinks red while locked or during the pickup window, and briefly on toggle.

---

### Ratchet expansion (v1.4.2)

**Combo:** Hold Glide (Ratchet), turn step encoder CW (gate channels only)

Ratchet (subdivide) count expanded from 4x maximum to 8x maximum. Each step can subdivide the current clock tick into up to 8 equal sub-steps, all firing within that single tick.

**Display in Glide mode:** dim salmon (1x) to bright red (8x).

---

### Repeat mode (v1.4.2)

**Combo:** Hold Glide (Ratchet), turn step encoder CCW (gate channels only)

A new per-step mode, mutually exclusive with ratchet. Instead of subdividing the current tick, the step fires an additional N times at the current clock division, each on its own master clock tick. This extends the effective pattern length -- an 8-step pattern with one step set to repeat x3 plays over 11 clock ticks.

CW from any repeat value passes back through neutral (no ratchet, no repeat) before entering ratchet mode. Play/Reset clears all active repeat counters.

**Display in Glide mode:** dim teal (1x) to blue (8x).

---

### Sub-step mask edit mode (v1.4.3)

**Entry:** Hold Glide (Ratchet) + Tap Tempo on a gate channel step, then release both buttons. Mode persists until you exit.

**Exit:** Tap Tempo alone, or Play/Reset.

Each ratcheted or repeated step stores an 8-bit sub-step mask. Bits control which sub-steps fire and which are silenced. All sub-steps including the first can be silenced (changed in v1.4.5).

**Editing:**
- Touch any step encoder to focus it (blinks yellow while idle).
- Page buttons 1-N show the sub-step mask for the focused step: lit = fires, unlit = silenced.
- Press a Page button to toggle that sub-step. Buttons beyond the current ratchet/repeat count are inactive.
- Turn the focused encoder within 300ms of first touch to adjust ratchet (CW) or repeat (CCW) count.
- Sub-step mask applies to both ratchets and repeats. Silenced sub-steps do not trigger gate track clock follow on linked CV tracks.

**Entry on CV channels is blocked** -- morph mode applies there instead.

---

### Linked Tracks: Follow Assign mode (v1.4.4)

**Entry:** SHIFT + Chan. (enters Channel settings), tap Glide (Ratchet). Mode persists after releasing all buttons. Exit with Glide (Ratchet) or Play/Reset.

**Sub-mode selection:** FINE cycles through four sub-modes. The selected channel's encoder blinks in the active sub-mode color. Page buttons select the source track (radio: press lit to unlink, press unlit to assign, self is ignored).

**Blue -- CV add follow:** The source CV track's current pitch (relative to 0V) is added to the target track's pitch each update cycle, transposing the entire sequence by the source value.

**Orange -- Gate clock, ratchets only:** The source gate track replaces the master clock for this CV track. Each ratchet sub-step on the gate track advances the CV track one step (producing a different pitch per sub-step). Repeat ticks do not advance the CV track beyond the first tick of each step.

**Yellow -- Gate clock, repeats only:** Each repeat tick on the gate track advances the CV track one step. Ratchet sub-steps within a step do not produce additional CV advances.

**Salmon -- Gate clock, ratchets + repeats:** Both ratchet sub-steps and repeat ticks advance the CV track.

**Teal -- Gate clock, step only (added v1.4.5):** The CV track advances once per gate step, ignoring ratchets and repeats entirely. Useful when the gate track drives rhythm but you want the CV track to move at the step rate only.

**Lavender -- CV replace (added v1.4.5):** On steps where both this track and the source track advance simultaneously (intersection), the source track's programmed pitch replaces this track's own step value. Source steps at 0V act as pass-through, leaving this track's own pitch intact. No replacement occurs on steps where only one track advances. Encoded in `transpose_source` as values `NumChans+1..2*NumChans` to avoid a struct layout change. Replacement value is read directly from the source step at the intersection boundary (not from the `last_cv_stepval` cache) to avoid a one-step phase offset caused by channel processing order.

In all three gate clock modes, the first tick of every gate step always advances the CV track regardless of ratchet/repeat settings. Sub-steps silenced via sub-step mask do not produce an advance. A muted gate track produces no advances. Multiple CV tracks may follow the same gate track with different modes independently. CV transpose follow and gate clock follow are independent per-channel settings and can be active simultaneously.

**Implementation notes:**

- `src/sequencer_settings.hh` -- `transpose_source`, `clock_source`, `clock_follow_mode` fields in `Settings::Channel` (0=unlinked, 1..8=source chan 1-based; mode 0=ratchets, 1=repeats, 2=both). `current_tag` bumped to `5u`.
- `src/app.hh` `Cv()` -- CV transpose via `last_cv_stepval[]` cache. Offset = `int32_t(last_cv_stepval[src]) - int32_t(Channel::Cv::zero)`, clamped, added to stepval. One-tick delay (~0.33ms).
- `src/sequencer.hh` `Interface::Update()` -- Gate Track Follow block runs every 3kHz iteration. Three orthogonal advance signals per gate channel: `gate_step_adv` (step boundary), `gate_ratchet_adv` (ratchet sub-step crossings via `floor(count * step_phase)` delta), `gate_repeat_adv` (one-shot flag from recording block). Each CV channel's `clock_follow_mode` selects which signals advance it.
- `src/sequencer_player.hh` -- `Update()` takes `std::array<uint8_t, NumChans>` (count, not bool) to carry multi-step advances per tick.
- `src/ui/seq_follow_assign.hh` -- persistent Follow Assign mode; FINE cycles sub-modes; page buttons are radio selector.

---

## Planned

See `TODO.md` for the full backlog with implementation notes.
