# Changelog

All changes relative to upstream 4ms-company/catalyst-firmware v1.3.

| Version | Summary | Status |
|---------|---------|--------|
| v1.4.1 | Phase Scrub Lock | Hardware verified |
| v1.4.2 | Ratchet expansion, Repeat mode | Hardware verified |
| v1.4.3 | Sub-step mask edit mode | Hardware verified |
| v1.4.4 | Linked Tracks: CV transpose follow + gate track clock follow | Hardware verified |
| v1.4.5 | Gate clock step-only mode, CV replace follow, bugfixes | Hardware verified |
| v1.4.6 | Phase Scrub Performance Page: granular sequencing, beat repeat, lock persistence, sub-step page nav | Released |
| v1.5.0 | VoltSeq mode: 8-channel step sequencer replacing Macro mode; three build variants (CatSeq+CatCon, CatSeq+VoltSeq, VoltSeq+CatCon) | Alpha — branch: voltseq |

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

### VoltSeq mode (v1.5.0)

A second firmware personality for the Catalyst Sequencer panel. Replaces Macro mode with an 8-channel step sequencer in the style of Voltage Block. Ships as a separate build variant (`catseq-voltseq`).

**Mode switching (in-firmware, no reflash):**
- VoltSeq → Sequencer: hold Fine + Play/Reset + Glide 1 second. All channel LEDs blink to confirm.
- Sequencer → VoltSeq: hold Shift + Tap Tempo + Chan. 1 second. All channel LEDs blink to confirm.
- Active mode saved to flash; switching does not erase the other mode's data (independent flash sectors).

**Channel types:** Each of the 8 channels is independently set to CV, Gate, or Trigger.
- CV: stepped or slewed voltage, per-channel configurable range (default −5V to +5V). Quantizable to built-in or custom scales. Encoder acceleration (faster spin = larger steps).
- Gate: variable-length gate (0–100%), x0x-style step editing.
- Trigger: short pulse (1–100ms), ratchet (subdivide, 1–8×, green) or repeat (extend, 1–8×, teal) per step.

**Step editing (normal mode):**
- Hold a Page button, turn Encoder N to set channel N's value at that step. Multiple Page buttons can be held simultaneously.
- 8 pages × 8 steps = 64 total steps per channel.
- Shift + Page button navigates pages.

**Armed CV channel:**
- Chan. + Page button N arms that channel; encoder LEDs show each step's CV color plus a white chaselight on the playing step.
- Encoder N directly edits step N's value on the current page. Fine = sub-semitone; acceleration applies.
- Slider records into the channel in real time (motion-gated). Slider maps to the channel's configured Range.
- Shift + Enc 5 (Range) adjusts the channel voltage range (also sets the slider recording window). Shift + Enc 7 (Transpose) cycles the quantizer scale.

**Armed Gate channel:**
- Encoder LEDs show step gate state (brightness = length) + white chaselight on playing step.
- Tap a Page button to toggle step on/off. Encoder N directly adjusts gate length for step N.

**Armed Trigger channel:**
- Encoder LEDs show step state (green = ratchet, teal = repeat, off = rest) + white chaselight on playing step.
- Tap a Page button to toggle step on/off. Encoder N CW = ratchet, CCW = repeat for step N.

**GLIDE modifier:** Hold Glide and turn an encoder to adjust per-channel: CV glide time (0–10s), gate length offset, or trigger pulse width (1–100ms). Shift + Glide + encoder offsets all ratchet/repeat counts for a Trigger channel.

**Glide Step Editor:** Hold Glide, long-press a Page button (600ms) on a CV or Gate channel. Encoder N toggles the per-step glide flag (CV) or adjusts gate length (Gate). Shift + Page navigates pages. Exit with Glide or Play/Reset.

**Channel Edit (Shift + Chan.):** Per-channel settings; encoders aligned to panel silkscreen labels. Press a Page button to focus a channel. Long-press a Page button (600ms) to clear all steps for that channel. Exit with Chan. or Play.

