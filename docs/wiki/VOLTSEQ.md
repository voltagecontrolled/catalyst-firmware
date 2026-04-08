# VoltSeq Mode

VoltSeq is a second firmware personality for the **Catalyst Sequencer** panel. It turns the module into an **8-channel voltage recorder and step sequencer**, similar to Voltage Block. Each of the 8 outputs is an independent channel with its own step data, channel type, length, clock division, direction, and more.

> **Firmware variant:** VoltSeq is a separate build (`catalyst-vX.Y.Z-catseq-voltseq.wav`). Load it via the audio-cable bootloader. The CatSeq+CatCon build is a separate file.

---

## Hardware Overview (Sequencer Panel)

VoltSeq uses the same physical panel as the Catalyst Sequencer. Controls behave differently in VoltSeq mode:

| Physical control | VoltSeq function |
|---|---|
| Outputs 1–8 (encoder + jack) | Channel outputs; encoders edit step values |
| Page buttons 1–8 | Step select / channel arm / page navigation |
| Phase Scrub (slider) | Voltage recorder input / Performance page |
| Play/Reset | Play/Stop; Shift+Play = Reset |
| Tap Tempo | Tap tempo / internal clock |
| Glide (Ratchet) | GLIDE modifier (hold for special functions) |
| Chan. | Channel arm / Channel Edit access |
| Shift | Shift modifier (page nav, global settings) |
| Fine/Copy | Fine-adjust modifier; entry to Performance page |
| Clock In (jack) | External clock |
| Reset (jack) | Patch reset |

---

## Switching Modes

VoltSeq and CatSeq share the same panel. You can switch modes in software without reflashing:

**Sequencer → VoltSeq:** Hold **Shift + Tap Tempo + Chan.** for 1 second. All 8 channel LEDs blink to confirm.

**VoltSeq → Sequencer:** Hold **Fine + Play/Reset + Glide** for 1 second. All 8 channel LEDs blink to confirm.

The active mode is saved to flash and recalled on boot.

> Switching modes does **not** erase the other mode's data. Sequencer patterns and VoltSeq channel data are stored in separate flash sectors.

---

## Channels and Channel Types

VoltSeq has **8 independent channels**, each corresponding to one output jack and encoder knob.

Each channel has a **type**:

| Type | Output | LED color |
|---|---|---|
| **CV** | Stepped or slewed voltage (–5V to +10V range, per-channel) | Scale color (e.g. yellow = chromatic, blue = custom) |
| **Gate** | Gate signal with variable length (0–100%) | Yellow; brightness = gate length |
| **Trigger** | Short trigger pulse with variable width; ratchet/repeat | Green = ratchet, teal = repeat |

Change channel type in **Channel Edit** (see below).

---

## Clock

VoltSeq can run from an external clock or an internal clock:

- **External clock:** patch a clock signal into the **Clock In** jack. When a signal is detected, VoltSeq uses it automatically.
- **Internal clock:** tap **Tap Tempo** to set BPM. You can also adjust BPM precisely with Encoder 7 while holding **Shift** (see Global Settings).
- **Reset:** a trigger at the **Reset jack** resets all channels to step 1.

---

## Play / Stop / Reset

| Action | Result |
|---|---|
| Press **Play/Reset** | Toggle play/stop |
| Press **Shift + Play/Reset** | Reset all channels to step 1 |

Settings are saved to flash when play/stop is toggled.

---

## Pages

Steps are organized into **8 pages of 8 steps** per channel, giving 64 total steps per channel. Navigate pages with:

**Shift + Page button N** → Jump to page N.

The current page determines which 8 steps are visible and editable.

---

## Step Editing

### Normal editing (no channel armed)

Hold a **Page button** and turn any **encoder** to set that step's value across channels:

- **Encoder N** sets the value for **channel N** at the held step.
- For CV channels: coarse steps ≈ 1 semitone; hold **Fine** for fine adjustment.
- For Gate channels: adjusts gate length (0–100%).
- For Trigger channels: adjusts ratchet/repeat count (±8).
- You can hold **multiple Page buttons** simultaneously to edit the same position across channels at once.

