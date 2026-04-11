**Note: this is a pre-release firmware. Not supported or endorsed by 4ms Company. Flashing third-party firmware could void your warranty or render your module inoperable. The author assumes no liability for any damage resulting from use of this firmware.**

---

## VoltSeq Mode (pre-release / active development)

VoltSeq is a firmware personality for the **Catalyst Sequencer** panel. It turns the module into an **8-channel voltage recorder and step sequencer**, similar to Voltage Block.

Flash the `.wav` file via the audio-cable bootloader.

### Switching modes (no re-flash required)

- **CatSeq → VoltSeq:** hold **Shift + Tap Tempo + Chan.** for 1 second
- **VoltSeq → CatSeq:** hold **Fine + Play/Reset + Glide** for 1 second

Mode is saved to flash. Switching does not erase the other mode's data.

---

### Channels

8 independent channels, each with its own output jack and encoder. Each channel has a **type** (set in Channel Edit, encoder 7):

| Type | Output |
|------|--------|
| **CV** | Stepped or slewed voltage; configurable range (default ±5V); optional quantizer scale |
| **Gate** | Gate signal with variable length per step (0–100%); per-step ratchet subdivisions |
| **Trigger** | Short trigger pulse; per-step ratchet (subdivide) or repeat (extend) |

---

### Clock

- **External clock:** patch into **Clock In** at 1 pulse per 16th note. VoltSeq locks automatically; removing the cable resumes the internal clock at the same tempo.
- **Internal clock:** tap **Tap Tempo** (3+ taps to set BPM). Fine-tune in Global Settings (encoder 6).
- **Reset jack:** resets all channels to step 1 and fires immediately.

---

### Recording

Arm a channel with **Chan. + Page button N**. The Phase Scrub slider records CV in real time on armed CV channels. Encoders edit individual steps while armed on any channel type.

---

### Channel Edit (Shift + Chan. — short tap)

Focus a channel with a page button, then use encoders:

| Encoder | Panel label | Parameter |
|---------|-------------|-----------|
| 1 | Start | Output delay (0–20 ms) |
| 2 | Dir. | Direction: Forward / Reverse / Ping-Pong / Random |
| 3 | Length | Step length (1–64 steps) |
| 4 | Phase | Phase rotate (destructive) |
| 5 | Range | Voltage range (CV) or pulse width ms (Trigger) |
| 6 | BPM/Clock Div | Per-channel clock division (enc 0 = quarter note … enc 7 = quadruple whole) |
| 7 | Transpose | Channel type and quantizer scale |
| 8 | Random | Random amount (0–100%) |

Exit with **Shift + Chan.** or **Play/Reset**.

---

### Global Settings (Shift + Chan. — hold 2 s)

| Encoder | Parameter |
|---------|-----------|
| 1 — Start | Play/Stop reset mode (red = on: Stop also resets all channels to step 1) |
| 6 — BPM/Clock Div | Internal BPM |

Exit with **Play/Reset**.

---

### Performance Page (Fine + Glide — hold 1.5 s)

The Phase Scrub slider controls an orbit engine that manipulates playback positions. Page buttons toggle per-channel orbit follow. Beat repeat available in blue/cyan modes. Exit with **Play/Reset**.

---

### Saving

VoltSeq saves automatically on **Stop** and when **Global Settings** or the **Performance Page** are exited. Exiting **Channel Edit** while playing defers the save to the next Stop — power-cycling before stopping will lose any changes made in that session.
