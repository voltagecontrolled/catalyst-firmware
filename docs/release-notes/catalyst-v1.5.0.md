**Third-party firmware. Not supported or endorsed by 4ms Company. Flash at your own risk.**

---

## Catalyst VoltSeq — v1.5.0

VoltSeq is a second firmware personality for the **Catalyst Sequencer** panel. It turns the module into an **8-channel voltage recorder and step sequencer** in the style of the Malekko Voltage Block — parameter lock, polyrhythmic, recordable with the Phase Scrub slider.

This release ships as a second WAV alongside the standard Catalyst Sequencer build. Both personalities live in the same firmware image; no reflashing is needed to switch between them.

See the [wiki](https://github.com/voltagecontrolled/catalyst-firmware/wiki) for full documentation.

---

### What's in this release

- **8 independent channels** — each with its own type (CV / Gate / Trigger), step length (1–64), clock division, and playback direction
- **CV channels** — stepped or slewed voltage, configurable voltage window (range + floor), optional quantizer scales, slider recording in real time
- **Gate channels** — variable gate length per step, per-step ratchet subdivisions, x0x-style step toggling
- **Trigger channels** — short pulse with configurable width, per-step ratchet (subdivide) or repeat (extend)
- **Per-step probability** — stochastic gate suppression (Gate/Trigger) or CV deviation (CV); edited per-step while armed
- **Phase Scrub Performance Page** — orbit engine and beat repeat ported from Catalyst Sequencer; per-channel follow toggle
- **External clock** — 1 pulse per 16th note at Clock In; resumes internal clock on removal
- **Intentional save** — all data written to flash only on explicit gesture (see below)

---

### Switching personalities

No reflashing needed. Both builds (CatSeq+CatCon and CatSeq+VoltSeq) ship in this release.

Within the CatSeq+VoltSeq build:

- **CatSeq → VoltSeq:** hold **Shift + Tap Tempo + Chan.** for 1 second — all 8 channel LEDs blink to confirm
- **VoltSeq → CatSeq:** hold **Fine + Play/Reset + Glide** for 1 second — all 8 channel LEDs blink to confirm

Mode is saved to flash. Switching does not erase the other mode's data.

---

### Saving

VoltSeq uses **intentional saving** — nothing is written to flash automatically (except Performance Page settings, which save on Performance Page exit).

**To save:** hold **Glide**, then press and hold **Chan.** Page buttons fast-blink during the hold, then blink 6 times to confirm. Glide must be pressed first — pressing Chan. first shows channel type colors instead and does not trigger a save.

Power-cycling without saving will lose unsaved changes.

---

### Build files

| File | Contents |
|---|---|
| `catalyst-v1.5.0-catseq-catcon.wav` | Catalyst Sequencer + Catalyst Controller |
| `catalyst-v1.5.0-catseq-voltseq.wav` | Catalyst Sequencer + Catalyst VoltSeq |

See the [wiki](https://github.com/voltagecontrolled/catalyst-firmware/wiki) for flashing instructions.