> Encoders map directly to channels: Encoder 1 = Channel 1, Encoder 8 = Channel 8.

### Armed CV recording (slider)

To record CV values from the **Phase Scrub slider** into a channel:

1. Hold **Chan.** and press a **Page button** to **arm that channel** (LED blinks).
2. While armed and playing: the slider value is recorded to the current playback step on every clock tick.
3. While armed and stopped: the slider sets the value of the last-touched step in real time.
4. Press **Chan.** and the same **Page button** again to disarm, or press the armed channel's Page button alone.

You can adjust the slider's **recording window** (voltage range mapped to slider travel) while the channel is armed by turning Encoders 3 and 4:

| Encoder | Parameter |
|---|---|
| Encoder 3 | Slider span (1, 2, 3, 4, 5, 10, 15 V) |
| Encoder 4 | Slider base voltage (–5, 0–10 V) |

### Armed Gate editing

1. Arm a Gate channel as above.
2. **Tap a Page button** to toggle that step on/off (x0x style).
3. **Hold a Page button and turn any encoder** to adjust the gate length for that step.

### Armed Trigger editing

1. Arm a Trigger channel as above.
2. **Tap a Page button** to toggle that step between rest and trigger.
3. **Hold a Page button and turn any encoder** to adjust ratchet/repeat count for that step.

---

## GLIDE Modifier

Hold **Glide (Ratchet)** to access per-channel live parameters:

| Encoder | Channel type | Parameter |
|---|---|---|
| Any encoder N | CV | Glide time for channel N (0–10 s) |
| Any encoder N | Gate | Offset all gate lengths for channel N (+/– 1% per step) |
| Any encoder N | Trigger | Trigger pulse width for channel N (1–100 ms) |

Hold **Shift + Glide** and turn an encoder to offset all ratchet/repeat counts for a Trigger channel.

### Entering Glide Step Editor

Hold **Glide** and **long-press** (600 ms) a **Page button** on a CV or Gate channel.

In the Glide Step Editor:
- **Encoder N** on the current page sets the glide flag for step N of the focused channel:
  - Turn **CW** = glide on for that step
  - Turn **CCW** = glide off for that step
- For Gate channels: encoders adjust the gate length for each step.
- Hold **Shift + Page button** to navigate pages while staying in the editor.
- Press **Glide** or the focused channel's **Page button** to exit and save.

### Entering Ratchet Step Editor

Hold **Glide** and **long-press** (600 ms) a **Page button** on a Trigger channel.

In the Ratchet Step Editor:
- **Encoder N** adjusts the ratchet/repeat count for step N on the current page:
  - Positive = ratchet (subdivide; green)
  - Negative = repeat (extend; teal)
- Hold **Shift + Page button** to navigate pages.
- Press **Glide** or the focused channel's **Page button** to exit and save.

---

## Channel Edit

**Combo:** Shift + Chan.

Enters per-channel settings. Press a **Page button** to focus that channel. Encoders then edit settings for the focused channel:

| Encoder | Parameter | Range |
|---|---|---|
| 1 | Direction | Forward → Reverse → Ping-Pong → Random |
| 2 | Phase rotate (destructive) | Rotates all 64 steps forward or backward |
| 3 | Range (CV) / Pulse width (Trigger) | CV: voltage output range; Trigger: 1–100 ms |
| 4 | Channel type | CV scales → Gate → Trigger (cycles) |
| 5 | Output delay | 0–20 ms (delays the CV/gate output for this channel) |
| 6 | Step length | 1–64 steps |
| 7 | Clock division | Per-channel clock divisor |
| 8 | Random amount | 0–100% (adds random variation to output values) |

**Exit:** press **Chan.** to save channel settings and return to normal mode.

> Phase rotate is destructive — it permanently shifts all step values for the channel. There is no undo.

---

## Global Settings

Hold **Shift** in normal mode and turn encoders to set sequencer-wide defaults used when creating new channels or resetting:

