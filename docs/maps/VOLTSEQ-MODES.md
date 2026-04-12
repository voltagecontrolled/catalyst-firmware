# Catalyst VoltSeq — Mode Map

Reference for the Catalyst VoltSeq UI state machine. Intended for UX review and bug hunting — maps every modal state to its state variables, entry/exit paths, button handling, and LED display, then lists known collision points and edge cases.

All code is in `src/ui/voltseq.hh`. Line numbers are approximate and may drift; search by function name.

---

## State Variables

```
armed_ch_              optional<uint8_t>   nullopt = unarmed; 0–7 = armed channel
focused_ch_            uint8_t             channel whose playhead the chaselight tracks (normal mode)
current_page_          uint8_t             page currently being viewed/edited (0–7, shared across modes)
edit_ch_               uint8_t             focused channel in Channel Edit (persists across entries)

channel_edit_active_   bool
channel_edit_last_enc_ uint8_t             last encoder turned in Channel Edit (0xFF = none)
length_display_until_  uint32_t            tick deadline for passive length display in Channel Edit

perf_page_active_      bool
perf_settings_active_  bool                sub-mode within perf page

phase_locked_          bool                Phase Scrub Lock active
picking_up_            bool                slider hasn't reached locked position yet
locked_raw_            uint16_t            ADC value when lock was set
orbit_pickup_          bool                beat-repeat entry pickup (slider must move away from snap point)
orbit_pickup_slider_   uint16_t
scrub_hold_pending_    bool                Fine+Glide hold timer running
scrub_hold_start_      uint32_t

global_settings_active_ bool
shift_chan_hold_pending_ bool               SHIFT+CHAN hold timer running
shift_chan_hold_start_  uint32_t

clear_mode_active_          bool
clear_mode_fine_suppressed_ bool            Fine button still held from entry combo — suppress until released
clear_mode_play_suppressed_ bool            Play button still held from entry combo — suppress until released
fine_play_pending_          bool            FINE+PLAY hold timer running
fine_play_press_t_          uint32_t

trigger_step_enc_turned_ uint8_t           bitmask: which steps had encoder turned while held (armed Trigger)
last_touched_step_     uint8_t             last page button pressed (0–7, in-page index)
record_slider_prev_    uint16_t            previous slider ADC (motion detection)
record_active_until_   uint32_t            slider recording active until this tick
```

---

## Update() Dispatch Priority

`Update()` runs a priority chain for modal states, but **Clear Mode is an overlay** — it calls `UpdateClearMode()` and then falls through to normal processing. All other modals use early return.

```
Priority  Condition                        Handler
──────────────────────────────────────────────────────────────────────
  1       p.shared.mode == Sequencer        → SwitchUiMode, return
  2       clear_mode_active_                → UpdateClearMode() [NO return — falls through]
  3       global_settings_active_           → UpdateGlobalSettings(), return
  4       perf_page_active_                 → UpdatePerfPage(), return
  5       channel_edit_active_              → UpdateChannelEdit(), return
  6       morph held && !armed              → UpdateGlideModifier(), return
  7       chan held                         → arm/disarm / type change, return
  8       armed_ch_.has_value()             → UpdateArmed() (no return guard; falls through)
  9       (else)                            → UpdateNormal()
──────────────────────────────────────────────────────────────────────
```

**Pre-priority guards** (run at the very top of Update() before the chain):

- `shift_chan_hold_pending_` — SHIFT+CHAN hold timer; fires Channel Edit entry or Global Settings entry. Guards against all active modals.
- `play_jgh` block — Play button rising edge is routed before the chain:
  - Clear Mode active → exit clear mode (no playback change)
  - Global Settings active → exit Global Settings
  - Shift held → `p.Reset()` (immediate, any press order)
  - Perf Page active → exit Perf Page (save shared)
  - Channel Edit active → exit Channel Edit
  - Armed → disarm (no playback change)
  - Fine held && `!fine_play_pending_` && `!clear_mode_active_` → arm `fine_play_pending_`
  - Else → Toggle play/stop
