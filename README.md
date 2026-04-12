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

Pre-built `.wav` files are attached to each [release](https://github.com/voltagecontrolled/catalyst-firmware/releases):

| File | Contents |
|---|---|
| `catalyst-vX.Y.Z-catseq-catcon.wav` | Catalyst Sequencer + Catalyst Controller |
| `catalyst-vX.Y.Z-catseq-voltseq.wav` | Catalyst Sequencer + Catalyst VoltSeq |

See the [wiki](https://github.com/voltagecontrolled/catalyst-firmware/wiki) for flashing instructions.

---

## Building from source

Requirements: `arm-none-eabi-gcc` v12.3, `ninja`. See [CLAUDE.md](CLAUDE.md) for full build and development notes.

```bash
git clone https://github.com/voltagecontrolled/catalyst-firmware.git
cd catalyst-firmware
git submodule update --init --recursive
make all    # CatSeq + CatCon; WAV at build/f401/f401.wav
make test
```

The VoltSeq build requires a separate cmake invocation with `-DCATALYST_SECOND_MODE=1`. See [CLAUDE.md](CLAUDE.md) for details.

---

- [Wiki](https://github.com/voltagecontrolled/catalyst-firmware/wiki) — user guides for both personalities
- [CHANGELOG.md](CHANGELOG.md) — full feature history with implementation notes
- [Releases](https://github.com/voltagecontrolled/catalyst-firmware/releases)
- Upstream: [4ms-company/catalyst-firmware](https://github.com/4ms/catalyst-firmware)
