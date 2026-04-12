# catalyst-firmware

Custom firmware for the **4ms Catalyst Sequencer** and **Catalyst Controller** Eurorack modules. Extends the stock firmware with two distinct personalities.

> **Third-party firmware.** Not supported or endorsed by 4ms Company. Flash at your own risk.

---

## Personalities

### Catalyst Sequencer

Extended sequencer personality for the Catalyst Sequencer panel. Builds on the stock 4ms firmware with Phase Scrub performance tools, ratchets and step repeats, sub-step mask editing, and linked track follow modes.

### Catalyst VoltSeq

A second personality for the same panel. Turns the module into an 8-channel voltage recorder and step sequencer — parameter lock style, inspired by the Malekko Voltage Block workflow. 8 independent channels, each with its own type (CV / Gate / Trigger), length, clock division, and direction. Polyrhythmic, stochastic, recordable with the Phase Scrub slider.

Switch between personalities without reflashing. See the [wiki](https://github.com/voltagecontrolled/catalyst-firmware/wiki) for the switch combo.

---

## Releases

Pre-built `.wav` files for the audio-cable bootloader are attached to each [release](https://github.com/voltagecontrolled/catalyst-firmware/releases):

| File | Contents |
|---|---|
| `catalyst-vX.Y.Z-catseq-catcon.wav` | Catalyst Sequencer + Catalyst Controller |
| `catalyst-vX.Y.Z-catseq-voltseq.wav` | Catalyst Sequencer + Catalyst VoltSeq |

### Flashing

1. **Download** the `.wav` for the build you want from the [Releases page](https://github.com/voltagecontrolled/catalyst-firmware/releases).
2. **Enter bootloader:** power off, then hold **Shift + Fine** while powering on. Keep both held until the LEDs animate.
3. **Connect** a 3.5mm cable from your computer's headphone output to the module's **Clock In** jack.
4. **Play** the `.wav` at full volume with no EQ or compression. LEDs animate during receive.
5. On success the LEDs show a success animation. Press **Play/Reset** to reboot. If it fails, replay from the beginning.

> Use a direct cable. Bluetooth and USB audio add latency that corrupts the signal.

---

## Building from source

Requirements: `arm-none-eabi-gcc` v12.3, `ninja`

```bash
git clone https://github.com/voltagecontrolled/catalyst-firmware.git
cd catalyst-firmware
git submodule update --init --recursive

# Catalyst Sequencer + Catalyst Controller (default)
make all
make wav   # outputs build/f401/f401.wav

# Catalyst Sequencer + Catalyst VoltSeq
cmake -B build_voltseq -GNinja -DCATALYST_SECOND_MODE=CATALYST_MODE_VOLTSEQ .
cmake --build build_voltseq
cmake --build build_voltseq --target f401.wav  # outputs build_voltseq/f401/f401.wav

# Unit tests
make test

# Flash via JLink
make jflash-app
```

---

## Documentation

- [Wiki](https://github.com/voltagecontrolled/catalyst-firmware/wiki) — user-facing guides for both personalities
- [CHANGELOG.md](CHANGELOG.md) — full feature history with implementation notes
- [TODO.md](TODO.md) — backlog for both modes
- [Releases](https://github.com/voltagecontrolled/catalyst-firmware/releases)

---

## Preset compatibility

`sizeof(Step)` was expanded from 4 to 8 bytes in v1.4.2. Presets saved under upstream v1.3 are not compatible and will be discarded on first boot. The firmware detects the mismatch via a version tag and resets to defaults automatically — no manual factory reset needed.

---

Upstream project: [4ms-company/catalyst-firmware](https://github.com/4ms/catalyst-firmware)
