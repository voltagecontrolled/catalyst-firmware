**Note: this is a pre-release firmware, use at your own risk. This is not supported or endorsed by 4ms Company and flashing third party firmware could void your warranty or render your module inoperable. The author assumes no liability for any damage resulting from use of this firmware.**

---

## VoltSeq Mode (v1.5.0)

VoltSeq is a new firmware personality for the Catalyst Sequencer panel. It turns the module into an **8-channel voltage recorder and step sequencer**, similar to Voltage Block.

**Load:** flash `catalyst-v1.5.0-alpha8-catseq-voltseq.wav` via the audio-cable bootloader.

**Switch between CatSeq and VoltSeq without re-flashing:**
- CatSeq → VoltSeq: hold **Shift + Tap Tempo + Chan.** for 1 second
- VoltSeq → CatSeq: hold **Fine + Play/Reset + Glide** for 1 second

Mode is saved to flash. Switching does not erase the other mode's data.

### Channels

8 independent channels, each with its own output jack and encoder. Each channel has a **type** (set in Channel Edit, encoder 7):

| Type | Output | Notes |
|------|--------|-------|
| **CV** | Stepped or slewed voltage | Configurable range (default ±5V); optional quantizer scale |
| **Gate** | Gate signal | Variable length per step (0–100%) |
| **Trigger** | Short pulse | Per-step ratchet (subdivide) or repeat (extend) counts |

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

### Global Settings (Shift held 1.5 s alone)

Hold Shift by itself for 1.5 seconds to enter. Any other button press during the hold cancels. Exit with Play/Reset.

| Encoder | Parameter |
|---------|-----------|
| 1 — Start | Play/Stop reset mode (red = on: Stop also resets all channels to step 1) |
| 3 — Length | Master reset steps (0 = off; all channels reset every N master ticks; red at 8/16/32/64) |
| 6 — BPM/Clock Div | Internal BPM (yellow pulse; white at 80/100/120/140 BPM) |

**Page buttons** select a reset leader channel (radio). When the leader channel completes its sequence, all other channels reset with it. Master reset takes priority if set.

### Voltage recording

Arm a channel with **Chan. + Page button N**. The Phase Scrub slider records voltage in real time:
- While playing: slider value is written to the current step on every clock tick
- While stopped: slider continuously sets the last-touched step

### Performance Page

Hold **Fine + Glide** for 1.5 seconds. The slider controls an orbit engine that manipulates playback positions across channels. Page buttons toggle per-channel orbit follow.

### Saving

VoltSeq saves automatically when Play/Stop is toggled or any editor is exited. Settings changed while playing are deferred to the next Play/Stop press.