- `fine_play_pending_` poll — FINE+PLAY hold timer; enters Clear Mode on threshold.

---

## Main Mode (Normal — unarmed, no modal)

**Main Mode** is the canonical starting point for the entire UI. All modal states (Channel Edit, Global Settings, Performance Page, Clear Mode) can only be entered from Main Mode, with one exception: Channel Edit can also be entered while Armed. Modals do not stack — entering one while another is active is silently blocked.

**State:** `armed_ch_ == nullopt`, all modal flags false, morph/chan not held.

**Entry:** Default state; also reached by disarming or exiting any modal via Play/Reset.

**Exit:** From Main Mode: pressing a Page button (→ step held sub-state), holding CHAN (→ Armed), SHIFT+CHAN short tap (→ Channel Edit), SHIFT+CHAN hold 1s (→ Global Settings), Glide held (→ GLIDE Modifier), Fine+Glide 1.5s (→ Performance Page), Fine+Play held ~500ms (→ Clear Mode).

### Sub-state: Shift held

| Input | Action |
|-------|--------|
| Page button N (rising edge) | NavigatePage(N) — no step is set held |
| Encoder N (any turn) | focused_ch_ = N (no step edited) |
| Any falling edge on scene buttons | SetStepHeld(false) — releases any steps held before Shift |

### Sub-state: Step held (one or more page buttons held, Shift not held)

| Input | Action |
|-------|--------|
| Page button N (rising edge) | last_touched_step_ = N; SetStepHeld(GlobalStep(N), true) |
| Page button N (falling edge) | SetStepHeld(GlobalStep(N), false) |
| Encoder N (any turn) | focused_ch_ = N; EditCvStep / EditGateStep / EditTrigStep for all held steps |

### Sub-state: Idle (no step held, no shift)

| Input | Action |
|-------|--------|
| Encoder N (any turn) | focused_ch_ = N; encoder events drained (no edit) |

**LED — Page buttons:** Chaselight for `focused_ch_`. Lit button = `playhead(focused_ch_) is on current_page_ at in-page position i`. Also lit for any currently-held page button.

**LED — Page buttons, Shift held:** `i == current_page_` only.

**LED — Encoders (step held):** StepColorForChannel(ch, held_step) for all 8 channels.

**LED — Encoders (Shift held, no step held):** StepColorForChannel(ch, playhead(ch)) for all channels; `focused_ch_` encoder blinks ChaseWhite.

**LED — Encoders (idle):** StepColorForChannel(ch, playhead(ch)) for all channels.

---

## Mode: Armed

**State:** `armed_ch_.has_value()`.

**Entry:** From Main Mode: CHAN held + Page button N. `armed_ch_ = N`, `focused_ch_ = N`.

**Exit:**
- CHAN held + Page button N again → `armed_ch_ = nullopt`
- Play/Reset → `armed_ch_ = nullopt` (no playback change)

**LED — Page buttons (Gate/Trigger armed):** x0x pattern — lit = step is on. Not suppressed by Shift.

**LED — Page buttons (CV armed or shift held for Gate/Trigger):** Chaselight (same as normal mode); Shift held → current page indicator.

### Sub-mode: Armed CV

**Entry:** `armed_ch_` set to a CV-type channel.

| Modifier | Input | Action |
|----------|-------|--------|
| None | Encoder N | EditCvStep(ch, page_step(N), acc) |
| Glide held | Encoder N | SetGlideFlag(ch, page_step(N), dir>0) — CW=on, CCW=off |
| Shift+Glide held | Encoder N | step_prob[page_step(N)] += dir, clamped 0–100 |
| Shift held | Page button N | NavigatePage(N) |
| Shift held | Encoder 4 | cs.range.Inc(dir) — voltage span |
| Shift held | Encoder 6 | cs.transpose += dir — floor voltage |

Slider recording (motion-gated, via Common()) runs independently for CV channels.

**LED — Encoders (no modifier):** StepColorForChannel(ch, page_step(i)) for all 8 steps on current page; playing step blinks ChaseWhite (suppressed while that page button is held).

