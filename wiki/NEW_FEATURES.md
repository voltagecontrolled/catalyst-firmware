# New Features - voltagecontrolled/catalyst-firmware

This page covers features added beyond the stock 4ms Catalyst Sequencer firmware. All features are Sequencer mode only unless noted.

---

## Phase Scrub Lock (v1.4.1)

**Combo:** COPY + GLIDE

Locks the Phase Scrub slider at its current position, preventing accidental playhead jumps during performance.

- Press **COPY + GLIDE** to lock. Encoder 8 LED blinks red while locked.
- Press **COPY + GLIDE** again to unlock. The slider uses **pickup behavior** - it resumes control only after physically reaching the locked position, so the playhead never jumps on unlock.
- Encoder 8 continues blinking during pickup; stops once the slider catches up.

---

## Extended Ratchets (v1.4.2)

Ratchet count on gate tracks has been extended from **4x maximum to 8x maximum**.

Everything else about ratchet behavior is unchanged. Hold **Glide (Ratchet)** and turn a step encoder CW to set ratchet count. LED color scales from dim salmon (low) to bright red (high).

---

## Step Repeats (v1.4.2)

**Combo:** Hold GLIDE, turn step encoder CCW

A new per-step mode, mutually exclusive with ratchets. Instead of subdividing a step, repeats fire the step an additional N times at the current clock division, **extending the effective pattern length**.

- An 8-step pattern with one step set to Repeat ×3 becomes 11 clock ticks long.
- Repeat count scales from dim teal (low) to bright blue (high).
- Play/Reset clears all repeat counters.

---

## Sub-Step Editor (v1.4.3)

**Combo:** GLIDE + TAP TEMPO (toggle)

A per-step mask editor for gate tracks. Lets you turn individual sub-steps within a ratchet or repeat pattern on or off, adding rhythmic variation without changing the overall step count.

### Entering and Exiting
- Press **GLIDE + TAP TEMPO** to enter sub-step edit mode.
- Press **GLIDE + TAP TEMPO** again to exit. **Play/Reset** also exits.

### Focusing a Step
- Touch any step encoder to focus it.
- The focused step's sub-step mask displays on the **Page buttons**:
  - **Lit** = sub-step fires
  - **Unlit** = sub-step silent
- Focus stays on that step until you touch another encoder or exit the mode.
- The focused step encoder **blinks yellow**.

### Editing Sub-Steps
- Press any **Page button** to toggle that sub-step on or off.
- Only Page buttons up to the current ratchet/repeat count are active. Remaining buttons are unlit and non-interactive.
- All sub-steps can be silenced, including the first. A step with all sub-steps off produces no output for that step's duration -- useful for microtiming (e.g. a 2x ratchet with only sub-step 2 active fires on the second subdivision, offsetting the hit by half a step).

### Adjusting Ratchet/Repeat Count in Edit Mode
- After focusing a step, **continue turning the encoder within 300ms** to adjust its ratchet or repeat count:
  - **CW** - increase ratchet count (red feedback)
  - **CCW** - increase repeat count (blue feedback)
- The Page buttons update in real time as the count changes - sub-steps expand or contract accordingly.
- After 300ms of no turning, the next encoder touch re-focuses without changing the count.

### Default Behavior
Sub-step masks default to all sub-steps firing, so steps with no mask set behave identically to stock firmware.

---

## Linked Tracks (v1.4.4)

Linked Tracks lets a CV track follow another track as a source - either for pitch transposition (CV source) or as a clock signal (gate source). Linking is set per-track and stored with the pattern.

### Entering Follow Assign Mode

1. Hold **SHIFT + CHAN** to enter Channel settings for the desired CV track.
2. While holding those buttons, tap **GLIDE**. The mode is now active and stays active after you release all buttons.
3. The selected track's encoder blinks in the current sub-mode color.
4. Tap **GLIDE** again or press **Play/Reset** to exit.

Press **FINE** to cycle through the four sub-modes described below.

### Page Button Assignment

