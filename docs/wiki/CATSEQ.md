# Catalyst Sequencer

The voltagecontrolled fork of the Catalyst Sequencer firmware adds several capabilities to the stock 4ms firmware: an extended Phase Scrub system with performance modes, a richer gate step model with ratchets, repeats, and sub-step masks, and a Linked Tracks system for inter-channel modulation. All features are Sequencer mode only unless noted.

---

## Phase Scrub

The Phase Scrub slider controls playhead position across the pattern. The fork extends this into a full performance system with a lockable position, persistent settings, and three orbit modes.

### Phase Scrub Lock

**Combo:** COPY + GLIDE (short press — release either button before 1.5 s)

Locks the slider at its current position, preventing accidental playhead jumps during performance. The encoder 8 LED blinks red while locked.

- **Toggle:** Press COPY + GLIDE and release before 1.5 s. Holding past 1.5 s enters the Performance Page instead.
- **Pickup on unlock:** when you unlock, the slider re-engages only after it physically reaches the locked position — the playhead never jumps. Encoder 8 continues blinking during pickup and stops once the slider catches up.
- **Lock state persists across power cycles.** On boot with the lock restored, the slider enters pickup mode automatically.

Phase Scrub Lock works in all slider performance modes:

- **Standard:** playhead stays at locked position.
- **Granular:** orbit window stays centered at the locked position.
- **Beat Repeat:** active zone and looped step are both frozen — the slider has no effect.

---

### Performance Page

**Entry:** Hold **COPY + GLIDE** for 1.5 seconds.
**Exit:** **COPY + GLIDE** (any duration) or **Play/Reset**.

A dedicated performance layer for Phase Scrub. All settings persist across power cycles.

**Page buttons** toggle per-track scrub participation. Lit = track follows scrub; unlit = track ignores scrub and plays normally at the master clock.

| Encoder | Function | LED |
|---|---|---|
| 1 | Quantize | Orange = on, unlit = off |
| 2 | Slider Performance Mode | Unlit / green / blue / cyan |
| 3 | Granular Width *or* Debounce Delay | Dim→bright orange / dim→bright white |
| 4 | Orbit Direction | Green / blue / orange / lavender |
| 8 | Phase Scrub Lock | Red = locked, unlit = unlocked |

---

### Standard Phase Scrub (Mode unlit)

Default behavior. The slider continuously positions the playhead across the full pattern. Tracks excluded via page buttons play normally.

---

### Granular Mode (Mode green)

The slider positions a **looping orbit window** within the sequence. The sequencer repeats steps within that window at the normal clock rate — similar to how a granular synthesizer loops a grain of audio.

- **Enc 3 (Width):** Size of the window as a percentage of pattern length. At 0% the orbit is off and behavior falls back to standard phase scrub. At 100% the window covers the full pattern.
- **Enc 4 (Direction):** Playback direction within the window.
  - **Green** — Forward
  - **Blue** — Backward
  - **Orange** — Ping-pong (bounces between window edges)
  - **Lavender** — Random (jumps to a random step each tick)

The orbit position advances continuously. Sliding to a new center shifts the window without resetting the counter — the orbit keeps playing and the window moves underneath it. Quantize (Enc 1) snaps the window center to the nearest step boundary.

**Use case:** position the slider at a short phrase and have it loop at the clock rate. Move the slider during performance to jump to different sections. Gradually widen the window to let more of the pattern through.

---

### Beat Repeat (Mode blue — 8 zones, with triplets)

The slider selects a **rhythmic subdivision rate** at which the current orbit repeats, locked to BPM. Slide all the way left to turn beat repeat off.

| Zone | Division |
|---|---|
| 1 | 1/2 |
| 2 | 1/4 |
| 3 | 1/4T |
| 4 | 1/8 |
| 5 | 1/8T |
| 6 | 1/16 |
| 7 | 1/16T |
| 8 | 1/32 |

**Enc 3 (Debounce):** How long the slider must sit in a zone before it commits (50 ms–1500 ms, default 150 ms). A longer debounce makes it easier to swipe through zones without accidentally committing.

**Quantize (Enc 1)** controls entry timing when coming in from the off position:
- **Off:** fires immediately on SHIFT release. Your timing, your control.
- **On:** snaps to the nearest step boundary. Release anywhere near the beat and it lands cleanly.

When entering beat repeat, the loop locks to the **first step that fires**, not the step that was playing when you released SHIFT. This eliminates reaction-time lag.

**SHIFT staging:** hold SHIFT to freeze the active division and looped step — reposition the slider freely, then release SHIFT to commit the new zone on the beat without the debounce wait. Also the cleanest way to exit: hold SHIFT, slide left, release.

---

### Beat Repeat (Mode cyan — 4 zones, no triplets)

Like blue but with four wider zones — easier to target live:

| Zone | Division |
|---|---|
| 1 | 1/2 |
| 2 | 1/4 |
| 3 | 1/8 |
| 4 | 1/16 |

Enc 3 (Debounce) works the same. Use cyan when you want broad, clean division changes with minimal risk of landing on a triplet.

---

## Ratchets, Repeats, and Sub-Steps

### Extended Ratchets

Ratchet count has been extended from **4× maximum to 8× maximum**. Hold **Glide (Ratchet)** and turn a step encoder CW to set ratchet count. LED color scales from dim salmon (low) to bright red (high).

---

### Step Repeats

**Combo:** Hold GLIDE, turn step encoder CCW

A per-step mode, mutually exclusive with ratchets. Instead of subdividing a step, repeats fire the step an additional N times at the current clock division, **extending the effective pattern length**.