**LED — Encoders (Glide held):** GlideFlagColor: full_white = glide on, dim_grey = off.

**LED — Encoders (Shift+Glide held):** StepProbColor(step_prob[gs]) — violet→grey→white ramp.

**LED — Encoders (Shift held):** enc 4 = range color, enc 6 = transpose color, all others dark.

### Sub-mode: Armed Gate

| Modifier | Input | Action |
|----------|-------|--------|
| None | Page button N (tap) | ToggleGateStep(ch, page_step(N)); last_touched_step_ = N |
| None | Encoder N | EditGateStep(ch, page_step(N), inc, fine) |
| Glide held | Encoder N | EditGateRatchet(ch, page_step(N), dir) |
| Shift+Glide held | Encoder N | step_prob[page_step(N)] += dir, clamped 0–100 |
| Shift held | Page button N | NavigatePage(N) |
| Shift held | Encoder (any) | events drained (no edit) |

> Note: Shift+Glide check comes first in code; then Glide-only; then Shift-only.

**LED — Encoders (no modifier):** GateStepColor(ch, page_step(i)); playing step blinks ChaseWhite.

**LED — Encoders (Glide held):** GateRatchetColor(ch, page_step(i)) — dim grey = no ratchet, bright green = high count.

**LED — Encoders (Shift+Glide held):** StepProbColor(step_prob[gs]) — violet→grey→white ramp.

### Sub-mode: Armed Trigger

| Modifier | Input | Action |
|----------|-------|--------|
| None | Page button N (tap) | ToggleTrigStep(ch, page_step(N)); last_touched_step_ = N |
| None | Encoder N | EditTrigStep(ch, page_step(N), inc) |
| Shift+Glide held | Encoder N | step_prob[page_step(N)] += dir, clamped 0–100 |
| Shift held | Page button N | NavigatePage(N) |
| Shift held | Encoder (any) | events drained (no edit) |
| Glide held (no Shift) | (no handling) | silently ignored — see Known Issues |

**LED — Encoders:** TrigStepColor(ch, page_step(i)); playing step blinks ChaseWhite (faster blink during repeat).

**LED — Encoders (Shift+Glide held):** StepProbColor(step_prob[gs]) — violet→grey→white ramp.

---

## Mode: GLIDE Modifier

**State:** `c.button.morph.is_high() && !armed_ch_.has_value()`.

This is not a persistent mode — it is active only while Glide is physically held. No state variable; priority 6 in Update() chain.

**Entry:** From Main Mode: hold Glide (unarmed). Shift state is passed as parameter.

**Exit:** Release Glide.

### No page button held, no shift

| Input | Action |
|-------|--------|
| Encoder N (CV channel) | cs.glide_time += dir × 0.1s, clamped 0–10s |
| Encoder N (Gate channel) | OffsetAllGateSteps(N, dir) — skips off steps |
| Encoder N (Trigger channel) | cs.pulse_width_ms += dir, clamped 1–100 |

### No page button held, Shift held

| Input | Action |
|-------|--------|
| Encoder N (Trigger channel) | OffsetAllTrigSteps(N, dir) — skips rest steps |

### Page button held

| Input | Action |
|-------|--------|
| Encoder N (Gate channel) | EditGateRatchet(N, GlobalStep(page_btn), dir) |

> Only the first held page button (lowest index) is used. Non-Gate channels are unaffected.

**LED:** No dedicated display — uses whatever the current background mode shows.

---

## Mode: Channel Edit

**State:** `channel_edit_active_ = true`.

**Entry:** From Main Mode or Armed Mode: SHIFT+CHAN short tap (release before 1s). On entry:
- `channel_edit_last_enc_ = 2` (pre-selects Length for passive display)
- `length_display_until_ = now + 600ms`
- `current_page_` reset to `playhead(edit_ch_) / 8` (syncs page to focused channel)

