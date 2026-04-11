# VoltSeq TODO

Backlog for VoltSeq firmware features, known bugs, and porting work.

---

## Feature Ideas

### Iterative Probability — Per-Step Cycle Filter
See `docs/planned/VOLTSEQ-ITERATIVE-PROBABILITY.md`.

---

## Active / Next Up

### Step-0 skip on reset — alpha46 testing needed
**Status:** alpha46 removes the fire-and-prime block from `Reset()`, `ResetExternal()`, and `Play()`. All three now rely on the priming path in `OnChannelFired` to fire step 0 on the first clock. **Not yet hardware verified** — alphas 31–45 were never actually compiled due to stale ninja deps (see Build System note below).

**Current approach (alpha46):** `primed=false`, `shadow=0`, `ResetDividers()`. First clock primes without advancing, fires step 0.

**Known trade-off:** Reset-while-playing no longer fires step 0 immediately — there's a gap until the next clock. If this is unacceptable, the PRE_STEP sentinel approach (replace `primed` boolean with a `shadow = 0xFF` pre-position) handles it cleanly. See conversation history for full design.

**If alpha46 doesn't fix it:** The PRE_STEP approach eliminates the `primed` flag entirely, removing the possibility of stale boolean state causing skips.

### Manual reset punch-in under external clock
**Observed (hardware verified):** SHIFT+PLAY cannot be punched in time to resync to an external clock. `Reset()` sets `primed=false` so the first post-reset clock primes rather than advances — adds ~1 step of latency that makes beat-accurate manual reset impractical.

**Impact:** Low priority — not needed for typical use. Would matter for live performance with a band.

**Proposed fix:** Call `ResetExternal()` from the SHIFT+PLAY handler instead of `Reset()` when the sequencer is playing. `Reset()` is still correct for play/stop reset mode (stopping while stopped).

**Caution:** May need a separate `ResetWhilePlaying()` path to avoid firing step 0 gates unintentionally on a deliberate stop.

---

### Step Probability / Random Deviation — Implemented in alpha56 (Needs Hardware Verification)
Per-step probability (`StepFlags::step_prob[64]`, 0–100) and CV deviation amount (`ChannelSettings::random_amount_v`, ±15V). Gesture: SHIFT+Glide in armed mode, enc 0–7. Deviation range: Channel Edit enc 7. Needs testing on hardware for all three channel types at low, mid, and max probability values.

---

### Glide Direction — Possible One-Way Slew Bug
**Observed:** Glide appears to only work from higher pitch to lower pitch (downward slew). Upward slew may not be audible or may be instant.

**Suspected cause:** The exponential slew in `CvOutput` (`app.hh`) uses:
```cpp
const float coef = 1.f / (glide_time * sample_rate + 1.f);
slewed = slew_val[ch] + (quantized - slew_val[ch]) * coef;
```
This is symmetric on paper, but `slew_val[ch]` is always set to `out_cv` after each tick. If there is a condition where `slew_val` is not initialized (e.g. first tick after reset, or after a step change with no prior output), it could snap rather than slew in one direction. Needs testing: program a low→high step transition with glide enabled and confirm slew is audible in both directions.

---

### Beat Repeat Mode — Fixed in alpha18 (Needs Hardware Verification)
Root cause was that orbit-following channels only fired via their own clock dividers, not at the subdivision rate. Fix suppresses normal clock fires for orbit channels during beat repeat and drives them from the orbit advance timer. See `docs/planned/VOLTSEQ-BEAT-REPEAT.md` for the original diagnosis.

---

### Live Encoder Recording in Normal View
**Desired:** In normal view (unarmed, not in any editor), turning an encoder while NOT holding a page button live-records a value into the current playing step for that channel. Encoder movement should be fast — coarse increments relative to the channel's configured Range — unless Fine is held, which switches to fine sub-semitone steps.

**Why a TODO:** The debounce interaction needs careful tuning. At 3kHz, an encoder turn produces a delta of 1 per detent. Applying a large increment per detent (e.g., ÷8 of the full range per step = 8192 units) may overshoot. Also need to decide: record on every tick the encoder moves, or only on positive edges? Probably coarse = range/32 per detent, fine = range/256 per detent. Gate length and Trigger values have narrower ranges so need separate scaling.

---

