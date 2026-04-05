# New Features - voltagecontrolled/catalyst-firmware

This page covers features added beyond the stock 4ms Catalyst Sequencer firmware. All features are Sequencer mode only unless noted.

> **v1.4.6 is a pre-release.** Features in this section are in testing and may change before a stable release.

---

## Phase Scrub Performance Page (v1.4.6 pre-release)

**Entry:** Hold **COPY + GLIDE** for 1.5 seconds.  
**Exit:** **COPY + GLIDE** (any duration) or **Play/Reset**.

A dedicated performance page for Phase Scrub. All settings are stored and survive reboots.

### Page controls at a glance

| Encoder | Function | LED |
|---------|----------|-----|
| 1 | Quantize | Orange = on, unlit = off |
| 2 | Slider Performance Mode | Unlit / green / blue / cyan |
| 3 | Granular Width *or* Debounce Delay | Dim→bright orange / dim→bright white |
| 4 | Orbit Direction | Green / blue / orange / lavender |
| 8 | Phase Scrub Lock | Red = locked, unlit = unlocked |

**Page buttons:** Toggle per-track scrub participation. Lit = track follows scrub; unlit = track ignores scrub and plays normally.

---

### Quantize (Enc 1)

Snaps the phase scrub position to the nearest step boundary. The playhead jumps cleanly between steps instead of continuously scrubbing. Particularly useful with Granular mode.

---

### Slider Performance Mode (Enc 2)

Selects what the Phase Scrub slider does in real time. Turn CW to step through modes, CCW to go back.

#### Unlit — Standard Phase Scrub

Default behavior. The slider continuously positions the playhead across the full pattern.

---

#### Green — Granular Sequencing

The slider positions a **looping orbit window** within the sequence. The sequencer repeats steps within that window at the normal clock rate, similar to how granular synthesis loops a grain of audio.

- **Enc 3 (Width):** Size of the window, as a percentage of pattern length. At 0% the orbit is off and the slider falls back to standard phase scrub. At 100% the orbit covers the full pattern. LED scales from unlit (0%) to bright orange (100%).
- **Enc 4 (Direction):** Playback direction within the window. Turn CW to advance through modes, CCW to go back. Enc 4 is unlit and inactive in all other modes.
  - **Green** — Forward (left to right through the window)
  - **Blue** — Backward
  - **Orange** — Ping-pong (bounces between window edges)
  - **Lavender** — Random (jumps to a random step in the window each tick)

The orbit position counter advances continuously. Sliding to a new center moves the window without resetting the counter — the orbit keeps playing uninterrupted, and the window shifts underneath it. Quantize (Enc 1) snaps the window center to the nearest step boundary.

Tracks excluded from scrub via the page buttons continue normal clock-driven playback regardless of orbit width or direction.

**Use case:** position the slider at a short phrase within a longer sequence and have it loop at the pattern's clock rate. Move the slider during performance to jump to different sections. Gradually widen the window to let more of the pattern through.

---

#### Blue — Beat Repeat (8 zones, with triplets)

The slider selects a **rhythmic subdivision rate** at which the current orbit repeats. The beat repeat fires independently of the master clock, locked to BPM. Slide all the way left to turn beat repeat off.

The slider is divided into 8 equal zones:

| Zone | Division | Feel |
|------|----------|------|
| 1 | 1/2 | Half note |
| 2 | 1/4 | Quarter note |
| 3 | 1/4T | Quarter triplet |
| 4 | 1/8 | Eighth note |
| 5 | 1/8T | Eighth triplet |
| 6 | 1/16 | Sixteenth note |
| 7 | 1/16T | Sixteenth triplet |
| 8 | 1/32 | Thirty-second |

- **Enc 3 (Debounce):** How long the slider must sit in a zone before it commits. LED scales dim→bright white. Default is 150ms; range is 50ms–1500ms. A longer debounce makes it easier to swipe through zones without accidentally committing. This setting is transient and resets on reboot.
- **Enc 4:** Unlit and inactive in beat repeat modes.

**Use case:** hold the slider on 1/8 during a breakdown for a stuttering repeat effect. Slide right for increasingly rapid subdivisions. Set Width > 0 (using Granular enc 3 before switching to beat repeat) to cycle through a small phrase at the repeat rate rather than hammering a single step.

---

#### Cyan — Beat Repeat (4 zones, no triplets)

Like Blue but with four wider zones — easier to target by hand during performance:

| Zone | Division | Feel |
|------|----------|------|
| 1 | 1/2 | Half note |
| 2 | 1/4 | Quarter note |
| 3 | 1/8 | Eighth note |
| 4 | 1/16 | Sixteenth note |

Enc 3 (Debounce) works the same as in Blue mode. Enc 4 is unlit and inactive. Use Cyan when you want broad, clean division changes with minimal risk of landing on a triplet by accident.

---

