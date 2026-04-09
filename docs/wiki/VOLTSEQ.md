# VoltSeq Mode

VoltSeq is a second firmware personality for the **Catalyst Sequencer** panel. It turns the module into an **8-channel voltage recorder and step sequencer**, similar to Voltage Block. Each of the 8 outputs is an independent channel with its own step data, type, length, clock division, direction, and more.

> **Firmware variant:** VoltSeq is a separate build (`catalyst-vX.Y.Z-catseq-voltseq.wav`). Load it via the audio-cable bootloader. The CatSeq+CatCon build is a separate file.

---

## Hardware Overview

VoltSeq uses the same physical panel as the Catalyst Sequencer. Panel controls are repurposed in VoltSeq:

| Physical control | VoltSeq function |
|---|---|
| Encoders 1–8 + jacks | Channel outputs; encoders edit step values |
| Page buttons 1–8 | Step select / channel arm / page nav (in editors) |
| Phase Scrub slider | CV recorder input / Performance page orbit |
| Play/Reset | Play/Stop; Shift+Play = Reset/Clear |
| Tap Tempo | Tap tempo |
| Glide | GLIDE modifier (hold for glide time / gate offset / pulse width) |
| Chan. | Channel arm / Channel Edit |
| Shift | Modifier; hold 1.5 s alone = Global Settings |
| Fine | Fine-adjust modifier; Fine+Glide = Performance page / lock |
| Clock In | External clock input |
| Reset jack | Hardware reset trigger |

---

## Switching Modes

VoltSeq and CatSeq share the same panel and can be switched in software:

**Sequencer → VoltSeq:** Hold **Shift + Tap Tempo + Chan.** for 1 second. All 8 channel LEDs blink to confirm.

**VoltSeq → Sequencer:** Hold **Fine + Play/Reset + Glide** for 1 second. All 8 channel LEDs blink to confirm.

The active mode is saved to flash and recalled on boot. Switching does **not** erase the other mode's data.

---

## Channels and Channel Types

VoltSeq has **8 independent channels**, each corresponding to one output jack and encoder knob.

Each channel has a **type**, set in Channel Edit (encoder 7):

| Type | Output | Encoder LED |
|---|---|---|
| **CV** | Stepped or slewed voltage, configurable range | Scale color (grey = unquantized, pink = chromatic, etc.) |
| **Gate** | Gate signal with variable length (0–100%) | Dim green; brightness = gate length |
| **Trigger** | Short trigger pulse with ratchet/repeat | Green = ratchet, teal = repeat, off = rest |

Default range for new channels is **−5V to +5V**. Changing a channel's type does not erase its step data — the same stored values are simply interpreted differently by each type.

---

## Clock

- **External clock:** patch a signal into **Clock In**. VoltSeq locks to it automatically.
- **Internal clock:** tap **Tap Tempo** to set BPM (first two taps are ignored; BPM updates on the third tap and beyond). Fine-tune BPM in Global Settings (encoder 6).
- **Reset jack:** a trigger resets all channels to step 1.

---

## Main Mode

The **main mode** is VoltSeq's default state — reached on boot, after disarming, or after exiting any editor. All other modes are entered from here and return here on exit. While in any modal editor (Channel Edit, Global Settings, Performance Page, Glide Step Editor, Clear Mode), the entry combos for other modals are silently ignored — you must exit the current mode first. The only exception is **Channel Edit**, which can also be entered while a channel is armed.

---

## General Operation

### Play / Stop / Reset

| Action | Result |
|---|---|
| **Play/Reset** | Toggle play/stop |
| **Shift + Play** (short, < 600 ms) | Reset all channels to step 1 |
| **Shift + Play** (hold ≥ 600 ms) | Enter Clear mode |
| **Play/Reset** while armed | Disarm channel (playback continues) |
| **Play/Reset** while in any edit mode | Exit that mode and save |

Settings are saved to flash whenever play/stop is toggled.

### Clear Mode

From the main mode: hold **Shift + Play/Reset** for 600 ms. All page button LEDs and the Play LED slow-blink to confirm entry.

| Action | Result |
|---|---|
| Tap **Page button N** | Clear all 64 steps of channel N (CV → center/0V; Gate/Trigger → all off) |
| Tap **Play/Reset** | Clear all 8 channels |
| Any other button | Exit without clearing |

### Page Navigation

Steps are organized into **8 pages of 8 steps** per channel, giving 64 total steps per channel.

