# catalyst-firmware

Custom firmware for the **4ms Catalyst Sequencer** and **Catalyst Controller** Eurorack modules. Extends the stock firmware with two distinct personalities.

> **Third-party firmware.** Not supported or endorsed by 4ms Company. Flash at your own risk.

## About

This is a fork of the 4ms Company's original work on the Catalyst Sequencer / Controller firmware v1.3. I got Catalyst Sequencer back when it was released and it's been one of my favorites ever since. Since 4ms made the firmware open source back in 2025, I've been collecting ideas to iterate on the original. 

### The Objective

My initial plan was to extend the capability of the stock Sequencer mode with some things I enjoy in a Eurorack sequencer, like deeper ratchet functionality and what's effectively become a _mod matrix_ of sorts. But I've always been a huge fan of macro style CV sequencers that can replicate the functionality of Elektron style sequencing in Eurorack. So with some encouragement from some folks at Modwiggler, I decided to tackle the VoltSeq addon to bring some things I love about tabletop gear into Eurorack. I also knew the fader had lot more potential so I focused on turning it into a live recording and performance tool.

### Development

Despite my best efforts, I've never really gotten good at writing code. But writing specifications, documentation, and testing over 100 alpha builds over a marathon ten-day period is what made this possible along with a good bit of help from Claude Code. I'm not a professional developer, but I had an idea of what I wanted to build, and iterated on it until it felt right.

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

Requirements: `arm-none-eabi-gcc` v12.3, `ninja`. 
```bash
git clone https://github.com/voltagecontrolled/catalyst-firmware.git
cd catalyst-firmware
git submodule update --init --recursive
make all    # CatSeq + CatCon; WAV at build/f401/f401.wav
make test
```

The VoltSeq build requires a separate cmake invocation with `-DCATALYST_SECOND_MODE=1`.

---

- [Wiki](https://github.com/voltagecontrolled/catalyst-firmware/wiki) — user guides for both personalities
- [CHANGELOG.md](CHANGELOG.md) — full feature history with implementation notes
- [Releases](https://github.com/voltagecontrolled/catalyst-firmware/releases)
- Upstream: [4ms-company/catalyst-firmware](https://github.com/4ms/catalyst-firmware)
