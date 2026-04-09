# VoltSeq TODO

Backlog for VoltSeq firmware features, known bugs, and porting work.

---

## Active / Next Up

### Beat Repeat Mode is Non-Functional (Blue/Cyan modes in Performance Page)
**Root cause (diagnosed):** Beat repeat was ported from CatSeq but the core advance loop does nothing useful in VoltSeq. In beat repeat mode `window_ref` is always 1, so `orbit_pos_` = `(0+1)%1 = 0` on every advance. Therefore `orbit_step_[ch] = center_step` always — it never changes regardless of which zone the slider is in. All 8 blue zones and all 4 cyan zones are identical: they just hold the output at a fixed step. This is why blue appears to behave the same as cyan.

**Zone detection and engine table are correct.** The UI correctly divides into 8 zones for blue and 4 for cyan. The `div_num_8 / div_denom_8` table has proper triplet values (2/3, 1/3, 1/6 etc.). The problem is purely that the advance produces no movement.

**Design decision needed — two options:**

- **Option A — Clock multiplier:** When beat repeat is active, channels following orbit have their step advance rate overridden by `1 / safe_period` ticks per step. Requires `OnChannelFired` to be callable at arbitrary rates independent of clock divider, or a separate timer per following-channel that fires at `safe_period`. This produces actual rhythmic gate/trigger subdivision.

- **Option B — Metered window loop (preferred):** Keep the granular orbit window concept but advance it at the beat-repeat subdivision rate rather than every master clock tick. Set `window_ref = some_steps` driven by the zone index, and `orbit_should_adv` fires at `safe_period`. Following channels cycle through that window at the metered rate. This maps naturally onto VoltSeq's step-address model.

---

### Live Encoder Recording in Normal View
**Desired:** In normal view (unarmed, not in any editor), turning an encoder while NOT holding a page button live-records a value into the current playing step for that channel. Encoder movement should be fast — coarse increments relative to the channel's configured Range — unless Fine is held, which switches to fine sub-semitone steps.

**Why a TODO:** The debounce interaction needs careful tuning. At 3kHz, an encoder turn produces a delta of 1 per detent. Applying a large increment per detent (e.g., ÷8 of the full range per step = 8192 units) may overshoot. Also need to decide: record on every tick the encoder moves, or only on positive edges? Probably coarse = range/32 per detent, fine = range/256 per detent. Gate length and Trigger values have narrower ranges so need separate scaling.

---

### Global Channel Length / Clock Div Sync
**Desired:** A combo in Global Settings (Shift + encoder) that sets ALL channels to the same length and/or the same clock division simultaneously. Useful for "syncing up" channels after independent editing, especially when setting up a fresh sequence.

**Proposed UX:** Shift held, enc 3 (Length) sets a "default length" as today but also immediately applies it to ALL channels with one extra confirmation press (e.g., press encoder knob or hold for 600ms). Similarly for enc 6 (Clock Div). Alternatively, a dedicated "apply global to all" page-button press while in Global Settings.

---

### Channel Length — View Without Editing
**Desired:** When navigating to Channel Edit and pressing enc 2 (Length) to see the current length, the display should appear without immediately consuming the first encoder detent as an edit. Consider a brief "view mode" window after Channel Edit is entered (or after channel focus) where the length feedback is shown passively before the first encoder turn registers as a change. Similar to a "hold to preview, turn to edit" interaction.

**Note:** Related to the existing 600ms display timeout added in alpha4. May be solvable simply by requiring a small minimum delta (e.g., 2 detents) before the first length edit registers.

---

### Clock Division Redesign — multipliers + triplets
**Current state:** `Clock::Divider::type` is a `uint8_t` 0–255; `Read()` = v+1 (pure integer division by 1–256). No support for multipliers or fractional ratios.

**Desired:** CCW from unity adds clock multipliers (×2, ×3, ×4); divisions include triplet-friendly values (÷3, ÷6, ÷12, ÷24) so channels can run at ⅓, ⅙, etc. of the master clock.

**Proposed design:**
Replace `Divider::type` storage with a **table index** into a fixed list of ratios:

```
Index  Ratio   Label
  0    ×4      ×4
  1    ×3      ×3
  2    ×2      ×2
  3    ×1      ×1   ← default (v=3)
  4    ÷2      /2
  5    ÷3      /3
  6    ÷4      /4
  7    ÷6      /6
  8    ÷8      /8
  9    ÷12     /12
 10    ÷16     /16
 11    ÷24     /24
 12    ÷32     /32
```

For multipliers (ratio < 1), `Divider::Update()` must fire more than once per master tick. Replace bool return with int8 fire-count. Caller (`OnChannelFired`) loops on the count. Internally use a **phase accumulator** (float 0..1) for clean sub-integer handling:

```cpp
int Update(float ratio):
    phase_acc += 1.0f / ratio
    fires = 0
    while (phase_acc >= 1.0f) { fires++; phase_acc -= 1.0f; }
    return fires
```

**Persistent layout change:** bump `VoltSeq::Data::current_tag` when the stored index replaces the raw uint8.

---

### Global BPM — 1 BPM increments + reference snap points
**Current state:** BPM is stored as `bpm_in_ticks`; adjusting by enc delta moves in tick units (non-linear, hard to set a deliberate tempo).

**Desired:**
- Encoder 6 (BPM/Clock Div) in Global Settings moves in **1 BPM steps** (convert current BPM to int, ±1, convert back to ticks).
- Add **snap points** at common tempos (e.g., 60, 80, 90, 100, 120, 130, 140, 150, 160, 180, 200 BPM). The snap should act like a slight magnetic pull — encoder must pass through +2 steps within 400 ms to skip past a snap point.
- Display note: wiki should list the snap BPM values so users know where to aim.

---

### Presets / Scene Save-Load
**No current implementation.** Users need a way to save and recall entire VoltSeq states (all 8 channels, steps, settings) as named presets — analogous to the sequencer's slot/scene system.

**Open questions:**
- How many presets? 8 (matching scene buttons)? 16?
- Flash budget: `sizeof(VoltSeq::Data)` ≈ 1.1 KB × 8 = ~9 KB. Needs flash-layout review.
- UX: long-press page button = save to that slot, short press = load? Or Shift+page = save, page = load?
- Does scene-switch happen instantaneously or on next step boundary?

---

## Needs Hardware Verification

These features exist in the firmware but have not been user-tested yet. Each item should be checked before a stable release.

### Alpha-specific regression checks
- **Clear mode** (alpha5): SHIFT+PLAY held 600ms → page N clears channel N, PLAY clears all, any other button exits. Short press still resets.
- **SHIFT+CHAN toggle exits Channel Edit** (alpha6): confirm both entry and exit work, and that the save fires on exit.
- **Play exits armed mode without stopping playback** (alpha6): confirm plain Play while armed only disarms.
- **CHAN+encoder no longer causes clock stumble** (alpha6): test type cycling while sequence is running.
- **Tap tempo requires 3 taps** (alpha5+6, both CatSeq and VoltSeq): confirm first two taps are ignored, third takes effect.
- **Custom scale fallback** (alpha6): channel saved with a custom scale type should load as unquantized without corrupting step data.
- **Global settings encoders** (alpha6): enc 2=Dir, 3=Length, 5=Range, 6=BPM — confirm all four respond and save.

### Core features never formally tested
- **External clock sync**: patch a clock into Clock In; confirm VoltSeq locks to it, Reset jack resets all channels.
- **Direction modes** (Reverse, Ping-Pong, Random): test with multi-page sequences; verify Ping-Pong wraps correctly at page boundaries.
- **Per-channel clock divisions** (Channel Edit enc 6): channels running at different rates than master; confirm independence.
- **Glide time per CV channel** (GLIDE modifier, enc N while Glide held): confirm non-zero glide produces slew on output.
- **Output delay** (Channel Edit enc 1, 0–20 ms): confirm offset is audible/measurable between channels.
- **Random amount** (Channel Edit enc 8): confirm non-zero amount introduces step randomisation.
- **Multi-page patterns**: sequences longer than 8 steps; direction modes crossing page boundaries.
- **Hold multiple page buttons simultaneously**: multi-channel step editing — all held channels should update.
- **Orbit follow mask exclusion** (Performance Page page buttons): confirm a channel with its bit cleared is genuinely unaffected by orbit. Was reported as possibly non-functional; persist fix is in but correctness of exclusion not verified.
- **Granular orbit mode** (green perf mode): only beat repeat was exercised during alpha testing.
- **Phase rotate** (Channel Edit enc 4): destructive operation, confirm it only moves active steps and leaves steps beyond channel length untouched.

---

## Main Sequencer Ports (future)

### Universal "Play/Reset exits any mode"
VoltSeq now exits all modes on Play press. The main sequencer has modal states that don't share this pattern. Port the single-button-exits-everything UX to the main sequencer UI for consistency.

---

## Known Quirks / Low Priority

- **Wholetone scale and unquantized CV show the same grey** — both map to `Palette::grey` in the type selector. Low-priority color collision; needs a distinct color for wholetone.
- In the Glide Step Editor and Ratchet Step Editor, page navigation requires Shift+Page. There is no way to navigate without Shift while in the editor. Consider allowing bare page presses to navigate (since exit is now Glide/Play only, there is no conflict).
- Phase rotate (Channel Edit enc 4) is destructive with no undo. Add a brief "rotate pending" state where the user must confirm (second turn = commit), or document clearly.
- Trigger pulse width (Channel Edit enc 5) has no display feedback. Consider a brightness ramp on enc 5 LED (dim = short pulse, bright = long pulse).