In all sub-modes, the **Page buttons** act as a radio selector for the source track:

- **Lit button** = currently assigned source track.
- **Press an unlit button** to assign that track as the source.
- **Press the lit button** to unlink (no source).
- Pressing the button for the current track itself has no effect.

---

### Blue - CV Transpose Follow

The selected CV track follows another CV track as a transpose source. The source track's current pitch (relative to 0V) is added to the target track's pitch each update cycle, shifting the entire pitch sequence up or down by the source value.

**Use case:** run a short 2-4 step CV sequence as a transposer over a longer melodic pattern. The transposer sequence can have its own clock division, length, and direction settings, shifting the melody by different intervals each cycle.

---

### Orange - Gate Clock Follow (Ratchets Only)

The selected CV track uses a gate track as its clock source instead of the master clock. The CV track advances **one step per ratchet sub-step** on the gate track.

- Each ratchet sub-step on the gate track produces a different pitch on the CV track.
- Repeat ticks on the gate track do **not** advance the CV track (only the first tick of the step does).
- Plain gate steps (no ratchet) advance the CV track once per step, same as the master clock.

**Use case:** a ratcheted gate track drives rapid pitch sequences on the CV track, creating melodic ratchets that advance at ratchet speed. The CV track gets ahead of the gate track when ratchets are present.

---

### Yellow - Gate Clock Follow (Repeats Only)

The selected CV track advances **once per repeat tick** on the gate track.

- Each repeat tick on the gate track advances the CV track one step, producing a new pitch on each repeat.
- Ratchet sub-steps on the gate track do **not** produce additional CV advances (only the step start does).
- Plain gate steps advance the CV track once per step.

**Use case:** a gate track with multi-tick repeats drives a slower-moving pitch sequence, where each repetition of the gate sound gets a different pitch - similar to a manual arp or a pitch stutter effect.

---

### Salmon - Gate Clock Follow (Ratchets + Repeats)

The selected CV track advances on **both** ratchet sub-steps and repeat ticks.

- Ratchet sub-steps each advance the CV track (same as Orange).
- Repeat ticks each advance the CV track (same as Yellow).
- Combines both effects simultaneously.

**Use case:** complex gate patterns with mixed ratchets and repeats drive maximally varied pitch movement on the CV track.

---

### Combining Transpose and Clock Follow

A CV track can have both a transpose source (Blue) and a gate clock source (Orange/Yellow/Salmon) assigned at the same time. The transpose offset is applied on top of whatever step the CV track is currently on, regardless of what is clocking it.

### Known Quirk: Switching Gate Clock Modes

When a gate track is assigned as a clock source in one mode (e.g. Orange) and you press FINE to switch to a different gate clock mode (e.g. Yellow), the page button for the previously assigned track remains lit. The assignment shown is still the Orange-mode assignment - switching modes does not automatically re-assign or clear it.

To change the active clock follow mode for an already-linked track:
1. Press the lit page button to unlink.
2. Press the same button again to re-assign under the new mode.

This will be improved in a future update so that switching modes automatically re-applies the current assignment under the new mode and clears it from the old one.

---

## Tips

- Phase Scrub Lock is most useful mapped to a performable moment - lock before a breakdown, unlock on the drop.
- Repeats are particularly effective on the last step of a pattern to create odd-length phrases that shift against other tracks.
- In the sub-step editor, setting a ratcheted step to `x o x o` produces a pattern that feels half the ratchet rate without changing the clock division.
- Mixing ratchets and repeats across steps in the same pattern produces complex polyrhythmic structures from a short base sequence.
- In Orange mode, a CV track with a shorter length than the gate track will cycle through its pitches faster during ratcheted steps, creating repeating melodic fragments that change phase relative to the gate pattern.
- In Yellow mode, a repeat count of ×3 on a gate step produces 4 pitches from the CV track (the step start plus 3 repeat ticks) for that single gate event.
- Transpose Follow with a 2-step CV source set to a root and a fifth produces automatic power-chord-style transposition of any melody it follows.