| Encoder | Panel label | Parameter |
|---|---|---|
| 1 | Start | Output delay (0–20 ms) |
| 2 | Dir. | Direction: Forward / Reverse / Ping-Pong / Random |
| 3 | Length | Step length (1–64, accelerated) |
| 4 | Phase | Phase rotate (destructive; rotates only active steps within length) |
| 5 | Range | Voltage range (CV) / Pulse width (Trigger, 1–100ms) |
| 6 | BPM/Clock Div | Clock division |
| 7 | Transpose | Channel type / quantizer scale (CV scales → Gate → Trigger, clamped) |
| 8 | Random | Random amount (0–100%) |

**Global Settings (Shift held 1.5 s alone — any other button cancels):** Exit via Play/Reset. Enc 1 (Start) = play/stop reset mode; Enc 3 (Length) = master reset steps (0–64; red at 8/16/32/64 snap values); Enc 6 (BPM) = internal BPM with ROYGBIV color zone. Page buttons select reset leader channel (radio; tap to set, tap again to clear).

**Performance Page (Fine + Glide held 1.5s):** Phase Scrub orbit engine ported from CatSeq. Slider controls orbit center; Page buttons toggle per-channel orbit follow. Settings sub-page (Fine + Glide held 1.5s again) exposes mode, width/debounce, direction, and Phase Scrub Lock — same as CatSeq scrub settings.

**Save behavior:** Saves automatically on play/stop toggle, Channel Edit exit, Glide/Ratchet editor exit, and Performance Page settings exit.

**Alpha build history:**

