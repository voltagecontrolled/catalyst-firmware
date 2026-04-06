# Changelog

All changes relative to upstream 4ms-company/catalyst-firmware v1.3.

| Version | Summary | Status |
|---------|---------|--------|
| v1.4.1 | Phase Scrub Lock | Hardware verified |
| v1.4.2 | Ratchet expansion, Repeat mode | Hardware verified |
| v1.4.3 | Sub-step mask edit mode | Hardware verified |
| v1.4.4 | Linked Tracks: CV transpose follow + gate track clock follow | Hardware verified |
| v1.4.5 | Gate clock step-only mode, CV replace follow, bugfixes | Hardware verified |
| v1.4.6 | Phase Scrub Performance Page: granular sequencing, beat repeat, lock persistence, sub-step page nav | Release Candidate |

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

### Linked Tracks: Teal and Lavender modes (v1.4.5)

Two additional follow modes added to the Follow Assign menu (SHIFT + Chan., tap Glide):

**Teal — Gate clock, step only:** CV track advances once per gate step, ignoring ratchets and repeats. Useful when a ratcheted gate track drives rhythm but you want steady one-pitch-per-step CV movement.

**Lavender — CV replace:** At steps where both the target and source track advance simultaneously (intersection), the source track's programmed pitch replaces the target's own step value. Source steps at 0V act as pass-through. Encoded in `transpose_source` as values `NumChans+1..2*NumChans`. Replacement reads the source step directly (not `last_cv_stepval[]` cache) to avoid a one-step phase offset caused by channel processing order.

Also fixed: page button state when switching between gate clock follow modes; Channel Settings entry bounce (Glide button overlap); sub-step 1 can now be silenced in the sub-step mask editor.

---

### Sub-step page navigation (v1.4.6)

**Combo:** SHIFT + PAGE button (while in sub-step mask edit mode)

Changes page without exiting sub-step edit mode. Previously, the only way to reach a different page of a long sequence was to exit the mode (losing edit context). SHIFT+PAGE selects the target page directly; the focused step clears so you can focus a step on the new page.

---

### Phase Scrub lock persistence (v1.4.6)

Phase Scrub lock state and locked slider position now survive power cycles. On boot with lock restored, the slider enters pickup mode automatically (same as a live unlock+relock). Saved in `Shared::Data` via the existing `do_save_shared` path.

---

### Phase Scrub Performance Page (v1.4.6)

**Entry:** COPY + GLIDE held 1.5 seconds (fires at 1.5s, no release needed). **Exit:** COPY + GLIDE (any duration) or Play/Reset.

**Lock toggle (outside menu):** COPY + GLIDE, release either button before 1.5s. Fires on release, so continuing to hold transitions into menu entry with no conflicting mid-press state.

A dedicated performance page for Phase Scrub controls, replacing the basic lock combo with a full menu. All settings persist across reboots.

| Encoder | Function | Colors |
|---------|----------|--------|
| 1 | Quantize | orange = on, off = off |
| 2 | Slider Performance Mode | off / green / blue / cyan |
| 3 | Granular Width (granular) or Debounce Delay (beat repeat) | off→orange / dim→bright white |
| 4 | Orbit Direction | green / blue / orange / lavender |
| 8 | Phase Scrub Lock | red = locked, off = unlocked |

Page buttons toggle per-track scrub participation: lit = track follows scrub, unlit = track ignores scrub and plays normally.

**Quantize (Enc 1):** Two behaviors depending on slider performance mode.
- *Standard/Granular:* snaps the scrub phase to the nearest step boundary. Uses `round(phase / step_size) * step_size` — deterministic, no intersection logic.
- *Beat Repeat:* controls entry timing. Off = fires the instant SHIFT releases (raw timing, full player control). On = snaps to the nearest step boundary: fires immediately if SHIFT releases in the first half of the current step, waits for the next step boundary if in the second half. Max wait = half a step period.

**Slider Performance Mode (Enc 2):** Selects what the slider does during performance.

- **Off (unlit):** Standard phase scrub.
- **Green — Granular:** Slider positions a looping orbit window within the sequence. Enc 3 sets window width (0–100% of pattern length); Enc 4 sets playback direction. Width=0 or slider at minimum disables orbit and falls back to standard scrub.
- **Blue — Beat Repeat (8-zone, with triplets):** Slider selects a subdivision rate from 8 zones: 1/2, 1/4, 1/4T, 1/8, 1/8T, 1/16, 1/16T, 1/32. Slider at minimum turns beat repeat off. Enc 3 sets debounce delay (see below). Enc 4 sets orbit direction as in granular mode.
- **Cyan — Beat Repeat (4-zone, no triplets):** Like Blue but with 4 wider zones: 1/2, 1/4, 1/8, 1/16. Easier to land on a target while performing. Enc 3 and Enc 4 same as Blue.

