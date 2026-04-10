**Note: this is a pre-release firmware. Not supported or endorsed by 4ms Company. Flashing third-party firmware could void your warranty or render your module inoperable. The author assumes no liability for any damage resulting from use of this firmware.**

---

## VoltSeq Mode (v1.5.0-alpha29)

VoltSeq is a firmware personality for the Catalyst Sequencer panel. It turns the module into an **8-channel voltage recorder and step sequencer**, similar to Voltage Block.

**Load:** flash `catalyst-v1.5.0-alpha29-catseq-voltseq.wav` via the audio-cable bootloader.

**Switch between CatSeq and VoltSeq without re-flashing:**
- CatSeq → VoltSeq: hold **Shift + Tap Tempo + Chan.** for 1 second
- VoltSeq → CatSeq: hold **Fine + Play/Reset + Glide** for 1 second

Mode is saved to flash. Switching does not erase the other mode's data.

---

### Channels

8 independent channels, each with its own output jack and encoder. Each channel has a **type** (set in Channel Edit, encoder 7):

| Type | Output | Notes |
|------|--------|-------|
| **CV** | Stepped or slewed voltage | Configurable range (default ±5V); optional quantizer scale |
| **Gate** | Gate signal | Variable length per step (0–100%); per-step ratchet subdivisions |
| **Trigger** | Short pulse | Per-step ratchet (subdivide) or repeat (extend) |

---

### Clock

- **External clock:** patch into **Clock In** at 1 pulse per 16th note (16 PPQN). VoltSeq locks to it automatically. When the cable is removed, the internal clock resumes at the same tempo.
- **Internal clock:** tap **Tap Tempo** (3+ taps). Fine-tune in Global Settings (encoder 6).
- **Reset jack:** resets all channels to step 1 and fires immediately — in sync from the moment the pulse arrives.

---

### Per-channel settings (Channel Edit — Shift + Chan.)

Press a page button to focus a channel, then use encoders:

| Encoder | Parameter |
|---------|-----------|
| 1 — Start | Output delay (0–20 ms) |
| 2 — Dir. | Direction: Forward / Reverse / Ping-Pong / Random |
| 3 — Length | Step length (1–64 steps) |
| 4 — Phase | Phase rotate (destructive) |
| 5 — Range | Voltage range (CV) or pulse width (Trigger) |
| 6 — BPM/Clock Div | Per-channel clock division |
| 7 — Transpose | Channel type and quantizer scale |
| 8 — Random | Random amount (0–100%) |

---

### Global Settings (Shift + Chan. held 2 s)

Exit with Play/Reset.

| Encoder | Parameter |
|---------|-----------|
| 1 — Start | Play/Stop reset mode (red = on: Stop also resets all channels to step 1) |
| 6 — BPM/Clock Div | Internal BPM (color zone: red <50, orange 50–79, yellow 80–99, green 100–119, blue 120–149, teal 150–179, lavender 180+) |

**Page buttons** select a reset leader channel (radio; tap to set, tap again to clear). When the leader channel completes its sequence and wraps, all channels snap to step 1 and fire immediately.

---

### Voltage recording

Arm a CV channel with **Chan. + Page button N**. The Phase Scrub slider records voltage in real time:
- While playing: slider value written to the current step on every clock tick
- While stopped: slider continuously sets the last-touched step

---

### Performance Page

Hold **Fine + Glide** for 1.5 seconds. The slider controls an orbit engine that manipulates playback positions. Page buttons toggle per-channel orbit follow. Beat repeat available in blue/cyan modes.

---

### Saving

VoltSeq saves automatically when Play/Stop is toggled or any editor is exited.
