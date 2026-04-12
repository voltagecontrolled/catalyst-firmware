# VoltSeq Cheat Sheet

Quick reference for Catalyst VoltSeq. See [Catalyst VoltSeq](Catalyst-VoltSeq) for full documentation.

---

## Main Mode

*Default state on boot and after exiting any modal.*

| Action | Result |
|---|---|
| **Play/Reset** | Toggle play / stop |
| **Shift + Play** (tap, < 600 ms) | Reset all channels to step 1 |
| **Hold Page N + turn Enc M** | Edit channel M step at position N |
| **Hold multiple Page buttons** | Edit same enc across all held steps simultaneously |
| **Turn Enc M** (no page held) | Focus chaselight on channel M (no step edited) |
| **Shift + Page N** | Navigate to page N |
| **Hold Chan.** | Encoder LEDs show channel type colors (CV = grey, Gate = green, Trigger = green/teal) |
| **Chan. + Page N** | Arm channel N |

**CV step editing:** ~1 semitone per detent. Hold **Fine** for sub-semitone. Fast spin accelerates.  
**Gate step editing:** gate length 0–100%. **Trigger step editing:** ratchet/repeat count.

---

## Channel Edit

*Entry: **Shift + Chan.** (short tap — release before 1 s)*  
*Exit: **Shift + Chan.** or **Play/Reset***

Press a **Page button** to focus a channel. Chaselight shows that channel's playhead.

| Enc | Panel label | Parameter |
|---|---|---|
| 1 | Start | Output delay (0–20 ms) |
| 2 | Dir. | Direction: Forward / Reverse / Ping-Pong / Random |
| 3 | Length | Step length 1–64 |
| 4 | Phase | Phase rotate (destructive) |
| 5 | Range | Voltage span (CV) / Pulse width ms (Trigger) |
| 6 | BPM/Clock Div | Per-channel clock division |
| 7 | Transpose | Floor voltage (CV only) |
| 8 | Random | CV deviation range: CW = unipolar (+N V), CCW = bipolar (±N V), 0 = off |

**Shift held:** Page buttons show current page indicator instead of chaselight.

---

## Armed Mode

*Entry: **Chan. + Page N***  
*Exit: **Chan. + Page N** again, or **Play/Reset***

**CV channel**

| Action | Result |
|---|---|
| **Enc N** | Edit step N CV value |
| **Fine + Enc N** | Sub-semitone editing |
| **Slider** | Record CV into current step (motion-gated) |
| **Shift + Enc 5** | Adjust voltage span (Range) |
| **Shift + Enc 7** | Adjust floor voltage (Transpose) |

**Gate channel**

| Action | Result |
|---|---|
| **Tap Page N** | Toggle step N on / off |
| **Enc N** | Adjust gate length for step N |

**Trigger channel**

| Action | Result |
|---|---|
| **Tap Page N** | Toggle step N between rest and single trigger |
| **Enc N CW** | Increase ratchet count (subdivide, 1–8×) |
| **Enc N CCW** | Increase repeat count (extend, 1–8×) |

---

## GLIDE Modifier

*Hold **Glide** (unarmed or armed)*

**Unarmed — channel-level adjustments**

| Channel type | Action | Result |
|---|---|---|
| CV | **Glide + Enc N** | Glide time for channel N (0–10 s) |
| Gate | **Glide + Enc N** | Offset all gate lengths for channel N |
| Gate | **Glide + hold Page N + Enc M** | Set ratchet count for step N on channel M |
| Trigger | **Glide + Enc N** | Pulse width for channel N (1–100 ms) |
| Trigger | **Shift + Glide + Enc N** | Offset all ratchet/repeat counts for channel N |

**Armed — per-step glide time (CV)**

| Action | Result |
|---|---|
| **Glide + Enc N** | Set glide time for step N; encoder LEDs show brightness = glide amount |
| **Glide + Enc N CCW to minimum** | Disable glide for step N |

**Armed — per-step ratchet count (Gate)**

| Action | Result |
|---|---|
| **Glide + Enc N** | Set ratchet count for step N; encoder LEDs show ratchet counts |

---

## Probability / Random

*Hold **Shift + Glide** while armed*

Encoder LEDs show a **violet → grey → white** ramp — violet = 0% (always fires), white = 100%.

| Action | Result |
|---|---|
| **Shift + Glide + Enc N CW** | Increase probability for step N |
| **Shift + Glide + Enc N CCW** | Decrease probability; fully CCW = 0 (always fires) |

**Gate / Trigger:** non-zero probability suppresses the step's output randomly.  
**CV:** non-zero probability applies a random voltage offset. Deviation range and polarity set in Channel Edit enc 8.

---

## Performance Page

*Entry: hold **Fine + Glide** for 1.5 s*  
*Exit: **Play/Reset** (saves performance settings automatically)*

| Control | Function |
|---|---|
| **Slider** | Orbit center position |
| **Page N** | Toggle channel N orbit follow on/off |
| **Shift held** | Freeze orbit center (beat-repeat staging) |
| **Shift + Page N** | Navigate pages |

Settings shown on entry (encoder LEDs):

| Enc | Parameter | LED |
|---|---|---|
| 1 | Quantize orbit center | Orange = on |
| 2 | Mode | Off / green (granular) / blue (beat-repeat ×8) / cyan (beat-repeat ×4) |
| 3 | Granular width *or* beat-repeat debounce | Dim→bright orange / dim→bright white |
| 4 | Orbit direction (granular only) | Green / blue / orange / lavender |
| 8 | Phase Scrub Lock | Red = locked; blinks red during pickup |

**Short Fine + Glide** (release before 1.5 s): toggle Phase Scrub Lock from main mode or within Performance Page.

---

## Clear & Save

### Clear Mode

*Entry: hold **Fine + Play** for ~500 ms — page buttons fast-blink to confirm*  
*Exit: **Play/Reset***

| Action | Result |
|---|---|
| **Page N** | Clear steps + glide + probability for channel N (preserves channel settings) |
| **Shift + Page N** | Full factory reset for channel N (steps + flags + all channel settings) |

Multiple channels can be cleared before exiting.

### Saving

*No data is written to flash automatically except Performance Page settings.*

**Save gesture:** hold **Glide**, then press and hold **Chan.** — page buttons fast-blink during hold, then blink 6× to confirm.

> **Glide must be pressed first.** Pressing Chan. first shows channel type colors instead and does not save.

---

## Global Settings

*Entry: hold **Shift + Chan.** for 1 s*  
*Exit: **Play/Reset***

| Enc | Panel label | Parameter | LED |
|---|---|---|---|
| 1 | Start | Play/stop reset mode | Red = on (Stop also resets all channels) |
| 3 | Length | Master loop length (0 = off; all channels reset together every N 16th-note steps) | Off / orange / red at bar-aligned values (8, 16, 32, 48, 64) |
| 6 | BPM/Clock Div | Internal BPM | Color zone: red < 50 / orange 50–79 / yellow 80–99 / green 100–119 / blue 120–149 / teal 150–179 / lavender 180+ |

---

## Mode Switch

| Combo | Result |
|---|---|
| Hold **Shift + Tap Tempo + Chan.** for 1 s | CatSeq → VoltSeq |
| Hold **Fine + Play/Reset + Glide** for 1 s | VoltSeq → CatSeq |

Mode is saved to flash. Switching does not erase the other mode's data.