- An 8-step pattern with one step set to Repeat ×3 becomes 11 clock ticks long.
- Repeat count scales from dim teal (low) to bright blue (high).
- Play/Reset clears all repeat counters.

---

### Sub-Step Editor

**Combo:** GLIDE + TAP TEMPO (toggle)

A per-step mask editor for gate tracks. Lets you turn individual sub-steps within a ratchet or repeat pattern on or off, adding rhythmic variation without changing the overall step count.

**Entering and exiting:** Press GLIDE + TAP TEMPO to toggle. Play/Reset also exits.

**Focusing a step:** touch any step encoder to focus it. The focused step's sub-step mask displays on the page buttons — **lit** = sub-step fires, **unlit** = sub-step silent. The focused step encoder blinks yellow.

**Editing sub-steps:** press any page button to toggle that sub-step. Only page buttons up to the current ratchet/repeat count are active. All sub-steps can be silenced, including the first — a step with all sub-steps off produces no output for that step's duration, useful for microtiming (e.g. a 2× ratchet with only sub-step 2 active fires on the second subdivision, offsetting the hit by half a step).

**Adjusting count in edit mode:** after focusing a step, continue turning the encoder within 300 ms to adjust its ratchet or repeat count (CW = ratchet, CCW = repeat). The page buttons update in real time.

**Page navigation:** SHIFT + PAGE button changes pages without exiting edit mode.

Sub-step masks default to all sub-steps active, so steps with no mask set behave identically to stock firmware.

---

## Linked Tracks

Linked Tracks lets a CV track follow another track as a source — either for pitch transposition (CV source) or as a clock signal (gate source). Linking is set per-track and stored with the pattern.

### Entering Follow Assign Mode

1. Hold **SHIFT + CHAN** to enter Channel settings for the target CV track.
2. While holding, tap **GLIDE**. The mode stays active after you release all buttons.
3. The selected track's encoder blinks in the current sub-mode color.
4. Tap **GLIDE** again or press **Play/Reset** to exit.

Press **FINE** to cycle through sub-modes. In all sub-modes, **page buttons** assign the source track — the lit button is the current source; press an unlit button to assign it; press the lit button to unlink.

---

### CV Follow Modes

#### Blue — CV Add Follow

The source CV track's current pitch (relative to 0V) is added to the target track's pitch each update cycle, shifting the entire sequence up or down by the source value.

**Use case:** a short 2–4 step CV sequence as a transposer over a longer melodic pattern. The transposer can have its own clock division, length, and direction.

---

#### Lavender — CV Replace Follow

The target track's step values are replaced entirely by the source CV track's current output. The target outputs exactly what the source outputs, ignoring its own programmed steps.

**Use case:** temporarily override a melodic track with the output of another sequence — useful for unison lines, hard pitch coupling, or driving a track from a live CV input routed through another channel.

---

### Gate Clock Follow Modes

In gate clock follow modes, the target CV track advances its step position in response to the source gate track's events rather than the master clock.

#### Orange — Ratchets Only
The CV track advances **one step per ratchet sub-step**. Plain gate steps advance it once per step. Repeat ticks do not advance it.

#### Yellow — Repeats Only
The CV track advances **once per repeat tick**. Ratchet sub-steps do not produce additional advances. Plain gate steps advance it once per step.

#### Salmon — Ratchets + Repeats
The CV track advances on both ratchet sub-steps and repeat ticks — combines Orange and Yellow simultaneously.

#### Teal — Step Only
The CV track advances **once per gate step**, ignoring ratchets and repeats entirely. Useful when the gate track drives rhythm but you want the CV track to move at the step rate only.

---

### Combining CV Follow and Gate Clock Follow

A CV track can have both a CV follow source (Blue or Lavender) and a gate clock source (Orange/Yellow/Salmon/Teal) at the same time. The CV follow is applied on top of whatever step the CV track is currently on, regardless of what is clocking it.

---

## Tips

### Phase Scrub and performance

- Lock before a breakdown; unlock on the drop.
- In Beat Repeat, set a longer debounce (Enc 3) and use SHIFT staging for precise division drops: hold SHIFT, slide to target, release on the beat.
- Cyan mode (4-zone) is easier to play live. Blue mode (8-zone) gives triplet access but smaller targets — increase debounce to compensate.
- Exclude a track from scrub (page button unlit) to keep it as a rhythmic anchor while granular or beat repeat reshapes everything else.
- In Granular mode, Ping-pong or Random direction produces textures that don't repeat predictably even when the orbit window is stationary.
- Lock the scrub in Granular mode after dialing in an orbit position to freeze the loop while freeing your hand from the slider.

### Ratchets, repeats, and sub-steps

- Repeats are particularly effective on the last step of a pattern to create odd-length phrases that shift against other tracks.
- In the sub-step editor, setting a ratcheted step to `x o x o` produces a pattern that feels half the ratchet rate without changing the clock division.
- Mixing ratchets and repeats across steps in the same pattern produces complex polyrhythmic structures from a short base sequence.

### Linked Tracks

- In Orange mode, a CV track shorter than the gate track will cycle through its pitches faster during ratcheted steps, creating repeating melodic fragments that change phase relative to the gate pattern.
- In Yellow mode, a repeat count of ×3 on a gate step produces 4 pitches from the CV track (the step start plus 3 repeat ticks) for that single gate event.
- CV Add Follow with a 2-step source set to a root and a fifth produces automatic power-chord-style transposition of any melody it follows.
- CV Replace Follow with a source track in random playback mode creates structured randomness: program a set of target pitches on the source, leave some steps at 0V, and assign it as the replace source. The target plays its own sequence except when the random source lands on a non-zero step. The 0V steps act as pass-through, so you control both the density and the note pool of the substitutions.
