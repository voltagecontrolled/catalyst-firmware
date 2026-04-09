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
| Glide | GLIDE modifier (hold for glide/pulse-width/step editor) |
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

Hold **Shift + Play/Reset** for 600 ms. All page button LEDs and the Play LED slow-blink to confirm entry.

| Action | Result |
|---|---|
| Tap **Page button N** | Clear all 64 steps of channel N (CV → center/0V; Gate/Trigger → all off) |
| Tap **Play/Reset** | Clear all 8 channels |
| Any other button | Exit without clearing |

### Page Navigation

Steps are organized into **8 pages of 8 steps** per channel, giving 64 total steps per channel.

Page navigation is available inside any editor (Channel Edit, Glide Step Editor, Ratchet Step Editor, Performance Page) via **Shift + Page button N**.

> Page navigation is not available in the main mode while Shift is held — holding Shift alone for 1.5 s enters Global Settings instead.

### Step Editing

Hold a **Page button** and turn an **encoder** to edit step values:

- **Encoder N** sets the value for **channel N** at the held step.
- Hold **multiple Page buttons** to edit the same position across multiple steps at once.
- For CV channels: coarse ≈ 1 semitone per detent; hold **Fine** for sub-semitone. Fast spinning accelerates.
- For Gate channels: adjusts gate length (0–100%). No acceleration.
- For Trigger channels: adjusts ratchet/repeat count (±8). No acceleration.

Channels shorter than the current page wrap to their equivalent position — a Trigger channel with 8 steps will output its step 0 when you are editing page 2 step 1.

### Chaselight

In the main mode (unarmed), the **page button LEDs** show the current playhead position for the most recently edited channel. The tracked channel updates whenever you turn an encoder while a page button is held.

---

## Armed Mode

**CHAN + Page button N** arms channel N for focused editing. The encoder LED for that channel blinks. To disarm:

- **CHAN + same Page button N** — disarm
- **Play/Reset** — disarm without stopping playback

### Armed CV Channel

The **Phase Scrub slider** records in real time:

- While **playing**: slider value is written to the current step on every clock tick.
- While **stopped**: slider continuously sets the last-touched step.

The slider maps across the channel's configured **Range**.

**Encoder LEDs:** show the CV step color for each step on the current page. The playing step blinks white (chaselight). The blink is suppressed on any held Page button so the color stays visible while editing.

**Encoder N:** directly edits step N's CV value. Fine = sub-semitone; fast spinning accelerates.

**Shift held while armed:**

| Encoder | Panel label | Parameter |
|---|---|---|
| 5 | Range | Channel voltage range (also sets slider recording window) |
| 7 | Transpose | Quantizer scale |

### Armed Gate Channel

**Encoder LEDs:** show each step's gate state. Playing step blinks white.

- **Tap a Page button** → toggle step on/off (x0x)
- **Encoder N** → adjust gate length for step N (0 = off, up to 100%)

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

**Shift + Glide + encoder N:** offset all ratchet/repeat counts for Trigger channel N.

### Glide Step Editor

Hold **Glide** and **long-press** (600 ms) a **Page button** on a CV or Gate channel.

- **Encoder N CW** = enable glide on step N; **CCW** = disable
- For Gate channels: encoders adjust each step's gate length individually
- **Shift + Page button** = navigate pages
- **Glide** or **Play/Reset** = exit and save

### Ratchet Step Editor

Hold **Glide** and **long-press** (600 ms) a **Page button** on a Trigger channel.

- **Encoder N** adjusts ratchet/repeat count for step N (positive = ratchet, negative = repeat)
- **Shift + Page button** = navigate pages
- **Glide** or **Play/Reset** = exit and save

---

## Channel Edit

**Entry:** Hold **Shift + Chan.**

Press a **Page button** to focus that channel. Encoders edit per-channel settings for the focused channel.

**Long-press** (600 ms) a Page button to **clear all steps** for that channel.

**Page buttons** show a chaselight for the focused channel's current playing position, so you can verify direction changes are taking effect while the sequence runs.

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

**Exit:** **Shift + Chan.** or **Play/Reset** — saves and returns.

> Phase rotate is destructive with no undo.

---

## Global Settings

**Entry:** Hold **Shift** alone for **1.5 seconds** (any other button press during the hold cancels it). All other Shift combos — Shift+Play, Shift+Chan, etc. — work normally.

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

**Entry:** Hold **Fine + Glide** for 1.5 seconds. Encoder LEDs light to confirm. Releasing before 1.5 s cancels without entering.

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
- Glide Step Editor exited
- Ratchet Step Editor exited
- Performance Page exited
- Mode switched

> **Note:** settings changed while playing (channel type, length, range, etc.) are not written to flash until the next play/stop toggle. Power-cycling before stopping will lose those changes.

---

## LED Reference

| Situation | LED |
|---|---|
| Main mode, unarmed | Each encoder: channel's current step color (playhead position) |
| Main mode, step held for editing | Each encoder: that step's color across all channels |
| Main mode, page buttons | Chaselight for most recently edited channel |
| Armed CV | Each encoder: step CV color for current page; playing step blinks white |
| Armed Gate/Trigger | Each encoder: step state for current page; playing step blinks white |
| CHAN held (no page button) | Each encoder: channel type color |
| Glide/Ratchet Step Editor | Active channel's page button blinks |
| Channel Edit — enc 1 | Brightness = output delay |
| Channel Edit — enc 2 | Green/Orange/Yellow/Magenta = direction |
| Channel Edit — enc 7 | Type/scale color |
| Channel Edit — enc 8 | Brightness = random amount |
| Channel Edit — page buttons | Chaselight for focused channel |
| Global Settings — enc 1 | Red = play/stop reset on, off = off |
| Global Settings — enc 3 | Red = snap steps (8/16/32/64), orange = other, off = disabled |
| Global Settings — enc 6 | Yellow pulse (white at snap BPM) = current BPM phase |
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
| **Hold Shift alone (1.5 s)** | Enter Global Settings |
| **Chan. + Page N** | Arm channel N |
| **Chan. + same Page N** | Disarm channel N |
| **Play/Reset** while armed | Disarm (playback continues) |
| **Shift + Chan.** | Enter/exit Channel Edit |
| **Hold Page N + turn Encoder M** | Edit channel M's value at step N |
| Armed CV + **Encoder N** | Edit step N's CV value |
| Armed CV + **Shift + Enc 5** | Adjust channel range |
| Armed CV + **Shift + Enc 7** | Adjust quantizer scale |
| Armed Gate/Trig + **Encoder N** | Edit gate length / ratchet-repeat for step N |
| Armed Gate/Trig + **tap Page N** | Toggle step N on/off |
| Channel Edit + **long-press Page N** | Clear all steps for channel N |
| Channel Edit + **Shift + Page N** | Navigate to page N |
| **Hold Glide + Encoder N** | Glide time / gate offset / pulse width for channel N |
| **Hold Shift + Glide + Encoder N** | Offset all ratchet counts for Trigger channel N |
| **Hold Glide + long-press Page N** (600 ms) | Enter Glide Step Editor (CV/Gate) or Ratchet Editor (Trigger) |
| **Fine + Glide** (hold 1.5 s) | Enter Performance Page |
| **Short Fine + Glide** (tap + release) | Toggle Phase Scrub Lock |
| **Shift + Tap + Chan.** (hold 1 s) | Switch to VoltSeq mode |
| **Fine + Play + Glide** (hold 1 s) | Switch to Sequencer mode |