### Staging a division change with SHIFT

In either beat repeat mode, **hold SHIFT** to freeze the currently active division — the slider can be repositioned freely without committing to the new zone. When you release SHIFT, whatever zone the slider is in commits immediately (no debounce wait).

This lets you pre-position the slider at your target division and then drop in exactly on the beat by releasing SHIFT.

---

### Phase Scrub Lock in performance modes

Phase Scrub Lock (Enc 8, or COPY + GLIDE short press) works in all slider performance modes. When locked:

- **Granular:** orbit window stays centered at the locked position.
- **Beat Repeat:** active zone is frozen at the locked position — the physical slider has no effect.

Unlock behavior is the same as in standard mode: the slider re-engages after physically reaching the locked position (pickup mode).

---

### Phase Scrub Lock persistence (v1.4.6 pre-release)

Phase Scrub Lock (COPY + GLIDE short press) now **survives power cycles**. On boot with the lock restored, the slider enters pickup mode automatically — the playhead stays at the locked position until the slider physically reaches it.

---

## Sub-step Page Navigation (v1.4.6 pre-release)

**Combo:** SHIFT + PAGE button (while in sub-step mask edit mode)

For sequences longer than 8 steps, you can now change pages without exiting sub-step edit mode. Previously the only way to navigate was to exit (losing edit context), change page, and re-enter.

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

### Blue - CV Add Follow

The selected CV track follows another CV track as a transpose source. The source track's current pitch (relative to 0V) is added to the target track's pitch each update cycle, shifting the entire pitch sequence up or down by the source value.

**Use case:** run a short 2-4 step CV sequence as a transposer over a longer melodic pattern. The transposer sequence can have its own clock division, length, and direction settings, shifting the melody by different intervals each cycle.

---

### Lavender - CV Replace Follow

The selected CV track's step values are replaced entirely by the source CV track's current output. The target track outputs exactly what the source track outputs, ignoring its own programmed steps.

**Use case:** temporarily override a melodic track with the output of another sequence -- useful for unison lines, hard pitch coupling, or driving a track from a live CV input routed through another channel.

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

### Teal - Gate Clock Follow (Step Only)

The selected CV track advances **once per gate step**, ignoring ratchets and repeats entirely.

- Each gate step advances the CV track exactly once, regardless of ratchet count or repeat ticks.
- Useful when the gate track drives rhythm but you want the CV track to move at the step rate only.

**Use case:** a ratcheted percussion track keeps its rhythmic intensity while a melodic CV track advances at a steady one-pitch-per-step rate.

---

### Combining CV Follow and Gate Clock Follow

A CV track can have both a CV follow source (Blue or Lavender) and a gate clock source (Orange/Yellow/Salmon/Teal) assigned at the same time. The CV follow is applied on top of whatever step the CV track is currently on, regardless of what is clocking it.

---

## Tips

### Phase Scrub and performance

- Phase Scrub Lock is most useful at a performable moment — lock before a breakdown, unlock on the drop.
- In Beat Repeat, set a longer debounce (Enc 3) and use SHIFT staging for precise division drops: hold SHIFT, slide to target, release on the beat.
- Cyan mode (4-zone beat repeat) is easier to play live. Blue mode (8-zone) gives you triplet access but a smaller target for each zone — set debounce longer to compensate.
- Exclude a track from scrub (page button unlit) to keep it as a rhythmic anchor while granular or beat repeat reshapes everything else.
- In Granular mode, set direction to Ping-pong or Random for textures that don't repeat predictably even when the orbit window is stationary.
- Lock the scrub in Granular mode after dialing in an orbit position to freeze the loop while freeing your hand from the slider.

### Ratchets, repeats, and sub-steps

- Repeats are particularly effective on the last step of a pattern to create odd-length phrases that shift against other tracks.
- In the sub-step editor, setting a ratcheted step to `x o x o` produces a pattern that feels half the ratchet rate without changing the clock division.
- Mixing ratchets and repeats across steps in the same pattern produces complex polyrhythmic structures from a short base sequence.

### Linked Tracks

- In Orange mode, a CV track with a shorter length than the gate track will cycle through its pitches faster during ratcheted steps, creating repeating melodic fragments that change phase relative to the gate pattern.
- In Yellow mode, a repeat count of ×3 on a gate step produces 4 pitches from the CV track (the step start plus 3 repeat ticks) for that single gate event.
- Transpose Follow with a 2-step CV source set to a root and a fifth produces automatic power-chord-style transposition of any melody it follows.
- Lavender (CV Replace) with a source track in random playback mode creates **structured randomness**: program a set of target pitches on the source track, leave some steps at 0V, and assign it as the lavender source for a target track. The target plays its own programmed sequence except when the random source lands on a non-zero step, in which case it plays the source's pitch instead. The 0V steps act as pass-through gates, so you control both the density and the note pool of the random substitutions.
