# voltagecontrolled/catalyst-firmware

Custom firmware for the **4ms Catalyst Sequencer** and **Catalyst Controller** Eurorack modules. Builds on the stock 4ms firmware with two distinct personalities and a richer feature set.

> **Third-party firmware.** Not supported or endorsed by 4ms Company. Flash at your own risk.

---

## Modes

### [Catalyst Sequencer](Catalyst-Sequencer)

The primary firmware personality. Extends the stock sequencer with:

- **Phase Scrub Performance Page** — granular orbit window, beat repeat with BPM-locked subdivisions, and per-track scrub participation
- **Phase Scrub Lock** — freeze the playhead position for live performance; persists across power cycles
- **Extended ratchets and step repeats** — up to 8× ratchet subdivisions; repeats extend effective pattern length
- **Sub-step mask editor** — per-sub-step on/off within a ratcheted or repeated step
- **Linked Tracks** — CV transpose follow, CV replace follow, and gate-clock follow modes for inter-channel modulation

---

### [Catalyst VoltSeq](Catalyst-VoltSeq)

A second firmware personality sharing the same panel. Turns the module into an **8-channel voltage recorder and step sequencer** — a parameter lock style instrument inspired by the Malekko Voltage Block workflow.

- 8 independent channels, each with its own type (CV / Gate / Trigger), length, clock division, and direction
- Arm a channel and use the Phase Scrub slider to record CV in real time; encoders edit individual steps
- Per-step probability and stochastic deviation for CV, Gate, and Trigger channels
- Polyrhythmic operation — channels run independently at their own lengths and divisions

Switch between personalities without re-flashing. See [Catalyst VoltSeq](Catalyst-VoltSeq) for the switch combo.

---

## Flashing

Both personalities ship as `.wav` files in each [release](https://github.com/voltagecontrolled/catalyst-firmware/releases). Choose the build that matches what you want:

| File | Contents |
|---|---|
| `catalyst-vX.Y.Z-catseq-catcon.wav` | Catalyst Sequencer + Catalyst Controller (standard) |
| `catalyst-vX.Y.Z-catseq-voltseq.wav` | Catalyst Sequencer + Catalyst VoltSeq |

### How to flash

1. **Download** the `.wav` for the build you want from the [Releases page](https://github.com/voltagecontrolled/catalyst-firmware/releases).
2. **Enter bootloader mode:** power off the module, then hold **Shift + Fine** while powering on. Keep both held until the LEDs animate to confirm bootloader mode is active.
3. **Connect** a 3.5mm audio cable from your computer or phone headphone output to the module's **Clock In** jack.
4. **Play** the `.wav` at full volume with no EQ or compression. The module's LEDs animate during receive.
5. **Confirm:** on success, the LEDs show a success animation. Press **Play/Reset** to reboot into the new firmware. If the flash fails, the error animation plays — replay the file from the beginning.

> Use a direct cable connection. Bluetooth and USB audio add latency and compression that corrupt the signal.

---

## Resources

- [What's planned — TODO.md](https://github.com/voltagecontrolled/catalyst-firmware/blob/main/TODO.md)
- [Full change history — CHANGELOG.md](https://github.com/voltagecontrolled/catalyst-firmware/blob/main/CHANGELOG.md)
- [Releases](https://github.com/voltagecontrolled/catalyst-firmware/releases)