### Global Channel Length / Clock Div Sync
**Desired:** A combo in Global Settings that sets ALL channels to the same length and/or the same clock division simultaneously. Useful for "syncing up" channels after independent editing, especially when setting up a fresh sequence.

**Proposed UX:** In Global Settings, a dedicated encoder (enc 3 is currently master reset steps — could be a Shift+enc combo or a separate hold gesture) writes a chosen length to all 8 channels. Similarly for clock div. Alternatively, a "apply to all" page-button hold while turning enc 3 in Channel Edit. UX TBD.

---

### Channel Length — View Without Editing
**Desired:** When navigating to Channel Edit and pressing enc 2 (Length) to see the current length, the display should appear without immediately consuming the first encoder detent as an edit. Consider a brief "view mode" window after Channel Edit is entered (or after channel focus) where the length feedback is shown passively before the first encoder turn registers as a change. Similar to a "hold to preview, turn to edit" interaction.

**Note:** Related to the existing 600ms display timeout added in alpha4. May be solvable simply by requiring a small minimum delta (e.g., 2 detents) before the first length edit registers.

---

### Clock Division Redesign — multipliers + triplets
Full design (table, phase accumulator, persistent layout notes) in `docs/planned/VOLTSEQ-CLOCK-DIVISIONS.md`.

---

### Global BPM — 1 BPM increments + reference snap points
**Current state:** BPM is stored as `bpm_in_ticks`; adjusting by enc delta moves in tick units (non-linear, hard to set a deliberate tempo).

**Desired:**
- Encoder 6 (BPM/Clock Div) in Global Settings moves in **1 BPM steps** (convert current BPM to int, ±1, convert back to ticks).
- Add **snap points** at common tempos (e.g., 60, 80, 90, 100, 120, 130, 140, 150, 160, 180, 200 BPM). The snap should act like a slight magnetic pull — encoder must pass through +2 steps within 400 ms to skip past a snap point.
- Display note: wiki should list the snap BPM values so users know where to aim.

---

### Presets / Scene Save-Load
See `docs/planned/VOLTSEQ-PRESETS.md`.

---

## Needs Hardware Verification

These features exist in the firmware but have not been user-tested yet. Each item should be checked before a stable release.

