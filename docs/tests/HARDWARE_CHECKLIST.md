# VoltSeq Hardware Test Checklist

Flash `catalyst-vX.Y.Z-catseq-voltseq.wav` before running. Work through each section in order — most sections require only one navigation combo to reach and can be tested before exiting.

---

## Boot

- [ ] Module boots without hanging or error blinks
- [ ] Encoder LEDs show step colors for each channel (main mode default view)
- [ ] Page buttons are unlit (no playhead until playing)

---

## Mode Switching

- [ ] **CatSeq → VoltSeq:** hold **Shift + Tap Tempo + Chan.** for 1 second — all 8 channel LEDs blink to confirm
- [ ] **VoltSeq → CatSeq:** hold **Fine + Play/Reset + Glide** for 1 second — all 8 channel LEDs blink to confirm
- [ ] Mode is saved: power cycle and confirm the same mode is active

---

## Main Mode (entry: boot / exit any editor)

### Play / Stop

- [ ] **Play/Reset** starts playback; chaselight appears on page buttons
- [ ] **Play/Reset** again stops playback
- [ ] Settings saved to flash on stop: power-cycle mid-session and confirm step data survives

### Reset (while playing)

- [ ] **Shift + Play** (short tap, < 600 ms): all channels snap to step 1 immediately, no stutter
- [ ] **Shift + Play** (hold ≥ 600 ms): enters Clear mode (page buttons and Play LED slow-blink)

### Clear Mode (entry: hold **Shift + Play** ≥ 600 ms)

- [ ] Page buttons slow-blink to confirm Clear mode entry
- [ ] Tap **Page button N**: clears all 64 steps of channel N; CV → center/0V, Gate/Trigger → all off
- [ ] **Play/Reset**: clears all 8 channels
- [ ] Any other button: exits without clearing

### Page Navigation

- [ ] **Shift + Page button N**: navigates to page N from main mode
- [ ] **Shift + Page button N**: works while armed
- [ ] While **Shift held**: current page button lights solid; focused encoder blinks white
- [ ] Pressing a page button while Shift held navigates without editing steps

### Step Editing

- [ ] Hold **Page button N** + turn **Encoder M**: edits channel M's step at position N
- [ ] Hold multiple page buttons: same encoder edit applies to all held step positions
- [ ] CV channel: coarse ~1 semitone per detent
- [ ] CV channel: hold **Fine** → sub-semitone resolution
- [ ] CV channel: fast spinning accelerates
- [ ] Gate channel: adjusts gate length (0–100%); no acceleration
- [ ] Trigger channel: adjusts ratchet/repeat count; no acceleration

### Chaselight

- [ ] Turn an encoder (no page held): focuses chaselight on that channel; no step edited
- [ ] Hold page + turn encoder: edits step and refocuses chaselight to that encoder's channel
- [ ] Page buttons show playhead position for most recently focused channel

---

## CHAN Held (no page button)

- [ ] Hold **Chan.**: encoder LEDs switch to channel-type colors (grey=CV, green=Gate, green/teal=Trigger)
- [ ] Release **Chan.**: returns to normal encoder display

---

## Armed Mode (entry: **Chan. + Page button N**)

- [ ] Correct encoder blinks to confirm arm
- [ ] **Chan. + same Page button N**: disarms
- [ ] **Play/Reset** while armed: disarms without stopping playback
- [ ] **Play/Reset** stops playback, channel remains armed if already armed before stop

### Armed CV Channel

- [ ] Phase Scrub slider writes voltage to current step while playing
- [ ] Phase Scrub slider sets last-touched step while stopped
- [ ] Encoder LEDs show CV step colors for current page
- [ ] Playing step blinks white (suppressed on held page button)
- [ ] **Encoder N**: edits step N's CV value; **Fine** = sub-semitone; fast spinning accelerates
- [ ] **Shift + Enc 5** (Range): adjusts channel voltage range; slider recording window follows
- [ ] **Shift + Enc 7** (Transpose): adjusts quantizer scale
- [ ] **Glide held**: all encoder LEDs show glide time as brightness (fully off = 0 s / disabled, full white = 10 s)
- [ ] **Glide held + any Encoder CW**: increase glide time; LEDs brighten
- [ ] **Glide held + any Encoder CCW**: decrease glide time; LEDs dim; fully CCW = off (no glide)

### Armed Gate Channel

- [ ] Encoder LEDs show gate state (bright = gate on)
- [ ] Playing step blinks white
- [ ] **Tap Page button**: toggle step on/off (x0x style)
- [ ] **Encoder N**: adjusts gate length for step N (0 = off)
- [ ] **Glide held**: encoder LEDs switch to per-step ratchet counts
- [ ] **Glide held + Encoder N**: adjusts ratchet count (0 = single gate, 2–8 = subdivide)

### Armed Trigger Channel

- [ ] Encoder LEDs show ratchet/repeat state
- [ ] Playing step blinks white
- [ ] **Tap Page button**: toggle step between rest and single trigger
- [ ] **Encoder N CW**: increase ratchet count (subdivide; 1–8)
- [ ] **Encoder N CCW**: increase repeat count (extend; negative values)

---

## GLIDE Modifier (unarmed, entry: hold **Glide**)

- [ ] **Glide + Encoder N** (CV channel): adjusts glide time (0–10 s)
- [ ] **Glide + Encoder N** (Gate channel): offsets all gate lengths for channel N
- [ ] **Glide + Encoder N** (Trigger channel): adjusts pulse width (1–100 ms)
- [ ] **Glide + hold Page N + Encoder M** (Gate channel M): sets ratchet count for step N
- [ ] **Shift + Glide + Encoder N** (Trigger channel): offsets all ratchet/repeat counts for channel N