| Encoder | Parameter |
|---|---|
| 1 | Default direction |
| 3 | Default voltage range |
| 6 | Default step length (1–64) |
| 7 | Internal BPM (Fine for fine adjustment) |

---

## Performance Page (Slider)

**Entry:** Hold **Fine + Glide** for 1.5 seconds. Encoder LEDs light to confirm.

**Exit:** Hold **Fine + Glide** briefly (release before 1.5 s), or press **Play/Reset**.

The Performance Page puts the Phase Scrub slider in control of an **orbit engine** that manipulates playback positions across channels — similar to the Sequencer's Phase Scrub Performance Page.

### Performance Page controls

| Control | Function |
|---|---|
| Slider | Orbit center position |
| Page buttons | Toggle per-channel orbit follow (lit = channel follows slider) |
| Shift (held) | Freeze orbit center while in beat-repeat mode |
| Shift + Page button | Navigate pages |

Step editing still works in the Performance Page (hold a Page button and turn encoders).

### Entering Performance Page Settings

From the Performance Page, hold **Fine + Glide** for another 1.5 seconds.

| Encoder | Parameter | LED |
|---|---|---|
| 1 | Quantize orbit center | Orange = on, unlit = off |
| 2 | Performance mode | Unlit = off / green = granular / blue = beat-repeat ×8 / cyan = beat-repeat ×4 |
| 3 | Granular width *or* beat-repeat debounce | Dim→bright orange / dim→bright white |
| 4 | Orbit direction (granular only) | Green / blue / orange / lavender |
| 8 | Phase Scrub Lock | Red = locked, unlit = unlocked |

**Page buttons** toggle the per-channel orbit follow mask here as well (lit = follows).

**Exit:** press **Fine + Glide** briefly or press **Play/Reset**. Settings save to flash.

### Phase Scrub Lock

**Encoder 8** in the Performance Page Settings toggles the slider lock:

- **Locked:** the slider is frozen at its current position. Encoder 8 LED is red.
- **Unlock + pickup:** turning Encoder 8 off enters pickup mode — the orbit center resumes only after the slider physically reaches the locked position, preventing jumps.
- Lock state persists across power cycles.

---

## Saving

VoltSeq saves automatically at the following moments:

- Play/Stop toggled
- Channel Edit exited (Chan.)
- Glide Step Editor exited
- Ratchet Step Editor exited
- Performance Page Settings exited
- Mode switched (shared mode saved)

There is no manual save step required.

---

## LED Reference

| Situation | Encoder LED |
|---|---|
| CV channel, normal | Scale color (chromatic = yellow, etc.) |
| Gate channel, normal | Yellow; brightness = gate length |
| Trigger channel, normal | Green (ratchet on), teal (repeat on), off (rest) |
| Channel armed | Blinks |
| Glide Step Editor active | Active channel encoder blinks |
| Ratchet Step Editor active | Active channel encoder blinks |
| Performance Page | Each encoder shows current output step color |
| Phase Scrub Lock active | Encoder 8 red |
| Phase Scrub pickup in progress | Encoder 8 blinks red |

---

## Quick Reference

| Combo | Action |
|---|---|
| Shift + Page N | Navigate to page N |
| Chan. + Page N | Arm channel N (arm/disarm) |
| Shift + Chan. | Enter Channel Edit |
| Hold Page N + turn Encoder M | Edit channel M's value at step N |
| Hold Glide + turn Encoder N | Set glide time / gate length offset / pulse width for channel N |
| Hold Shift + Glide + turn Encoder N | Offset all ratchet counts for Trigger channel N |
| Hold Glide + long-press Page N (600 ms) | Enter Glide Step Editor for channel N |
| Hold Glide + long-press Page N on Trigger (600 ms) | Enter Ratchet Step Editor for channel N |
| Fine + Glide (hold 1.5 s) | Enter/exit Performance Page |
| Shift + Tap + Chan. (hold 1 s) | Switch to VoltSeq mode |
| Fine + Play + Glide (hold 1 s) | Switch to Sequencer mode |
