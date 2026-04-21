**Third-party firmware. Not supported or endorsed by 4ms Company. Flash at your own risk.**

---

## Catalyst VoltSeq — v1.5.1

UX refinements for VoltSeq mode. **Saved state is not compatible with v1.5.0** — all channels will reset to defaults on first boot after flashing.

---

### Changes

#### Range simplified to four options
CV channel range (enc 4 in Channel Edit) now offers four spans: **2V / 5V / 10V / 15V**. The previous seven-step ladder (1–4V in 1V increments plus 5/10/15V) had too many options with too little practical difference in the lower end. Default is 5V. Colors: orange (2V), yellow (5V), green (10V), blue (15V).

#### Transpose display: red / blue
Transpose (enc 6) now uses a clean red ↔ blue gradient: dim-red at −1V ramping to bright-red at −5V, grey at 0V, dim-blue at +1V ramping to bright-blue at maximum. Previous mixed warm/cool colors (orange, pink, lavender, etc.) are replaced.

#### Random deviation in semitones
The random deviation encoder (enc 7 in Channel Edit) now steps in **semitones** instead of whole volts, with a maximum of **±3 octaves (36 semitones)**. Unipolar (CW): light grey gradient; bipolar (CCW): salmon gradient. Octave boundaries (12, 24, 36 semitones) snap to blue (CW) or red (CCW) for easy landmark recognition.

#### Peek-before-edit for Length and Clock Div
Both **enc 2 (Length)** and **enc 5 (Clock Div)** now require one "peek" detent before editing begins. The first turn of either encoder shows the current value on the LEDs without changing it; a second turn within the 900 ms display window starts adjusting. After the window expires, the next turn is another peek. This makes it easy to check a setting without accidentally changing it.

#### Clear and init defaults: 0V / gates off
Cleared CV channels now default to **0V** (bottom of the window at default transpose) instead of 2.5V. Gate and Trigger channels are already initialized to off; this remains unchanged. Applies to both partial clear (page button in Clear Mode) and full reset (Shift + page button).

---

### Build files

| File | Contents |
|---|---|
| `catalyst-v1.5.1-catseq-catcon.wav` | Catalyst Sequencer + Catalyst Controller |
| `catalyst-v1.5.1-catseq-voltseq.wav` | Catalyst Sequencer + Catalyst VoltSeq |