| Build | Key changes / bugs fixed |
|-------|--------------------------|
| alpha1 | Initial implementation: full 8-channel engine, CV/Gate/Trigger types, x0x editing, GLIDE editor, Channel Edit, orbit/Performance Page, mode switch |
| alpha2 | Slider recording (motion-gated); trigger repeat infinite-loop fix (`FirePulse` helper); chaselight on page buttons (CV); channel type UX; Play/Reset exits all modal states |
| alpha3 | Default range corrected to −5V..+5V (bipolar); all default steps centered at 0V (32768); Channel Edit encoder order matches panel silkscreen; armed CV redesign (encoder N = step N, slider records to range, Shift+enc 5/7 for range/scale); step-1 inaccessible in armed Gate/Trig fixed (bare disarm removed); Shift debounce fix (pre-read `bank_jgh` to prevent spurious Channel Edit entry); phase rotate scoped to active steps only; encoder acceleration (`EncoderAccel`); chaselight on encoder LEDs in armed mode; direction feedback in Channel Edit; clear channel (long-press page button in Channel Edit) |
| alpha4 | Step 1 exits Glide/Ratchet Step Editor fixed (removed bare page-button exit; exit now Glide or Play only); step 2 fires first after Reset+Play fixed (`primed[]` array skips advance on first `OnChannelFired`); CHAN+encoder type cycling accelerated (`enc_accel_`); repeat chaselight blinks at ~47 Hz (vs 12 Hz) while `repeat_remaining > 0`; Performance Page entry now flashes page buttons as confirmation (blinker); Channel Edit length display reverts after 600 ms of enc 2 inactivity; unquantized CV encoder LEDs now show visible grey (was `very_dim_grey` = nearly off) |
| alpha5 | Clear channel/pattern mode (SHIFT+PLAY held 600ms → page N = clear ch N, PLAY = clear all, any other = exit; slow blink on entry); SHIFT+PLAY reset moved from press to release (short hold < 600ms) to avoid accidental reset when entering clear mode; orbit follow-mask toggle now calls `do_save_shared` so per-channel exclude state persists across power cycles |
| alpha6 | Encoder acceleration toned down (3-tier 64×/16×/4× → 2-tier 16×/4×, tighter thresholds); acceleration now active only in CV step editing — all other modes (gate editing, type cycling, length, BPM) use unaccelerated delta; chaselight blink suppressed on held page button so step color is visible while editing; Play exits armed mode without stopping playback; Channel Edit exit fixed (Play consumed at top-of-Update; SHIFT+CHAN now toggles entry/exit); Glide/Ratchet editor Play exit fixed (same root cause); CHAN+encoder type-change save deferred to CHAN release (was triggering a flash write per detent, causing clock stumble); tap tempo requires ≥ 3 taps before updating BPM (both CatSeq and VoltSeq); custom scales removed from type selector (all had duplicate colors matching built-ins); global settings encoders remapped to match panel labels (enc 2=Dir, 3=Length, 5=Range, 6=BPM); Shift-held encoder LED feedback added for global settings |
| alpha9 | Global Settings redesigned as dedicated modal (Shift held 1.5 s alone — any other button press cancels); page buttons now select reset leader channel (radio: tap to set, tap again to clear); removed default direction/length/range from global settings; enc 1 (Start) = play/stop reset mode (Stop also resets to step 1), enc 3 (Length) = master reset steps (0–64; LED red at 8/16/32/64 snap values, orange otherwise, off when disabled), enc 6 (BPM) = internal BPM (yellow pulse; white at 80/100/120/140 BPM snap points); Play/Stop reset mode implemented (stop triggers Reset when enabled); master reset counter auto-resets all channels every N master ticks (overrides reset leader); reset leader channel resets all other channels on leader wrap; short Fine+Glide (Phase Scrub Lock toggle) fixed — was incorrectly exiting Performance Page; direction selector changed to clamped (was wrapping); chaselight focus tracking fixed — follows last encoder turned in step-edit mode; stale encoder events drained when no step is held (was bleeding into next step edit); chaselight added to Channel Edit page buttons; GetOutputStep fix — held steps wrap by channel length so short channels (e.g. 8-step trig) repeat correctly when editing a longer channel on page 2+; `current_tag` bumped to 4 |
| alpha10 | SHIFT+CHAN hold 2s for Global Settings (replaces broken hold-Shift); short SHIFT+CHAN = Channel Edit; BPM indicator redesigned as ROYGBIV color zones (< 50 = red, 50–79 = orange, 80–99 = yellow, 100–119 = green, 120–149 = blue, 150–179 = teal, 180+ = lavender); Phase Scrub Lock indicator: enc 8 red when locked, blinks red during pickup; Channel Edit length preview on entry (600ms passive display); per-step glide flag editing in armed CV (Glide held + enc N = set/clear) |
| alpha11 | Ratchet Step Editor removed (ratchets fully accessible via armed Trigger enc N and unarmed hold-step+enc); Gate ratchets added — Gate step encoding changed: high byte = gate length (0–255 ≈ 0–100%), low byte = ratchet count (0/1 = single gate, 2–8 = N subdivided pulses per step); armed Gate + Glide held + enc N = per-step ratchet count; unarmed Glide + page-button held + enc N = per-step ratchet for Gate channels; gate ratchet LED display when Glide held (dim grey = none, bright green = high count); global GLIDE+enc 2 glide time removed from Channel Edit; `current_tag` bumped to 5 |
| alpha12 | Chaselight brightness reduced to ~43% white (Color(110,110,110)) in armed mode — step colors no longer overwhelmed by full-white playhead indicator |
| alpha13 | SHIFT+PAGE navigates pages in all modes (normal, armed CV/Gate/Trigger, Channel Edit, Glide Step Editor already had it); encoder wiggle in main mode with no step held now updates chaselight focus channel immediately; shift-held state shows page indicator on page buttons (currently selected page lit) and blinks the focused channel encoder; Channel Edit entry syncs `current_page_` to the focused channel's current playhead page (fixes chaselight disappearing after cross-mode page navigation) |
| alpha14 | Step clock rate changed to 16th-note resolution — `StepPeriodTicks` divides `bpm_in_ticks` by 4; tap tempo remains quarter-note input so tapping 120 BPM produces steps at 480 steps/min (16th notes at 120 BPM) |
| alpha15 | Modal entry blocked from non-main states: SHIFT+CHAN (Channel Edit / Global Settings) and Fine+Glide (Performance Page) now silently ignore entry gestures while any modal is active; Armed → Channel Edit remains the one allowed cross-state entry; no modal stacking |
| alpha16 | Fix phase scrub lock indicator to match CatSeq behavior (event-driven flash only: on toggle, while slider moves when locked, during pickup; silent when locked and idle); fix Fine+Glide short tap inside Perf Settings — was exiting to Perf Page normal before the lock toggle fired; now stays in Settings |
| alpha17 | Remove Glide Step Editor — long-press Glide+page-button no longer enters a persistent editor; Glide+page-button held is now unambiguously Gate ratchet editing with no 600ms timing cliff; CV per-step glide flags remain accessible via armed CV + Glide + enc N; stale-timer collision #10 eliminated |
| alpha18 | Fix beat repeat (blue/cyan perf modes): Gate and Trigger channels now fire at the subdivision rate set by the performance page zone; previously channels only fired at their own clock division rate and the orbit window was always 1 (center step only), so no stuttering occurred; orbit-following channels are now suppressed from their normal clock fires during beat repeat and instead fire via the orbit advance timer using `beat_repeat_safe_period_` for gate/ratchet duration |
| alpha19 | Remove Channel Edit long-press clear: page button in Channel Edit is now tap-to-focus only (immediate on rising edge, no timer); channel clearing exclusively via SHIFT+PLAY → Clear Mode; removes accidental-clear hazard during editing; `channel_edit_clear_timer_`, `channel_edit_clear_btn_`, `kNoLongpressBtn`, and `kLongpressMs` removed |
| alpha20 | Remove dead step-edit block from UpdatePerfPage: `ForEachEncoderInc` guarded by `AnyStepHeld()` was unreachable since Perf Page never calls `SetStepHeld()`; block and `fine` parameter removed |