**Shift + Page button N** navigates to page N in every mode: normal, armed (any channel type), Channel Edit, Glide Step Editor, and Performance Page.

While Shift is held (in normal or armed mode), the page buttons switch to a **navigation indicator**: the currently selected page button lights solid, and the encoder LED for the currently tracked channel blinks to confirm which channel's chaselight you are viewing. Pressing any page button in this state navigates without editing any steps.

### Step Editing

Hold a **Page button** and turn an **encoder** to edit step values:

- **Encoder N** sets the value for **channel N** at the held step.
- Hold **multiple Page buttons** to edit the same position across multiple steps at once.
- For CV channels: coarse ≈ 1 semitone per detent; hold **Fine** for sub-semitone. Fast spinning accelerates.
- For Gate channels: adjusts gate length (0–100%). No acceleration.
- For Trigger channels: adjusts ratchet/repeat count (±8). No acceleration.

Channels shorter than the current page wrap to their equivalent position — a Trigger channel with 8 steps will output its step 0 when you are editing page 2 step 1.

### Chaselight

In the main mode (unarmed), the **page button LEDs** show the current playhead position for the most recently focused channel.

The tracked channel updates two ways:
- **Turn an encoder while holding a page button** — edits a step and focuses that encoder's channel.
- **Turn an encoder without holding any page button** — focuses that channel without editing anything. Use this to quickly aim the chaselight at any channel while playing.

---

## Armed Mode

From the main mode: **CHAN + Page button N** arms channel N for focused editing. The encoder LED for that channel blinks. To disarm:

- **CHAN + same Page button N** — disarm
- **Play/Reset** — disarm without stopping playback

### Armed CV Channel

The **Phase Scrub slider** records in real time:

- While **playing**: slider value is written to the current step on every clock tick.
- While **stopped**: slider continuously sets the last-touched step.

The slider maps across the channel's configured **Range**.

**Encoder LEDs:** show the CV step color for each step on the current page. The playing step blinks white (chaselight). The blink is suppressed on any held Page button so the color stays visible while editing.

**Encoder N:** directly edits step N's CV value. Fine = sub-semitone; fast spinning accelerates.

**Glide held while armed:** encoder LEDs switch to show per-step glide flags (white = glide on, dim = off). Turn encoder N CW to enable glide on step N; CCW to disable. Glide time (the slew duration) is set separately via the unarmed GLIDE modifier.

**Shift held while armed:**

| Encoder | Panel label | Parameter |
|---|---|---|
| 5 | Range | Channel voltage range (also sets slider recording window) |
| 7 | Transpose | Quantizer scale |

### Armed Gate Channel

**Encoder LEDs:** show each step's gate state. Playing step blinks white. While **Glide** is held, LEDs switch to show per-step ratchet counts (dim = no ratchet, bright green = high ratchet).

- **Tap a Page button** → toggle step on/off (x0x)
- **Encoder N** → adjust gate length for step N (0 = off, up to 100%)
- **Glide held + Encoder N** → adjust ratchet count for step N (0 = single gate, 2–8 = fire N pulses per step, subdivided within the step period)

### Armed Trigger Channel

**Encoder LEDs:** show ratchet/repeat state. Playing step blinks white.

- **Tap a Page button** → toggle step between rest and single trigger
- **Encoder N CW** → increase ratchet count (subdivide; 1–8)
- **Encoder N CCW** → increase repeat count (extend; negative values)

---

## GLIDE Modifier

Hold **Glide** to access per-channel live parameters:

| Encoder N | CV channel | Gate channel | Trigger channel |
|---|---|---|---|
| Turn | Glide time (0–10 s) | Offset all gate lengths | Trigger pulse width (1–100 ms) |

**Step held + Glide + Encoder N (Gate channel):** set ratchet count for that specific step of Gate channel N.

**Shift + Glide + encoder N:** offset all ratchet/repeat counts for Trigger channel N.

For CV channels, glide time is a channel-level setting (how long the slew takes). Which steps actually slew is controlled per-step while armed — see **Armed CV Channel** below.

---

## Channel Edit

**Entry:** From the main mode or while armed — press and release **Shift + Chan.** (short tap — release before 2 seconds). A 2-second hold enters Global Settings instead.

Press a **Page button** to focus that channel. Encoders edit per-channel settings for the focused channel.

**Long-press** (600 ms) a Page button to **clear all steps** for that channel.

**Page buttons** show a chaselight for the focused channel's current playing position. While **Shift is held**, the page buttons switch to a page indicator (currently selected page lights solid) so you can orient yourself before navigating.