**Exit:**
- SHIFT+CHAN short tap → `channel_edit_active_ = false`
- Play/Reset → handled at top of Update(); `channel_edit_active_ = false`

**Focused channel:** `edit_ch_` (persists across entries). Changed by tapping a page button.

### Sub-state: Shift held

| Input | Action |
|-------|--------|
| Page button N | NavigatePage(N) |
| Encoder (any) | events not consumed (no action taken) |

All other processing is skipped while Shift is held.

### Sub-state: Page button press (tap = focus)

Rising edge: `edit_ch_ = btn`; passive length display for 600ms.

### Sub-state: Encoder editing (normal)

All 8 encoders control the focused channel (`edit_ch_`). Turning any encoder sets `channel_edit_last_enc_` and cancels the length display timeout (or restarts it if enc 2 is turned).

Encoders are 0-indexed here (0 = panel Enc 1).

| Encoder | Panel label | Parameter |
|---------|-------------|-----------|
| 0 | Start | output_delay_ms, 0–20 |
| 1 | Dir. | direction (Forward/Reverse/PingPong/Random, clamped) |
| 2 | Length | length 1–64; also sets current_page_ = (length-1)/8; restarts 600ms display |
| 3 | Phase | RotateChannel (destructive) |
| 4 | Range | CV: voltage span — index into {1,2,3,4,5,10,15}V; clamps Transpose on change; inactive for Gate/Trigger |
| 5 | BPM/Clock Div | clock division |
| 6 | Transpose | CV: floor voltage ±1V, clamped to [−5, 10−span]; inactive for Gate/Trigger |
| 7 | Random | random_amount_v: signed int8, ±1V per detent, −15..+15; CV only (inactive for Gate/Trigger) |

### LED — Length display (active when `TimeNow() <= length_display_until_`)

- **Page buttons:** lit for pages 0..(last_page). last_page = (length-1)/8.
- **Encoders:** type color for positions 0..(steps_on_last_page-1); dim_grey beyond.

### LED — Normal display

- **Page buttons:** chaselight for focused channel. Shift held: `i == current_page_` only.
- **Encoder 0:** white brightness = output_delay_ms / 20.
- **Encoder 1:** direction color (green/orange/yellow/magenta).
- **Encoder 4:** range color; off for Gate/Trigger.
- **Encoder 6:** transpose color (floor voltage); off for Gate/Trigger.
- **Encoder 7:** RandomAmountColor(random_amount_v) — grey→blue→white (unipolar +N); salmon→red→white (bipolar −N); off at 0; off for Gate/Trigger.
- **Others:** dim_grey.

---

## Mode: Performance Page

**State:** `perf_page_active_ = true`.

**Entry:** From Main Mode only: Fine+Glide held 1.5s. Entry confirmed by page-button blink flash. `perf_settings_active_` is also set true on initial entry (settings shown first).

**Exit:** Play/Reset at top of Update(); saves shared data.

### Sub-mode: Perf Settings

**State:** `perf_settings_active_ = true`.

**Entry:** Automatic on initial Perf Page entry. Re-entry: Fine+Glide 1.5s while already in Perf Page.

**Exit:** Play/Reset exits the whole Perf Page (handled at top of Update() — the `play_jgh` event is consumed before `UpdatePerfSettings` runs). Fine+Glide short tap triggers `DoLockToggle()` in `Common()` and does **not** exit Perf Settings.

| Input | Action |
|-------|--------|
| Encoder 0 | quantized_scrub toggle |
| Encoder 1 | slider_perf_mode 0–3 (clamped); clears beat-repeat state |
| Encoder 2 | orbit_width (granular) or beat_repeat_debounce_idx (beat-repeat) |
| Encoder 3 | orbit_direction 0–3 (granular only) |
| Encoder 7 | DoLockToggle() |
| Page button N | scrub_ignore_mask ^= (1<<N) |

### Sub-mode: Perf Page (normal)

| Modifier | Input | Action |
|----------|-------|--------|
| None | Page button N | scrub_ignore_mask ^= (1<<N) |
| Shift held | Page button N | NavigatePage(N) |
| Shift held (beat-repeat) | (held) | freezes orbit_center and beat-repeat zone commitment |