**Deviations from spec (`docs/planned/VOLTSEQ.md`):**
- Slider recording uses the channel Range parameter directly; separate `slider_base_v`/`slider_span_v` fields were removed.
- Armed Gate/Trigger: encoder N edits step N directly (no hold-page-button required). The old hold+page+encoder mechanism was replaced.
- Channel Edit encoder order matches physical panel labels, not EncoderAlts enum order.
- Phase rotate operates only on active steps (0 to length-1); unused steps beyond length are untouched.
- Disarming requires Chan. + Page button (bare page-button disarm removed to allow all steps to be edited while armed, including step 1 of the armed channel).
- Only two WAV builds per release (catseq-catcon, catseq-voltseq); the voltseq-catcon variant is deferred.
- `VoltSeq::Data::current_tag = 5u` (bumped in alpha11; Gate step encoding changed to high byte = gate length, low byte = ratchet count).

**Implementation notes:**

- `src/conf/build_options.hh` — `CATALYST_SECOND_MODE` flag; `CATALYST_MODE_VOLTSEQ = 1`.
- `src/voltseq.hh` — `VoltSeq::Data` (`current_tag = 3u`), `VoltSeq::Interface` (clock engine, playheads, direction state machines, output calculation, orbit engine, trigger/repeat engine).
- `src/ui/voltseq.hh` — `VoltSeq::Main`: all edit modes, GLIDE modifier, step editors, Channel Edit, Global Settings, Performance Page, mode switch detection.
- `src/ui/helper_functions.hh` — `EncoderAccel` struct (velocity-based acceleration; 4×/16× multipliers, active only in CV step editing).
- `src/channelmode.hh` — `Channel::Mode::RawIndex()` and `SetRaw()` accessors for type-selector cycling.
- `src/app.hh` — VoltSeq output path: `MapStepValue`, `CvOutput`, `GateOutput`, `TriggerOutput`.
- `src/params.hh` / `src/ui.hh` / `src/app.hh` — VoltSeq integrated under `CATALYST_SECOND_MODE == CATALYST_MODE_VOLTSEQ`.
- `.github/workflows/release.yml` — two-build release: catseq-catcon and catseq-voltseq WAVs both attached.

---

## Planned

See `TODO.md` for the full backlog with implementation notes.
