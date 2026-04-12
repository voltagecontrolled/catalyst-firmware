# VoltSeq TODO

Backlog for VoltSeq firmware features, known bugs, and porting work.

---

## RC1 Blockers (milestone: catalyst-v1.5.0-rc1)

These are tracked as GitHub issues on voltagecontrolled/catalyst-firmware.

### #23 — Saving should not persist current page
On save/restore, `current_page_` is written to flash. On next boot the display can land "off-screen" relative to the user's sequence. Fix: always restore `current_page_` to 0 on load, regardless of what was saved.

### #12 — Internal reset / global loop sync
No way to re-sync all channels to a common downbeat without an external reset signal. Need a button combo that resets all channels to step 1 simultaneously. Global loop length (all channels wrap together) is a stretch goal. `Shift+Play` is the natural candidate, but see #9 / issue around reset timing.

### #10 — Shift+Play held ≥ 600 ms does not enter Clear mode
Clear mode entry is blocked when Play fires its short-tap reset action before the hold threshold. Fix: make the combo order-independent (both held simultaneously regardless of press order); suppress the short-tap action until button release confirms duration.

### #8 — Docs incorrectly claim last-used mode auto-loads on reboot
Mode does NOT auto-restore on reboot. To set startup mode, user must boot while holding the mode-switch combo. Update wiki and any user-facing docs.

---

## Post-RC / Future

### Reset punch-in timing (#9)
Shift+Play reset during playback is slightly off-beat. Low priority — behavior is usable, not sample-accurate. The `primed=false` approach adds ~1-step latency; `ResetExternal()` from the SHIFT+PLAY handler when playing could eliminate it.

### Gate track follow does nothing (#3)
Track follow assign mode allows assignments from gate tracks to other gate tracks, but no functionality is defined. UI cleanup: either block gate→gate assignments in the UI or implement something useful. No milestone set — post-1.5.0.

### Live encoder recording in normal view
In normal view (unarmed), turning an encoder while NOT holding a page button should live-record a value into the current playing step. Coarse increments (range/32 per detent) unless Fine held. Debounce interaction needs tuning.

### Global channel length / clock div sync
A combo in Global Settings that sets ALL channels to the same length and/or clock division simultaneously. UX TBD (enc hold, Shift+enc combo, or "apply to all" page-button gesture in Channel Edit).

### Channel length — view without editing
Brief passive display on Channel Edit entry before the first enc detent registers as an edit. Possibly just a minimum 2-detent threshold before first change.

### Clock division redesign — multipliers + triplets
Full design in `docs/planned/VOLTSEQ-CLOCK-DIVISIONS.md`.

### Global BPM — 1 BPM steps + snap points
Store BPM as integer; snap to common tempos (60, 80, 90, 100, 120, 130, 140, 150, 160, 180, 200 BPM) with a 2-detent magnetic pull. Update wiki with snap values.

### Presets / scene save-load
See `docs/planned/VOLTSEQ-PRESETS.md`.

### Iterative probability — per-step cycle filter
See `docs/planned/VOLTSEQ-ITERATIVE-PROBABILITY.md`.

---

## Known Quirks / Low Priority

- **GLIDE inaccessible while armed Trigger** — pulse width adjustment requires disarming. Document as intentional or add a Glide-held sub-state to `UpdateArmedTrigger`.
- **Perf Page step-edit block is dead code** — `AnyStepHeld()` is always false in Perf Page; the encoder block never fires. Perf Page is a settings modal, not a step editor — remove the dead block.
- **`current_page_` bleeds between modes** — navigating in Perf Page or Glide Editor affects what Normal mode shows on return. Low-severity; SHIFT+Page recovers. Fix: reset `current_page_` on mode transitions where it matters.
- **Wholetone scale and unquantized CV show the same grey** — color collision, needs a distinct color for wholetone.
- **Phase rotate (Channel Edit enc 4) is destructive with no undo** — consider a "rotate pending" visual before committing.
- **Trigger pulse width (Channel Edit enc 5) has no display feedback** — consider a brightness ramp.

---

## Build System

### VoltSeq requires separate cmake configuration
VoltSeq builds use `-DCATALYST_SECOND_MODE=CATALYST_MODE_VOLTSEQ` and output to `build_voltseq/`. The default `make all` builds CatSeq/CatCon into `build/`.

```bash
cmake -B build_voltseq -GNinja -DCATALYST_SECOND_MODE=CATALYST_MODE_VOLTSEQ .
cmake --build build_voltseq
cmake --build build_voltseq --target f401.wav
```

WAV is at `build_voltseq/f401/f401.wav`.

### Ninja dep staleness on header-only files
After any header edit, verify ninja plans to recompile: `cmake --build build_voltseq -n` should show compilation steps, not `[0/1]`. If it shows `[0/1]`, run `rm -rf build_voltseq` and reconfigure. Alphas 31–45 shipped identical binaries due to this.