| Encoder | Panel label | Parameter | Notes |
|---|---|---|---|
| 1 | Start | Output delay | 0–20 ms; LED brightness = amount |
| 2 | Dir. | Direction | Forward / Reverse / Ping-Pong / Random — clamped; LED color = direction |
| 3 | Length | Step length | 1–64 steps |
| 4 | Phase | Phase rotate | Destructive; rotates active steps only |
| 5 | Range | Voltage range (CV) / Pulse width (Trigger) | CV: range preset; Trigger: 1–100 ms |
| 6 | BPM/Clock Div | Per-channel clock division | — |
| 7 | Transpose | Channel type / scale | CV scales → Gate → Trigger; LED = type color |
| 8 | Random | Random amount | 0–100%; LED brightness = amount |

**Exit:** **Shift + Chan.** (short tap) or **Play/Reset** — saves and returns.

> Phase rotate is destructive with no undo.

---

## Global Settings

**Entry:** From the main mode — hold **Shift + Chan.** for **2 seconds**. The hold is a two-button combo: press and hold both Shift and Chan. together; if either is released before 2 seconds, it fires as a short tap instead (which enters/exits Channel Edit). Any other button press during the hold cancels it.

**Exit:** **Play/Reset** — saves and returns.

### Encoders

| Encoder | Panel label | Parameter | LED |
|---|---|---|---|
| 1 | Start | Play/Stop reset mode | Red = on (Stop also resets to step 1), off = off |
| 3 | Length | Master reset steps | Red = musical snap (8/16/32/64), orange = other value, off = disabled |
| 6 | BPM/Clock Div | Internal BPM | Yellow pulse = current phase; **white** pulse at 80/100/120/140 BPM |

### Play/Stop Reset Mode

When **on** (encoder 1 red): pressing Stop resets all channels to step 1 in addition to stopping playback. Useful for performance situations where you always want sequences to restart from the top.

### Master Reset

When non-zero (encoder 3): all channels automatically reset to step 1 every N master clock ticks. At **8, 16, 32, or 64** steps the LED turns red — these align to musical bar lengths at common channel lengths.

### Reset Leader

**Page buttons** select a **reset leader channel** (radio select — only one at a time). When the leader channel completes its sequence and wraps back to step 1, all other channels reset with it, keeping everything phase-locked.

- Tap a page button to select that channel as leader (button lights up)
- Tap the lit button again to deselect (no leader)

**Priority:** if Master Reset is set (encoder 3 ≠ 0), it overrides the Reset Leader.

---

## Performance Page

**Entry:** From the main mode — hold **Fine + Glide** for 1.5 seconds. Encoder LEDs light to confirm. Releasing before 1.5 s cancels without entering.

**Exit:** **Play/Reset** — saves to flash.

The Performance Page puts the Phase Scrub slider in control of an **orbit engine** that manipulates playback positions across channels.

### Controls

| Control | Function |
|---|---|
| Slider | Orbit center position |
| Page buttons | Toggle per-channel orbit follow (lit = channel follows slider) |
| Shift (held) | Freeze orbit center in beat-repeat mode |
| Shift + Page button | Navigate pages |
| Hold Page + turn Encoder | Step editing (same as main mode) |

### Settings

Shown immediately on entry. Encoder LEDs reflect current values.

| Encoder | Panel label | Parameter | LED |
|---|---|---|---|
| 1 | Start | Quantize orbit center | Orange = on, off = off |
| 2 | Dir. | Performance mode | Off = disabled / green = granular / blue = beat-repeat ×8 / cyan = beat-repeat ×4 |
| 3 | Length | Granular width *or* beat-repeat debounce | Dim→bright orange (granular) / dim→bright white (beat-repeat) |
| 4 | Phase | Orbit direction (granular only) | Green / blue / orange / lavender |
| 8 | Random | Phase Scrub Lock | Red = locked, off = unlocked |

**Page buttons** also toggle the per-channel orbit follow mask here.

### Phase Scrub Lock

Toggle the slider lock with either:
- **Encoder 8** (Random) while in Performance Page Settings
- **Short Fine + Glide** (tap and release before 1.5 s) — works from anywhere, including outside the Performance Page

**Locked:** slider is frozen at its current position; encoder 8 LED is red.

**Unlock:** entering pickup mode — the orbit center resumes only after the slider physically reaches the locked position, preventing position jumps.