**LED — Page buttons:** scrub_ignore_mask bits — lit = channel follows orbit.

**LED — Encoders (Perf Settings):**
- 0: quantize (orange=on, off=off)
- 1: perf mode color (off/green/blue/cyan)
- 2: orbit width (orange brightness) or debounce (white brightness)
- 3: orbit direction color (granular only)
- 7: lock state (red=locked, off=unlocked)

**LED — Encoders (Perf Page normal):** StepColorForChannel(ch, output_step(ch)) for all channels. Enc 7: pulses red when locked or picking up.

---

## Mode: Global Settings

**State:** `global_settings_active_ = true`.

**Entry:** From Main Mode only: SHIFT+CHAN held 1s (kGlobalSettingsHoldTicks). `c.button.scene.clear_events()` called on entry.

**Exit:** Play/Reset (top of Update()).

Encoders are 0-indexed (0 = panel Enc 1).

| Encoder | Panel label | Parameter |
|---------|-------------|-----------|
| 0 | Start | play_stop_reset toggle |
| 2 | Length | master_loop_length 0–64 |
| 5 | BPM/Clock Div | BPM increment (fine=1 BPM, normal=10 BPM) |

**LED — Encoders:**
- 0: red = play/stop reset on, off = off
- 2: off = disabled, orange = active (non-bar value), red = bar-aligned (8/16/32/48/64)
- 5: BPM color zone pulse
- others: dim_grey

---

## Mode: Clear Mode (Overlay)

**State:** `clear_mode_active_ = true`.

Clear Mode is an **overlay**, not an early-return modal. `UpdateClearMode()` runs and then Update() falls through to normal processing — Play, encoders, CHAN, and all other inputs continue to work normally while Clear Mode is active.

**Entry:** From Main Mode only: FINE+PLAY held ~500ms (kClearHoldTicks). On entry:
- `clear_mode_fine_suppressed_ = true`, `clear_mode_play_suppressed_ = true`
- `c.button.scene.clear_events()` and `c.button.play.clear_events()` called

Suppression flags release as soon as the respective button goes low after entry — prevents the held buttons from immediately triggering a clear or exit.

**Exit:** Play/Reset (handled in `play_jgh` block at top of Update(), before the priority chain). `clear_mode_active_ = false`. No channels affected on exit.

**In UpdateClearMode():**

| Input | Action |
|-------|--------|
| Page button N (rising edge) | ClearChannelSteps(N) — steps + flags; stay in mode |
| Shift + Page button N (rising edge) | FullResetChannel(N) — steps + flags + ChannelSettings{}; stay in mode |

All other inputs (Play, encoders, CHAN, etc.) fall through to normal Update() handling.

**LED — Page buttons:** blink ~3Hz (`(TimeNow()>>10)&1`), overriding normal chaselight display.

**LED — Encoders and Play LED:** normal display — unaffected by Clear Mode.

---

## FINE+PLAY Hold State (not a mode — timer)

`fine_play_pending_` is a pending state, not a priority-chain mode. It runs as a parallel timer check in Update() regardless of other modes.

Arms from the `play_jgh` block when: Fine is held, `!fine_play_pending_`, `!clear_mode_active_`.

| Event | Action |
|-------|--------|
| Fine or Play released before threshold | `fine_play_pending_ = false` (level-checked, not edge) |
| Both held for ~500ms | `clear_mode_active_ = true`; set suppression flags; clear button events |

---

## SHIFT+CHAN Hold State (not a mode — timer)

`shift_chan_hold_pending_` is a parallel timer running at the top of Update().

**Arms:** SHIFT+CHAN rising edge, `!any_modal`, `!shift_chan_hold_pending_`.

| Event | Action |
|-------|--------|
| CHAN released (or Shift released) before 1s | Toggle `channel_edit_active_`; if entering: init entry state + reset current_page_ |
| Held 1s | `global_settings_active_ = true`; clear scene button events |