---

## Channel Edit (entry: short tap **Shift + Chan.** — release before 2 s)

- [ ] Entry confirmed: encoder LEDs change to channel-edit colors
- [ ] **Press Page button**: focuses that channel
- [ ] Page buttons show chaselight for focused channel
- [ ] **Shift held**: page buttons switch to current-page indicator (lit solid)
- [ ] Enc 1 (Start): output delay 0–20 ms; LED brightness tracks amount
- [ ] Enc 2 (Dir.): direction cycles Forward / Reverse / Ping-Pong / Random; LED color changes
- [ ] Enc 3 (Length): step length 1–64; page buttons briefly show page count / step fill on last page
- [ ] Enc 4 (Phase): phase rotate (destructive); confirm steps shift
- [ ] Enc 5 (Range): voltage range (CV) or pulse width (Trigger)
- [ ] Enc 6 (BPM/Clock Div): per-channel clock division
- [ ] Enc 7 (Transpose): channel type (CV → Gate → Trigger) and quantizer scale; LED = type color
- [ ] Enc 8 (Random): random amount 0–100%; LED brightness tracks amount
- [ ] **Shift + Chan.** (short tap): exits and saves
- [ ] **Play/Reset**: exits and saves; playback continues if was playing

---

## Global Settings (entry: hold **Shift + Chan.** for 1 second)

- [ ] Entry after ~1 s hold; short tap enters Channel Edit instead
- [ ] If either button released before 1 s, fires as short tap (Channel Edit entry) — not Global Settings
- [ ] Chaselight visible on page buttons (focused channel playhead blinks)
- [ ] Reset leader button lights solid (if set)
- [ ] Enc 1 (Start): Play/Stop reset mode — red = on; off = off
- [ ] When Play/Stop reset on: pressing Stop also resets all channels to step 1
- [ ] Enc 6 (BPM/Clock Div): adjusts internal BPM; LED pulses in BPM color zone
  - [ ] Red < 50 BPM
  - [ ] Orange 50–79 BPM
  - [ ] Yellow 80–99 BPM
  - [ ] Green 100–119 BPM
  - [ ] Blue 120–149 BPM
  - [ ] Teal 150–179 BPM
  - [ ] Lavender 180+ BPM
- [ ] **Play/Reset**: exits and saves

---

## Clock

### Internal Clock

- [ ] **Tap Tempo** 3+ taps: BPM locks on third tap and updates with each subsequent tap
- [ ] BPM LED in Global Settings matches current BPM color zone
- [ ] Fine-tune BPM via enc 6 in Global Settings

### External Clock (16 PPQN — 1 pulse per 16th note)

- [ ] Patch clock into **Clock In**: VoltSeq locks to external clock automatically
- [ ] Unplug external clock: internal clock resumes at the same BPM (no 4× speed increase)
- [ ] Playback stays in sync with clock; no stutter between steps
- [ ] Step 0 fires immediately on first clock pulse (no extra pause)

### Reset Jack

- [ ] Patch trigger into **Reset jack**: all channels snap to step 1 immediately
- [ ] Step 1 outputs fire on the reset pulse (no extra clock cycle delay)
- [ ] Sequence is in sync from the moment the reset pulse arrives
---

## Performance Page (entry: hold **Fine + Glide** for 1.5 s)

- [ ] Releasing before 1.5 s cancels without entering
- [ ] Encoder LEDs light to confirm entry
- [ ] Slider controls orbit center position
- [ ] **Page button N**: toggles per-channel orbit follow (lit = follows slider)
- [ ] **Shift held**: freezes orbit center in beat-repeat mode
- [ ] **Shift + Page button N**: navigates pages
- [ ] **Hold Page + turn Encoder**: step editing same as main mode

### Performance Page Settings (shown on entry)

- [ ] Enc 1 (Start): quantize orbit center — orange = on
- [ ] Enc 2 (Dir.): performance mode — off / green (granular) / blue (beat-repeat ×8) / cyan (beat-repeat ×4)
- [ ] Enc 3 (Length): granular width or beat-repeat debounce
- [ ] Enc 4 (Phase): orbit direction (granular only) — Green / Blue / Orange / Lavender
- [ ] Enc 8 (Random): Phase Scrub Lock — red = locked, blinks red during pickup

### Phase Scrub Lock

- [ ] **Enc 8** in Performance Page: toggles lock on/off
- [ ] **Short Fine + Glide** (tap, release before 1.5 s): toggles lock from main mode
- [ ] **Short Fine + Glide**: toggles lock from within Performance Page
- [ ] Locked: slider frozen; enc 8 LED red
- [ ] Unlock: pickup mode — orbit resumes only after slider reaches locked position (no jump)
- [ ] Lock state survives power cycle

### Exit

- [ ] **Play/Reset**: exits Performance Page and saves to flash

---

## Saving

- [ ] Step data survives power cycle
- [ ] Channel settings (length, type, direction, range, etc.) survive power cycle
- [ ] Global settings (BPM, reset mode, reset leader) survive power cycle
- [ ] Performance page settings survive power cycle
- [ ] Mode selection (CatSeq/VoltSeq) survives power cycle
- [ ] Active mode between Channel Edit and Global Settings is correctly saved: exit via Play/Reset, power cycle, confirm