Lock state persists across power cycles.

---

## Saving

VoltSeq saves automatically at these moments:

- Play/Stop toggled
- Global Settings exited
- Channel Edit exited
- Performance Page exited
- Mode switched

> **Note:** settings changed while playing (channel type, length, range, etc.) are not written to flash until the next play/stop toggle. Power-cycling before stopping will lose those changes.

---

## LED Reference

| Situation | LED |
|---|---|
| Main mode, unarmed | Each encoder: channel's current step color (playhead position) |
| Main mode, step held for editing | Each encoder: that step's color across all channels |
| Main mode, page buttons | Chaselight for most recently focused channel |
| Main mode, Shift held | Page buttons: current page lit solid; focused encoder blinks white |
| Armed CV | Each encoder: step CV color for current page; playing step blinks white |
| Armed Gate | Each encoder: step gate state; playing step blinks white |
| Armed Gate + Glide held | Each encoder: per-step ratchet count (dim = none, bright green = high) |
| Armed Trigger | Each encoder: step ratchet/repeat state; playing step blinks white |
| CHAN held (no page button) | Each encoder: channel type color |
| Channel Edit — enc 1 | Brightness = output delay |
| Channel Edit — enc 2 | Green/Orange/Yellow/Magenta = direction |
| Channel Edit — enc 7 | Type/scale color |
| Channel Edit — enc 8 | Brightness = random amount |
| Channel Edit — page buttons | Chaselight for focused channel |
| Channel Edit, Shift held | Page buttons: current page lit solid (not focused channel) |
| Global Settings — enc 1 | Red = play/stop reset on, off = off |
| Global Settings — enc 3 | Red = snap steps (8/16/32/64), orange = other, off = disabled |
| Global Settings — enc 6 | BPM color zone pulse (red <50, orange 50–79, yellow 80–99, green 100–119, blue 120–149, teal 150–179, lavender 180+) |
| Global Settings — page buttons | Lit = reset leader channel |
| Performance Page | Each encoder: channel's current output step color |
| Perf Settings — enc 8 | Red = locked; blinks red during pickup |

---

## Quick Reference

| Combo | Action |
|---|---|
| **Play/Reset** | Toggle play/stop |
| **Shift + Play** (short) | Reset all channels to step 1 |
| **Shift + Play** (hold 600 ms) | Enter Clear mode |
| **Shift + Chan.** (short tap) | Enter/exit Channel Edit |
| **Shift + Chan.** (hold 2 s) | Enter Global Settings |
| **Chan. + Page N** | Arm channel N |
| **Chan. + same Page N** | Disarm channel N |
| **Play/Reset** while armed | Disarm (playback continues) |
| **Shift + Page N** | Navigate to page N (works in all modes) |
| **Hold Shift** (normal/armed) | Page indicator on page buttons; focused encoder blinks |
| **Wiggle encoder** (no step held) | Focus chaselight on that channel |
| **Hold Page N + turn Encoder M** | Edit channel M's value at step N |
| Armed CV + **Encoder N** | Edit step N's CV value |
| Armed CV + **Shift + Enc 5** | Adjust channel range |
| Armed CV + **Shift + Enc 7** | Adjust quantizer scale |
| Armed Gate + **Encoder N** | Adjust gate length for step N |
| Armed Gate + **Glide held + Encoder N** | Adjust ratchet count for step N (0 = single, 2–8 = subdivide) |
| Armed Gate/Trig + **tap Page N** | Toggle step N on/off |
| Armed Trigger + **Encoder N** | Adjust ratchet/repeat count for step N |
| **Hold Glide + step Page N + Encoder M** | Adjust Gate channel M ratchet at step N (unarmed) |
| Channel Edit + **long-press Page N** | Clear all steps for channel N |
| Channel Edit + **Shift + Page N** | Navigate to page N |
| **Hold Glide + Encoder N** | Glide time / gate offset / pulse width for channel N |
| **Hold Shift + Glide + Encoder N** | Offset all ratchet counts for Trigger channel N |
| Armed CV + **Hold Glide + Encoder N** | Enable (CW) / disable (CCW) glide on step N |
| **Fine + Glide** (hold 1.5 s) | Enter Performance Page |
| **Short Fine + Glide** (tap + release) | Toggle Phase Scrub Lock |
| **Shift + Tap + Chan.** (hold 1 s) | Switch to VoltSeq mode |
| **Fine + Play + Glide** (hold 1 s) | Switch to Sequencer mode |
