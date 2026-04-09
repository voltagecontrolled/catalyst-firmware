# VoltSeq Mode Map

Reference for the VoltSeq UI state machine. Intended for UX review and bug hunting — maps every modal state to its state variables, entry/exit paths, button handling, and LED display, then lists known collision points and edge cases.

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
channel_edit_clear_btn_ uint8_t            page button held for long-press clear (kNoLongpressBtn = none)

glide_editor_active_   bool
glide_editor_ch_       uint8_t             channel being edited in Glide Step Editor

glide_longpress_btn_   uint8_t             page button tracked in GLIDE modifier (kNoLongpressBtn = none)
glide_longpress_shift_ bool                shift state when the page button was pressed
glide_longpress_timer_ Clock::Timer        fires at 600 ms

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
shift_play_pending_    bool                SHIFT+PLAY hold timer running
shift_play_press_t_    uint32_t
clear_mode_active_     bool

trigger_step_enc_turned_ uint8_t           bitmask: which steps had encoder turned while held (armed Trigger)
last_touched_step_     uint8_t             last page button pressed (0–7, in-page index)
record_slider_prev_    uint16_t            previous slider ADC (motion detection)
record_active_until_   uint32_t            slider recording active until this tick
```

---

## Update() Dispatch Priority

`Update()` is a priority chain: each block returns early if its condition is true. Lower entries never run while a higher entry is active.

```
Priority  Condition                        Handler
──────────────────────────────────────────────────────────────────────
  1       p.shared.mode == Sequencer        → SwitchUiMode, return
  2       clear_mode_active_                → UpdateClearMode(), return
  3       global_settings_active_           → UpdateGlobalSettings(), return
  4       perf_page_active_                 → UpdatePerfPage(), return
  5       channel_edit_active_              → UpdateChannelEdit(), return
  6       glide_editor_active_              → UpdateGlideEditor(), return
  7       morph held && !armed              → UpdateGlideModifier(), return
  8       chan held                         → arm/disarm / type change, return
  9       armed_ch_.has_value()             → UpdateArmed() (no return guard; falls through)
 10       (else)                            → UpdateNormal()
