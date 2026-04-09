# VoltSeq TODO

Backlog for VoltSeq firmware features, known bugs, and porting work.

---

## Feature Ideas

### Iterative Probability — Per-Step Cycle Filter
See `docs/planned/VOLTSEQ-ITERATIVE-PROBABILITY.md`.

---

## Active / Next Up

### Step Probability / Random Amount — Needs Investigation
**Observed:** Random amount (Channel Edit enc 8) has not been confirmed working. Test on CV, Gate, and Trigger channels at various amounts and confirm steps are being skipped/randomized as expected.

**Where to look:** `src/voltseq.hh` — search for `random_amount` to find where it is applied in the engine. Verify the randomization fires on step advance, not on every tick, and that 0% = no randomization, 100% = fully random on every step.

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

### Beat Repeat Mode is Non-Functional (Blue/Cyan modes in Performance Page)
Root cause diagnosed and design options documented. See `docs/planned/VOLTSEQ-BEAT-REPEAT.md`.

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
- **Global Settings — Master reset steps** (alpha9): enc 3 sets 0–64 steps; LED off at 0, orange at non-snap values, red at 8/16/32/64. Confirm all channels reset to step 1 every N master ticks when non-zero. Confirm overrides reset leader.
- **Global Settings — Reset leader** (alpha9): page buttons radio-select leader channel; tap lit button to deselect. Confirm leader channel wrap resets all other channels. Confirm no reset when master_reset_steps is non-zero (master takes priority).
- **Global Settings — BPM color zones** (alpha10): enc 6 pulse color changes with BPM range (red <50, orange 50–79, yellow 80–99, green 100–119, blue 120–149, teal 150–179, lavender 180+).
- **GetOutputStep wrap fix** (alpha9): short channels repeat correctly on later pages. Test: 8-step trigger on ch 1, 16-step CV on ch 2. Go to page 2, hold a page button — trigger steps should repeat (step 0–7 map to page-2 positions), not be silent.
- **Direction selector clamped** (alpha9): in Channel Edit enc 2, direction should stop at Forward (CW limit) and Random (CCW limit), not wrap.
- **Gate ratchets** (alpha11): Gate step encoding uses high byte = gate length, low byte = ratchet count. Confirm armed Gate + Glide held + enc N sets ratchet count (0 = single gate, 2–8 = N pulses per step). Confirm unarmed Glide + page-button held + enc N also works. Confirm gate ratchet LED display when Glide held (dim grey = no ratchet, bright green = high count).
- **SHIFT+PAGE navigation** (alpha13): confirm page navigation works in normal mode, armed mode (all channel types), and Channel Edit. While Shift held: page buttons should show current page lit (not chaselight); encoder LEDs should show playhead colors with focused channel blinking. After navigating, chaselight should track the new page correctly.
- **Chaselight focus from encoder wiggle** (alpha13): in main mode (unarmed, no step held), wiggling an encoder without holding a page button should update which channel's chaselight is shown on the page buttons. Confirm no steps are edited during this wiggle.
- **Channel Edit page sync** (alpha13): on entering Channel Edit, the page buttons chaselight should be visible immediately (no "chaselight gone" after cross-mode page navigation). Test: go to Performance Page, navigate to page 2, exit, enter Channel Edit — chaselight should show on the correct page for the focused channel.

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

Items identified during the alpha13 mode-state audit (`docs/VOLTSEQ-MODES.md`). All are candidates for cleanup but none have been acted on — do a full review before changing anything.

### Modal entry timers fire in any active mode

The SHIFT+CHAN hold timer (`shift_chan_hold_pending_`) and the Fine+Glide hold timer (`scrub_hold_pending_` in `Common()`) both run unconditionally regardless of which mode is currently active. This means:

- SHIFT+CHAN held 2s while in Perf Page → enters Global Settings (Perf Page dormant in background)
- SHIFT+CHAN short tap while in Glide Step Editor → Channel Edit and Glide Editor both flagged active simultaneously; Channel Edit wins by priority; Glide Editor resumes on Channel Edit exit
- Fine+Glide 1.5s while in Channel Edit → enters Perf Page; Channel Edit dormant in background, resumes on Perf Page exit

Decide: should these entry combos be blocked while any other modal is active, or is the "stack and resume" behavior acceptable/desirable?

---

### Glide Step Editor — consider removing

The Glide Step Editor (entered via Glide + long-press page button 600ms) provides:
- CV channels: per-step glide flag editing
- Gate channels: gate step length editing

Both are also accessible without this editor:
- Per-step CV glide flags: armed CV + Glide held + enc N
- Gate step lengths: armed Gate + enc N

The editor's only ergonomic advantage is persistence (no button to hold). Its cost: it creates a 600ms timing cliff in the GLIDE modifier where holding a page button too long switches from Gate ratchet editing (the useful path) to editor entry. Removing it would make the short Glide+page-button gesture unambiguous. The `glide_longpress_timer_` and related state could also be simplified.

If removed, the stale-timer collision (mode map collision #10) goes away with it.

---

### GLIDE modifier inaccessible while armed Trigger

When a Trigger channel is armed, the GLIDE modifier block is skipped (`morph held && !armed`). `UpdateArmedTrigger` has no Glide handling. Result: pulse width adjustment (Glide + enc N for Trigger channels) is inaccessible while armed. The user must disarm to adjust pulse width.

Options: (a) add a Glide-held sub-state to `UpdateArmedTrigger` matching the unarmed GLIDE modifier behavior for Trigger; (b) document as intentional and add a note to the wiki.

---

### Step editing in Performance Page is unreachable

`UpdatePerfPage` has a `ForEachEncoderInc` block guarded by `p.AnyStepHeld()`. But `AnyStepHeld()` tracks logical holds set by `p.SetStepHeld()`, which is never called in Perf Page — page button presses only toggle the orbit follow mask. `AnyStepHeld()` is always false in this context. The encoder step-edit block is dead code.

The wiki documents "Hold Page + turn Encoder" as working in Perf Page. Either fix it (call `SetStepHeld` on page button press before the mask toggle) or remove the dead block and update the wiki.

---

### Channel Edit long-press clear is easy to trigger accidentally

600ms is short — pressing a page button to focus a channel and pausing before releasing can clear all 64 steps with no undo. The 3-flash blinker fires after the clear, not before.

Options: (a) increase the threshold (e.g., 1000–1500ms); (b) require a second confirmation gesture (turn an encoder, or press again); (c) add a "pending clear" visual warning during the hold window so the user knows what's about to happen.

---

### current_page_ bleeds between modes

`current_page_` is a single shared value. Navigating to page 3 in any mode leaves the display on page 3 when entering the next mode. Channel Edit entry now resets it to the focused channel's playhead page (alpha13), but no other mode does. Navigating in Glide Step Editor or Perf Page affects what Normal mode shows on return.

Low-severity in practice (SHIFT+PAGE always lets you recover), but worth being intentional about which mode transitions should and shouldn't reset the page.

---

## Known Quirks / Low Priority

- **Wholetone scale and unquantized CV show the same grey** — both map to `Palette::grey` in the type selector. Low-priority color collision; needs a distinct color for wholetone.
- **Glide Step Editor page navigation requires Shift+Page.** Bare page presses are blocked to avoid making step 1 of channel 0 unreachable (page button 0 = step 1 of the channel). Consider whether bare page presses could be allowed since exit is Glide or Play only.
- **Phase rotate (Channel Edit enc 4) is destructive with no undo.** Add a "rotate pending" visual before committing, or document clearly.
- **Trigger pulse width (Channel Edit enc 5) has no display feedback.** Consider a brightness ramp on enc 5 LED (dim = short pulse, bright = long pulse).