> Timer only arms when no modal is active. Armed state is not a modal, so Armed → Channel Edit still works.

---

## Phase Scrub Lock (persistent, not a mode)

Managed in `Common()` via `scrub_hold_pending_` / `phase_locked_` / `picking_up_`. Runs unconditionally every tick regardless of mode.

**Lock toggle:** Fine+Glide tapped and released (either button released before 1.5s) → `DoLockToggle()`.

**Perf Page entry:** Fine+Glide held ≥1.5s → `perf_page_active_ = true`. Blocked if Channel Edit, Global Settings, or Clear Mode is active.

These are mutually exclusive by the release detection in Common().

---

## Slider Recording (persistent, not a mode)

Managed in `Common()`. Active when:
- `armed_ch_.has_value()` and channel type is CV
- `!perf_page_active_`
- Slider is in motion (motion-gated: `record_active_until_` deadline)

While playing: writes to shadow step (current playing position). While stopped: writes to `GlobalStep(last_touched_step_)`.

---

## Known Collisions and Edge Cases

### 1. ~~SHIFT+CHAN hold fires in any active mode~~ — Resolved

`shift_chan_hold_pending_` now guards against all modal flags. The timer only arms from Main Mode or Armed Mode.

### 2. ~~Fine+Glide hold fires in any active mode~~ — Resolved

`scrub_hold_pending_` guards against `clear_mode_active_ || global_settings_active_ || channel_edit_active_`. Perf Page is not blocked (to allow re-entry of Perf Settings from within Perf Page).

### 3. Glide ignored when armed Trigger

`UpdateArmedTrigger` has no Glide-only handling. While armed Trigger + Glide held (without Shift):
- `UpdateGlideModifier` is skipped (only runs when `!armed_ch_`)
- Pulse width and offset-all-steps are inaccessible
- Encoders still edit ratchet/repeat counts as normal

Shift+Glide still works in armed Trigger for probability editing.

### 4. Shift+Glide in armed mode — probability editing

All three armed handlers check `shift && morph` first, before individual Shift-only and Glide-only checks. While both are held:
- Enc 0–7 adjust `step_prob[step]` for the armed channel (0–100)
- Encoder LEDs show `StepProbColor(step_prob[gs])` — violet→grey→white ramp
- Individual Shift-only and Glide-only paths are unreachable while both are held

### 5. ~~Dead step-edit block in UpdatePerfPage~~ — Resolved

The unreachable `ForEachEncoderInc` block (guarded by `p.AnyStepHeld()`, always false in Perf Page) has been removed. Step editing via hold-page+encoder is not supported in Perf Page.

### 6. current_page_ is shared state across all modes

`current_page_` is a single value used as context by every mode. Navigation in one mode carries over to the next. The only mode that resets it on entry is Channel Edit (resets to the focused channel's playhead page).

### 7. Clear Mode does not clear armed state

FINE+PLAY while armed arms `fine_play_pending_`, which can enter Clear Mode. `armed_ch_` is not cleared. After Clear Mode exits, armed mode resumes. Functionally harmless.

### 8. ~~Channel Edit long-press clear vs. page focus race~~ — Resolved

Page button in Channel Edit is tap-to-focus only. Channel clearing is exclusively via FINE+PLAY → Clear Mode.

---

## Save Behavior Summary

Catalyst VoltSeq uses **intentional saving** — no data is written to flash automatically except where noted.

| Event | Save target | Notes |
|-------|-------------|-------|
| Glide+Chan gesture (~800ms) | do_save_macro | Steps, channel settings, global settings — explicit save only |
| Perf Page exit (Play/Reset) | do_save_shared | Orbit settings, scrub mode, lock state |
| Phase Scrub Lock toggle | do_save_shared | Immediate — lock state persists across power cycles |
| Orbit follow mask toggle | do_save_shared | Immediate |

All other state changes (step edits, channel type, length, BPM, etc.) are only persisted via the Glide+Chan gesture. Power-cycling without saving discards all unsaved changes.