**Enc 3 — context-aware:** In granular mode, adjusts orbit width (0–100%, LED = dim→bright orange). In either beat repeat mode, adjusts the debounce delay before a new zone commits (8 steps, LED = dim→bright white; default = 150ms, range 50ms–1500ms). Debounce setting is transient, not saved across reboots.

**Shift staging (beat repeat):** Hold SHIFT to freeze both the committed zone and `orbit_center`. The slider can be moved freely — no scrubbing occurs and no new zone commits. On SHIFT release, the pending zone commits immediately and `orbit_center` resumes tracking the slider. Useful for staging a division change or a clean return to zero without chaotic scrubbing in transit.

**Beat repeat entry and step lock:** On first entry from off, the sequencer snaps `orbit_center` to the current player step at the moment of first fire, so the step you hear first is the step that loops. When quantize is off, first fire is immediate (SHIFT release tick). When quantize is on, first fire is at the nearest step boundary (immediate if in the first half of the step, else next clock_ticked). In both cases `orbit_center` is snapped at the actual fire moment. Slider enters pickup mode after entry; moving it takes over `orbit_center` continuously.

**Orbit behavior:** The orbit position counter advances continuously; moving the slider to a new center shifts the window without resetting the position. Quantize snaps the center to the nearest step boundary if enabled. `scrub_ignore_mask` applies — ignored channels continue normal playback regardless of mode. Phase Scrub Lock is respected in all modes: when locked, the orbit center and beat repeat zone calculation use the locked slider value rather than the physical slider position.

**Implementation notes:**

- `src/shared.hh` `Shared::Data` — `slider_perf_mode`: 0=standard, 1=granular, 2=beat-repeat 8-zone (triplets), 3=beat-repeat 4-zone (no triplets). `orbit_width`, `orbit_direction` persisted. `Shared::Interface` — transient fields: `orbit_center`, `beat_repeat_committed`, `beat_repeat_pending`, `beat_repeat_pending_since`, `beat_repeat_debounce_idx`, `beat_repeat_snap_pending`.
- `src/sequencer.hh` `Interface` — private orbit state: `orbit_active`, `orbit_pos`, `orbit_ping_dir`, `orbit_step[NumChans]`, `orbit_step_prev[NumChans]`, `beat_repeat_countdown`, `beat_repeat_phase`, `beat_repeat_synced`. `GetEffectiveStepPhase(chan)` checks `OrbitActiveForChannel(chan)` (not bare `orbit_active`) so excluded channels use their own step phase. Beat repeat uses independent countdown timer derived from `GetGlobalDividedBpm()`; first fire waits for `clock_ticked` (`beat_repeat_synced` flag) to align to step grid. At the first `clock_ticked`, if `beat_repeat_snap_pending` is set, `shared.orbit_center` is snapped to the current player step before `orbit_step[]` is computed.
- `src/app.hh` `Cv()` — substitutes orbit steps when `OrbitActiveForChannel(chan)`. `Gate()` — uses `GetOrbitStepCluster()` and `GetEffectiveStepPhase()` when orbit active.
- `src/ui/seq_common.hh` `Usual::Common()` — when `slider_perf_mode > 0`: derives `effective_slider` (uses `locked_raw` when `phase_locked || picking_up`), sets `orbit_center` (frozen while SHIFT held or in orbit pickup mode), runs beat repeat zone debounce with Shift staging, passes `phase=0` to `p.Update()`. On shift-release entry from off: sets `beat_repeat_snap_pending` and enters pickup mode (`orbit_pickup_slider` captures slider position at entry). COPY+GLIDE combo: both-held → arm timer; release either before 1.5s → `DoLockToggle()`; hold 1.5s → menu entry.
- `src/ui/seq_scrub_settings.hh` — enc 3 context-aware: `orbit_width` in granular, `beat_repeat_debounce_idx` (transient) in beat repeat modes; mode switch clears `beat_repeat_committed/pending`. Save deferred to exit paths to avoid flash write stutter.

---

## Planned

See `TODO.md` for the full backlog with implementation notes.