──────────────────────────────────────────────────────────────────────
```

**Pre-priority guards** (run at the very top of Update() before the chain):

- `shift_chan_hold_pending_` — SHIFT+CHAN hold timer; fires Channel Edit entry or Global Settings entry independent of which mode is active (only guards against re-entry while `global_settings_active_`).
- `play_jgh` block — Play button rising edge is routed before the chain:
  - Global Settings active → exit Global Settings (save)
  - Shift held → arm shift_play_pending_ (for Reset / Clear mode)
  - Perf Page active → exit Perf Page (save)
  - Channel Edit active → exit Channel Edit (save)
  - Glide Editor active → exit Glide Editor (save)
  - Armed → disarm (no playback change)
  - Else → Toggle play/stop (save)

---

## Main Mode (Normal — unarmed, no modal)

**Main Mode** is the canonical starting point for the entire UI. All modal states (Channel Edit, Global Settings, Performance Page, Glide Step Editor, Clear Mode) can only be entered from Main Mode, with one exception: Channel Edit can also be entered while Armed (see Armed mode below). Modals do not stack — entering one while another is active is silently blocked.

**State:** `armed_ch_ == nullopt`, all modal flags false, morph/chan not held.

**Entry:** Default state; also reached by disarming or exiting any modal via Play/Reset.

**Exit:** From Main Mode: pressing a Page button (→ step held sub-state), holding CHAN (→ Armed), SHIFT+CHAN short tap (→ Channel Edit), SHIFT+CHAN hold 2s (→ Global Settings), Glide held (→ GLIDE Modifier), Fine+Glide 1.5s (→ Performance Page).

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
| Shift held | Page button N | NavigatePage(N) |
| Shift held | Encoder 4 | cs.range.Inc(dir) |
| Shift held | Encoder 6 | CycleTypeSel(ch, dir) |

Slider recording (motion-gated, via Common()) runs independently for CV channels.

**LED — Encoders (no modifier):** StepColorForChannel(ch, page_step(i)) for all 8 steps on current page; playing step blinks ChaseWhite (suppressed while that page button is held).

**LED — Encoders (Glide held):** GlideFlagColor: full_white = glide on, dim_grey = off.

### Sub-mode: Armed Gate

| Modifier | Input | Action |
|----------|-------|--------|
| None | Page button N (tap) | ToggleGateStep(ch, page_step(N)); last_touched_step_ = N |
| None | Encoder N | EditGateStep(ch, page_step(N), inc, fine) |
| Glide held | Encoder N | EditGateRatchet(ch, page_step(N), dir) |
| Shift held | Page button N | NavigatePage(N) |
| Shift held | Encoder (any) | events drained (no edit) |

> Note: Glide check comes AFTER Shift check in code — Shift takes priority over Glide.

**LED — Encoders (no modifier):** GateStepColor(ch, page_step(i)); playing step blinks ChaseWhite.

**LED — Encoders (Glide held):** GateRatchetColor(ch, page_step(i)) — dim grey = no ratchet, bright green = high count.

### Sub-mode: Armed Trigger

| Modifier | Input | Action |
|----------|-------|--------|
| None | Page button N (tap) | ToggleTrigStep(ch, page_step(N)); last_touched_step_ = N |
| None | Encoder N | EditTrigStep(ch, page_step(N), inc) |
| Shift held | Page button N | NavigatePage(N) |
| Shift held | Encoder (any) | events drained (no edit) |
| Glide held | (no handling) | silently ignored — see Known Issues |

**LED — Encoders:** TrigStepColor(ch, page_step(i)); playing step blinks ChaseWhite (faster blink during repeat).

---

## Mode: GLIDE Modifier

**State:** `c.button.morph.is_high() && !armed_ch_.has_value()`.

This is not a persistent mode — it is active only while Glide is physically held. No state variable; priority 7 in Update() chain.

**Entry:** Hold Glide (unarmed). Shift state is passed as parameter.

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

### Page button held (pending long-press)

| Input | Action |
|-------|--------|
| Encoder N (Gate channel) | EditGateRatchet(N, GlobalStep(page_btn), dir); cancels long-press |

### Long-press fires (600 ms, page button still held)

| Condition | Action |
|-----------|--------|
| Channel N is CV or Gate | glide_editor_active_ = true; glide_editor_ch_ = N |
| Channel N is Trigger | no-op (Trigger has no Glide Step Editor) |
| Shift was held at press time | no-op (long-press entry blocked when shift held) |

**LED:** No dedicated display — uses whatever the current background mode shows.

---

## Mode: Glide Step Editor

**State:** `glide_editor_active_ = true`.

**Entry:** From Main Mode only, via GLIDE modifier: hold Glide, long-press a Page button (600ms) on a CV or Gate channel.

**Exit:**
- Glide button rising edge → `glide_editor_active_ = false`; save if stopped
- Play/Reset → handled at top of Update() before editor runs; `glide_editor_active_ = false`; save if stopped

| Modifier | Input | Action |
|----------|-------|--------|
| None | Encoder N (CV channel) | SetGlideFlag(ch, page_step(N), dir>0) |
| None | Encoder N (Gate channel) | EditGateStep(ch, page_step(N), dir, fine) |
| Shift held | Page button N | NavigatePage(N) |

> Bare page button presses are intentionally not processed (page button 0 overlaps step 1 of channel 0; catching bare presses would make step 1 unreachable).

**LED — Page buttons:** Channel N blinks (blink = `(TimeNow()>>8)&1`). Other buttons off.

**LED — Encoders (CV channel):** full_white if glide flag set; dim_grey if not.

**LED — Encoders (Gate channel):** GateStepColor(ch, page_step(i)).

---

## Mode: Channel Edit

**State:** `channel_edit_active_ = true`.

**Entry:** From Main Mode or Armed Mode: SHIFT+CHAN short tap. On entry:
- `channel_edit_last_enc_ = 2` (pre-selects Length for passive display)
- `length_display_until_ = now + 600ms`
- `current_page_` reset to `playhead(edit_ch_) / 8` (syncs page to focused channel)

**Exit:**
- SHIFT+CHAN short tap → `channel_edit_active_ = false`; save if stopped
- Play/Reset → handled at top of Update(); `channel_edit_active_ = false`; save if stopped

**Focused channel:** `edit_ch_` (persists across entries). Changed by tapping a page button.

### Sub-state: Shift held

| Input | Action |
|-------|--------|
| Page button N | NavigatePage(N) |
| Encoder (any) | events not consumed (no action taken) |

All other processing is skipped while Shift is held.

### Sub-state: Page button press (tap = focus, hold = clear)

`channel_edit_clear_btn_` tracks which button is held. On rising edge, the timer starts. On falling edge before 600ms: `edit_ch_ = btn`; passive length display for 600ms. At 600ms: `ClearChannel(btn)`.

Only one page button tracked at a time (no multi-press in Channel Edit).

### Sub-state: Encoder editing (normal)

All 8 encoders control the focused channel (`edit_ch_`). Turning any encoder sets `channel_edit_last_enc_` and cancels the length display timeout (or restarts it if enc 2 is turned).

| Encoder | Panel label | Parameter |
|---------|-------------|-----------|
| 0 | Start | output_delay_ms, 0–20 |
| 1 | Dir. | direction (Forward/Reverse/PingPong/Random, clamped) |
| 2 | Length | length 1–64; also sets current_page_ = (length-1)/8; restarts 600ms display |
| 3 | Phase | RotateChannel (destructive) |
| 4 | Range / PW | CV: range preset; Trigger: pulse_width_ms 1–100 |
| 5 | BPM/Clock Div | clock division |
| 6 | Transpose | type selector (CV scales → Gate → Trigger, clamped) |
| 7 | Random | random_amount 0–1 in 0.05 steps |

### LED — Length display (active when `TimeNow() <= length_display_until_`)

- **Page buttons:** lit for pages 0..(last_page). last_page = (length-1)/8.
- **Encoders:** type color for positions 0..(steps_on_last_page-1); dim_grey beyond.

### LED — Normal display

- **Page buttons:** `i == edit_ch_` = lit solid. Playing step blinks (chaselight). Shift held: `i == current_page_` only.
- **Encoder 0:** white brightness = output_delay_ms / 20.
- **Encoder 1:** direction color (green/orange/yellow/magenta).
- **Encoder 2–5:** dim_grey.
- **Encoder 6:** ChannelTypeColor(edit_ch_).
- **Encoder 7:** white brightness = random_amount.

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
| Step held + Encoder N | Encoder N | EditCvStep / EditGateStep / EditTrigStep |

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

**Entry:** From Main Mode only: SHIFT+CHAN held 2s (kGlobalSettingsHoldTicks). `c.button.scene.clear_events()` is called on entry.

**Exit:** Play/Reset (top of Update()); saves macro data.

| Input | Action |
|-------|--------|
| Encoder 0 | play_stop_reset toggle |
| Encoder 2 | master_reset_steps 0–64 |
| Encoder 5 | BPM increment (fine=1 BPM, normal=10 BPM) |
| Page button N | reset_leader_ch = N; tap same N to deselect (= 0xFF) |

**LED — Page buttons:** lit = `reset_leader_ch == i`.

**LED — Encoders:**
- 0: red=play/stop reset on, off=off
- 2: off=disabled, orange=non-snap, red=snap (8/16/32/64)
- 5: BPM color zone pulse
- others: dim_grey

---

## Mode: Clear Mode

**State:** `clear_mode_active_ = true`.

**Entry:** From Main Mode only: SHIFT+PLAY held 600ms (kClearHoldTicks). `c.button.scene.clear_events()` and `c.button.play.clear_events()` called on entry. Blinker set.

**Exit:** Any input in UpdateClearMode:
- Page button N → ClearChannel(N); exit
- Play/Reset → ClearChannel(all); exit
- Any other button → exit without clearing

**LED — Encoders:** all off.

**LED — Page buttons + Play LED:** slow blink (~3Hz, `(TimeNow()>>10)&1`).

---

## SHIFT+PLAY Hold State (not a mode — timer)

`shift_play_pending_` is a pending state, not a priority-chain mode. It runs as a parallel timer check in Update() regardless of other modes.

| Event | Action |
|-------|--------|
| Shift released before Play | `shift_play_pending_ = false` (no-op) |
| Play released while Shift held, < 600ms | Reset all channels; `shift_play_pending_ = false` |
| Held 600ms | Enter Clear Mode; consume scene and play button events |

---

## SHIFT+CHAN Hold State (not a mode — timer)

`shift_chan_hold_pending_` is a parallel timer running at the top of Update().

**Arms:** SHIFT+CHAN rising edge, `!global_settings_active_`.

| Event | Action |
|-------|--------|
| CHAN released (or Shift released) before 2s | Toggle `channel_edit_active_`; if entering: init entry state + reset current_page_ |
| Held 2s | `global_settings_active_ = true`; clear scene button events |

> Timer only arms when no modal is active (`!any_modal`). Armed state is not a modal, so Armed → Channel Edit still works. Silently blocked in all other non-Main states.

---

## Phase Scrub Lock (persistent, not a mode)

Managed in `Common()` via `scrub_hold_pending_` / `phase_locked_` / `picking_up_`. Runs unconditionally every tick regardless of mode.

**Lock toggle:** Fine+Glide tapped and released (either button released before 1.5s) → `DoLockToggle()`.

**Perf Page entry:** Fine+Glide held ≥1.5s → `perf_page_active_ = true`. Blocked if Channel Edit, Global Settings, Glide Editor, or Clear Mode is active.

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

`shift_chan_hold_pending_` now guards against all modal flags (`clear_mode_active_ || global_settings_active_ || perf_page_active_ || channel_edit_active_ || glide_editor_active_`). The timer only arms from Main Mode or Armed Mode. Armed is not a modal, so Armed → Channel Edit still works.

### 2. ~~Fine+Glide hold fires in any active mode~~ — Resolved

`scrub_hold_pending_` now guards against `clear_mode_active_ || global_settings_active_ || channel_edit_active_ || glide_editor_active_`. Perf Page is not blocked (to allow re-entry of Perf Settings from within Perf Page). Entry from Clear Mode, Channel Edit, Global Settings, and Glide Editor is now silently blocked.

### 3. Glide ignored when armed Trigger

`UpdateArmedTrigger` has no Glide button handling. While armed Trigger + Glide held:
- The `UpdateGlideModifier` block is skipped (only runs when `!armed_ch_`)
- GLIDE modifier behavior (pulse width, offset all steps) is inaccessible
- Encoders still edit ratchet/repeat counts as normal
- Glide button has no visible effect

### 4. Shift+Glide collision in armed CV

`UpdateArmedCV` checks Glide first, then Shift. If both are held simultaneously:
- Glide wins (per-step glide flag editing)
- Shift+enc 4/6 (range/scale/page) is inaccessible while Glide is held

### 5. Page button in UpdatePerfPage does not set step held

In `UpdatePerfPage`, page buttons toggle `scrub_ignore_mask` on any press regardless of whether a step-edit is intended. The code then falls through to check `p.AnyStepHeld()` for encoder editing. But since page button presses are consumed by the mask toggle, holding a page button then turning an encoder does NOT work in Perf Page — `scene[i].is_high()` returns true (button is held) but the step-edit check uses `p.AnyStepHeld()` which depends on `SetStepHeld()` being called, which never happens in Perf Page.

Actually, re-reading the code — `p.AnyStepHeld()` tracks logical step holds set via `p.SetStepHeld()`, not physical button states. Since Perf Page never calls `SetStepHeld`, `AnyStepHeld()` is always false there. The `ForEachEncoderInc` block in `UpdatePerfPage` is unreachable. Step editing via hold-page+encoder is broken in Perf Page despite being documented.

### 6. current_page_ is shared state across all modes

`current_page_` is a single value used as context by every mode. Navigation in one mode carries over to the next:
- Navigate to page 3 in Perf Page → Channel Edit opens on page 3 even if the focused channel's sequence is only 8 steps (page 0). *(Partially mitigated in alpha13: Channel Edit entry now resets `current_page_` to the focused channel's playhead page.)*
- Navigate in Glide Editor → affects Normal mode display on return.
- No mode resets `current_page_` on exit except Channel Edit entry.

### 7. Clear Mode does not clear armed state

SHIFT+PLAY while armed starts `shift_play_pending_`, which can enter Clear Mode. `armed_ch_` is not cleared. After Clear Mode exits, `armed_ch_` is still set and armed mode resumes. This is functionally harmless but may be surprising.

### 8. Channel Edit long-press clear vs. page focus race

In Channel Edit, pressing a page button starts the long-press clear timer. If the user intends to focus a channel (short tap) but holds slightly too long (≥600ms), all steps for that channel are cleared with no undo. There is a 3-flash blinker confirmation but no hold-to-confirm or two-step gesture.

### 9. save_macro deferred while playing

Saves to flash are deferred to the next play/stop toggle when playing. Settings changed in Channel Edit, type changes via CHAN+encoder, and glide/ratchet editor changes are all lost on a power cycle if the sequence is never stopped.

### 10. GLIDE modifier page-button tracking is independent of armed state guard

`UpdateGlideModifier` is only called when `!armed_ch_`. However, `glide_longpress_btn_` and `glide_longpress_timer_` are not reset when arming/disarming. If a page button is pressed in the GLIDE modifier, then the user arms a channel before the 600ms timer fires, the timer state is left dangling. On next GLIDE modifier entry, the stale timer check could immediately fire a Glide Editor entry for the wrong channel.

---

## Save Behavior Summary

| Event | Save target | Condition |
|-------|-------------|-----------|
| Play/Stop toggle | do_save_macro | always |
| Channel Edit exit (Play or SHIFT+CHAN) | do_save_macro | only if stopped |
| Glide Editor exit (Glide or Play) | do_save_macro | only if stopped |
| Global Settings exit (Play) | do_save_macro | always |
| Perf Page exit (Play) | do_save_shared | always |
| Perf Settings exit (Play or Fine+Glide) | do_save_shared | always |
| CHAN+encoder type change | do_save_macro | deferred to CHAN release; only if stopped |
| Orbit follow mask toggle | do_save_shared | always |
| Phase Scrub Lock toggle | do_save_shared | always |
| Channel clear (Clear Mode / Channel Edit long-press) | do_save_macro | sets flag; actual write at next save trigger |