### Alpha-specific regression checks
- **Clear mode** (alpha5): SHIFT+PLAY held 600ms → page N clears channel N, PLAY clears all, any other button exits. Short press still resets.
- **SHIFT+CHAN toggle exits Channel Edit** (alpha6): confirm both entry and exit work, and that the save fires on exit.
- **Play exits armed mode without stopping playback** (alpha6): confirm plain Play while armed only disarms.
- **CHAN+encoder no longer causes clock stumble** (alpha6): test type cycling while sequence is running.
- **Tap tempo requires 3 taps** (alpha5+6, both CatSeq and VoltSeq): confirm first two taps are ignored, third takes effect.
- **Custom scale fallback** (alpha6): channel saved with a custom scale type should load as unquantized without corrupting step data.
- **Global Settings modal entry** (alpha10): SHIFT+CHAN held 2 s alone — any other button during hold must cancel entry. Confirm Shift+Play, and other Shift combos still work normally (they interrupt the hold timer).
- **Global Settings — Play/Stop reset mode** (alpha9): enc 1 toggles on/off; when on, Stop resets all channels to step 1. Confirm enc 1 LED is red when on, off when off.
- **Global Settings — Master reset steps** (alpha9): enc 3 sets 0–64 steps; LED off at 0, orange at non-snap values, red at 8/16/32/64. Confirm all channels reset to step 1 every N master ticks when non-zero.
- **Global Settings — BPM color zones** (alpha10): enc 6 pulse color changes with BPM range (red <50, orange 50–79, yellow 80–99, green 100–119, blue 120–149, teal 150–179, lavender 180+).
- **GetOutputStep wrap fix** (alpha9): short channels repeat correctly on later pages. Test: 8-step trigger on ch 1, 16-step CV on ch 2. Go to page 2, hold a page button — trigger steps should repeat (step 0–7 map to page-2 positions), not be silent.
- **Direction selector clamped** (alpha9): in Channel Edit enc 2, direction should stop at Forward (CW limit) and Random (CCW limit), not wrap.
- **Gate ratchets** (alpha11): Gate step encoding uses high byte = gate length, low byte = ratchet count. Confirm armed Gate + Glide held + enc N sets ratchet count (0 = single gate, 2–8 = N pulses per step). Confirm unarmed Glide + page-button held + enc N also works. Confirm gate ratchet LED display when Glide held (dim grey = no ratchet, bright green = high count).
- **SHIFT+PAGE navigation** (alpha13): confirm page navigation works in normal mode, armed mode (all channel types), and Channel Edit. While Shift held: page buttons should show current page lit (not chaselight); encoder LEDs should show playhead colors with focused channel blinking. After navigating, chaselight should track the new page correctly.
- **Chaselight focus from encoder wiggle** (alpha13): in main mode (unarmed, no step held), wiggling an encoder without holding a page button should update which channel's chaselight is shown on the page buttons. Confirm no steps are edited during this wiggle.
- **Channel Edit page sync** (alpha13): on entering Channel Edit, the page buttons chaselight should be visible immediately (no "chaselight gone" after cross-mode page navigation). Test: go to Performance Page, navigate to page 2, exit, enter Channel Edit — chaselight should show on the correct page for the focused channel.
- **16th-note clock rate** (alpha14): tap 120 BPM → sequence should run at 16th-note speed (8-step pattern completes twice per bar). Confirm BPM color zones still feel correct at musical tempos.
- **Modal blocking** (alpha15): while in Channel Edit, confirm SHIFT+CHAN does nothing and Fine+Glide does nothing. While in Performance Page, confirm SHIFT+CHAN does nothing. While in Global Settings, confirm Fine+Glide does nothing. Confirm Armed → Channel Edit still works (SHIFT+CHAN short tap while a channel is armed).
- **Glide Step Editor removed** (alpha17): confirm Glide + page button (any hold duration) now only does Gate ratchet editing — no editor entry, no 600ms cliff. Confirm CV glide flags still work via armed CV + Glide held + enc N.
- **Beat repeat firing at subdivision rate** (alpha18): enter Performance Page, select blue or cyan mode, commit a zone. Gate/Trigger channels in the follow mask should stutter at the subdivision rate (e.g., zone 4 = triplet 8th = 1.5× beat rate) rather than at the channel's own clock division. Confirm each zone produces audibly distinct repetition rates. Confirm gate duration scales to fit within the subdivision window. Confirm channels excluded from the orbit follow mask continue playing normally.
- **Channel Edit page button focus (alpha19)**: tapping any page button in Channel Edit should immediately focus that channel and show passive length display — no delay, no long-press behavior. Confirm holding a page button for several seconds does NOT clear the channel. Confirm channel clearing still works via SHIFT+PLAY → Clear Mode.

### Core features never formally tested
- **External clock sync**: patch a clock into Clock In; confirm VoltSeq locks to it, Reset jack resets all channels.
- **Direction modes** (Reverse, Ping-Pong, Random): test with multi-page sequences; verify Ping-Pong wraps correctly at page boundaries.
- **Per-channel clock divisions** (Channel Edit enc 6): channels running at different rates than master; confirm independence.
- **Glide time per CV channel** (GLIDE modifier, enc N while Glide held): confirm non-zero glide produces slew on output.
- **Output delay** (Channel Edit enc 1, 0–20 ms): confirm offset is audible/measurable between channels.
- **Random amount** (Channel Edit enc 8): confirm non-zero amount introduces step randomisation.
- **Multi-page patterns**: sequences longer than 8 steps; direction modes crossing page boundaries.
- **Hold multiple page buttons simultaneously**: multi-channel step editing — all held channels should update.
- **Orbit follow mask exclusion** (Performance Page page buttons): confirm a channel with its bit cleared is genuinely unaffected by orbit. Was reported as possibly non-functional; persist fix is in but correctness of exclusion not verified.
- **Granular orbit mode** (green perf mode): only beat repeat was exercised during alpha testing.
- **Phase rotate** (Channel Edit enc 4): destructive operation, confirm it only moves active steps and leaves steps beyond channel length untouched.

---

## Main Sequencer Ports (future)

### Universal "Play/Reset exits any mode"
VoltSeq now exits all modes on Play press. The main sequencer has modal states that don't share this pattern. Port the single-button-exits-everything UX to the main sequencer UI for consistency.

---

## Flash Save Latency

Saving to flash (`SaveMacro` / `SaveShared`) is synchronous and blocks the CPU for the duration of the write — long enough to stall the 3kHz clock and cause an audible glitch. Currently mitigated by deferring saves to moments when the clock is stopped or the user releases a control:

- **CHAN+encoder type change:** deferred to CHAN button release
- **Channel Edit / Glide editor / Ratchet editor exit:** deferred to next play-stop toggle when playing; saves immediately if stopped

**Known limitation:** settings changed while playing (channel type, length, range, etc.) will not persist if the module is power-cycled before the next play-stop press.

**Long-term fix:** async flash write using a background DMA or interrupt-driven page write, so the save does not block the main loop. Alternatively, a dedicated "save on idle" timer that fires when no buttons have been pressed for N seconds.

---

## UX Cleanup

Items identified during the alpha13 mode-state audit (`docs/VOLTSEQ-MODES.md`). Items below are still open — do a full review before changing anything.

---


### GLIDE modifier inaccessible while armed Trigger

When a Trigger channel is armed, the GLIDE modifier block is skipped (`morph held && !armed`). `UpdateArmedTrigger` has no Glide handling. Result: pulse width adjustment (Glide + enc N for Trigger channels) is inaccessible while armed. The user must disarm to adjust pulse width.

Options: (a) add a Glide-held sub-state to `UpdateArmedTrigger` matching the unarmed GLIDE modifier behavior for Trigger; (b) document as intentional and add a note to the wiki.

---

### Step editing in Performance Page is unreachable

`UpdatePerfPage` has a `ForEachEncoderInc` block guarded by `p.AnyStepHeld()`. But `AnyStepHeld()` tracks logical holds set by `p.SetStepHeld()`, which is never called in Perf Page — page button presses only toggle the orbit follow mask. `AnyStepHeld()` is always false in this context. The encoder step-edit block is dead code.

The wiki documents "Hold Page + turn Encoder" as working in Perf Page. Either fix it (call `SetStepHeld` on page button press before the mask toggle) or remove the dead block and update the wiki.

---


### current_page_ bleeds between modes

`current_page_` is a single shared value. Navigating to page 3 in any mode leaves the display on page 3 when entering the next mode. Channel Edit entry now resets it to the focused channel's playhead page (alpha13), but no other mode does. Navigating in Glide Step Editor or Perf Page affects what Normal mode shows on return.

Low-severity in practice (SHIFT+PAGE always lets you recover), but worth being intentional about which mode transitions should and shouldn't reset the page.

---

## Build System

### VoltSeq requires separate cmake configuration
VoltSeq builds use `-DCATALYST_SECOND_MODE=1` and output to `build_voltseq/`. The default `make all` builds CatSeq/CatCon into `build/`. Always use:
```bash
cmake -B build_voltseq -GNinja -DCATALYST_SECOND_MODE=1 .
cmake --build build_voltseq
cmake --build build_voltseq --target f401.wav
```
WAV is at `build_voltseq/f401/f401.wav`.

### Ninja dep staleness on header-only files
Ninja's dependency scanner can lose track of header-only files (e.g. `voltseq.hh`, `ui/voltseq.hh`, `palette.hh`). Symptom: `ninja -n` shows `[0/1]` after editing headers. **Alphas 31–45 shipped identical binaries** because of this. Fix: `rm -rf build_voltseq && cmake -B build_voltseq ...` to rebuild the dep graph from scratch. After any header edit, verify ninja plans to recompile before trusting the WAV.

---

## Known Quirks / Low Priority

- **Wholetone scale and unquantized CV show the same grey** — both map to `Palette::grey` in the type selector. Low-priority color collision; needs a distinct color for wholetone.
- **Glide Step Editor page navigation requires Shift+Page.** Bare page presses are blocked to avoid making step 1 of channel 0 unreachable (page button 0 = step 1 of the channel). Consider whether bare page presses could be allowed since exit is Glide or Play only.
- **Phase rotate (Channel Edit enc 4) is destructive with no undo.** Add a "rotate pending" visual before committing, or document clearly.
- **Trigger pulse width (Channel Edit enc 5) has no display feedback.** Consider a brightness ramp on enc 5 LED (dim = short pulse, bright = long pulse).
