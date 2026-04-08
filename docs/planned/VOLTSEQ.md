# VoltSeq — Catalyst as a Step Sequencer

> **Implementation status (as of v1.5.0-alpha3):** Core feature is complete and in active alpha testing. See `CHANGELOG.md` for the full implemented description. The sections below are the original design spec; known deviations from what shipped are noted inline.
>
> **Key deviations from this spec:**
> - `slider_base_v` / `slider_span_v` removed — slider recording uses the channel `Range` directly.
> - Armed Gate/Trigger: encoder N directly edits step N (no hold-page-button required).
> - Channel Edit encoder order matches physical panel silkscreen (Start/Dir/Length/Phase/Range/ClockDiv/Transpose/Random), not EncoderAlts enum order.
> - Phase rotate operates only on active steps within `length`, not all 64.
> - Bare page-button disarm removed; disarm requires Chan. + Page button.
> - Only two WAV builds per release (catseq-catcon, catseq-voltseq); voltseq-catcon deferred.
> - `VoltSeq::Data::current_tag = 3u` as of alpha3.

Replaces Macro mode in one of two release variants (see Release Strategy below). The mode-switch shortcut (Sequencer ↔ Macro) becomes Sequencer ↔ VoltSeq. All existing Macro data is abandoned on first boot; bump `Macro::Data::current_tag` (or rename it) to force a clean init. Consider renaming the `Macro` enum value in `params.shared.mode` to `VoltSeq` in place — same underlying integer, no flash migration needed.

## Release Strategy

Three WAV files will be published per release:

| Filename | Left 3 (Fine+Play+Glide) | Right 3 (Shift+Tap+Chan) |
|---|---|---|
| `catalyst-vX.Y.Z-catseq-catcon.wav` | CatSeq | CatCon |
| `catalyst-vX.Y.Z-catseq-voltseq.wav` | CatSeq | VoltSeq |
| `catalyst-vX.Y.Z-voltseq-catcon.wav` | VoltSeq | CatCon |

All three are built from the **same commit** using compile-time flags in `conf/build_options.hh` selecting which `::App` class each mode slot resolves to. `MacroSeq` in `src/app.hh` dispatches on `params.shared.mode` (still a 2-value enum — mode A or mode B); what each value maps to is determined at compile time. No separate branches needed.

The GitHub Actions release workflow (`release.yml`) will need a small update to run three build passes per tag and attach all three WAVs to the release.

Flash note: the three variants share the same two storage slots (`Sequencer::Data` and `Macro::Data`). Switching between builds will trigger a clean init on the affected slot. Users will generally pick one build and stay on it.

Panel reference: **Sequencer panel throughout.** All button labels use Sequencer panel silkscreen.

---

## Concept

8 channels of output, up to 64 steps per channel (8 pages × 8 steps). Each page button is a step. Each encoder knob is a channel. Channels advance independently on the clock with per-channel length, clock division, direction, and type. Three channel types: **CV** (continuous voltage), **Gate** (sustained high/low), **Trigger** (x0x pulse with ratchets/repeats).

---

## Hardware Mapping

| Hardware | VoltSeq role |
|---|---|
| Pages buttons 1–8 | Steps 1–8 on current page |
| Encoders 1–8 | Channel values (ch 1–8) for the held step; channel settings in Channel Edit |
| Slider (Phase Scrub) | Real-time CV record source (when CV channel armed) |
| Clock In jack | External clock |
| Reset jack | Hardware reset — all playheads to step 0 |
| Phase CV jack | Reserved (v1) |
| Play/Reset button | Press: play/stop; SHIFT+PLAY: reset all playheads to step 0 (matches Sequencer convention) |
| Shift | Global settings modifier; page navigation modifier |
| Chan. / Quantize | Arm channel modifier (CHAN + page button); Channel Edit entry (Shift + CHAN) |
| Tap Tempo | Single tap: tap tempo (matches Sequencer convention — `add.just_went_high()` = tap) |
| Glide (Ratchet) | Glide / gate-width / ratchet modifier (see Glide section) |
| Fine / Copy | Fine encoder adjustment (1-unit steps); copy/paste steps |

Encoders are **not** press encoders. All interactions are rotation only.

---

## Mode Switching

Mode switches are triggered by holding all three buttons of either cluster simultaneously. Detected globally from any UI state (handled in the common tick, not a specific mode).

| Gesture | CatSeq + CatCon (stock) | CatSeq + VoltSeq | VoltSeq + CatCon |
|---|---|---|---|
| Left 3 held (Fine + Play + Glide) | → CatSeq | → CatSeq | → VoltSeq |
| Right 3 held (Shift + Tap + Chan) | → CatCon | → VoltSeq | → CatCon |

VoltSeq occupies whichever slot the displaced mode was in. The cluster-to-mode mapping is **identical to stock** — no gesture relearning across builds. In the CatSeq+VoltSeq build, both modes use the Sequencer panel; the left/right distinction remains consistent even though no panel flip is involved.

Replaces the previous SHIFT+CHAN long-hold modeswitcher, which only fired from within Channel Settings. The 3-button combo works from anywhere and unambiguously targets a specific mode rather than toggling.

**Implementation note:** This gesture needs to be adopted in the Sequencer mode (`seq_common.hh` `Common()`) as well, replacing the `modeswitcher` in `seq_settings.hh`. Both modes should share the same detection logic.

### Startup Mode Lock (Power-On)

Holding either 3-button cluster **at power-on** sets that mode as the persisted startup mode (`params.shared.data.saved_mode`), exactly as the current firmware does. The boot detection lives in `src/ui.hh` before the main loop, saves if changed, then waits for buttons to release before continuing.

Current mapping (`fine+play+morph` → Sequencer, `bank+add+shift` → Macro) is preserved exactly. VoltSeq takes whichever slot the displaced mode occupied — no change to the physical gesture, no user relearning.

---

## Step and Page Model

- **Page**: 8 steps. Navigate via **Shift + Pages button 1–8**.
- **Global step index** (0-based): `page × 8 + step_within_page`, indices 0–63.
- **Per-channel length**: Independent step count per channel (1–64). Channels wrap at their own length, enabling polyrhythms.
- **Page display while Shift held**: Pages button LEDs — solid = current page, dim = page contains non-default data, off = empty page.

---

## Channel Types

Set per channel via enc 4 (Transpose silkscreen) in Channel Edit mode. The encoder LED cap color indicates the current type/scale at a glance.

| Type | Description | Encoder LED |
|---|---|---|
| CV — Off (unquantized) | Continuous voltage, no quantization | White / dim |
| CV — Chromatic | Quantized to chromatic scale | Blue |
| CV — [scale N] | Quantized to one of the built-in or custom scales | Scale-specific color |
| Gate | Sustained high/low per step; encoder sets gate length (% of step clock) | Yellow |
| Trigger | Short pulse per step; encoder sets ratchet/repeat count | Green |

Cycling enc 4 CW steps through: CV-Off → CV-Chromatic → [scales] → Gate → Trigger → CV-Off.

---

## Playback Model

### Clock

- **External**: Clock In jack, rising edge = one master tick.
- **Internal**: Tap Tempo sets BPM when no clock is patched. Running average of the last N inter-tap intervals.
- **Per-channel clock division**: Reuses existing `Clock::Divider` type. Channel N advances its playhead every M master ticks per its configured divisor.

### Playheads

Each channel has its own playhead (0 to `length − 1`). On each master tick, a channel whose division counter fires advances its playhead according to its direction.

**Shadow playhead**: When one or more page buttons are held during playback, a shadow copy of each channel's playhead continues advancing normally. Outputs are driven from the held/arpeggiated step(s) instead. On release, all playheads snap to their shadows. This preserves tempo continuity.

### Direction

Per channel. Stored in `ChannelSettings::direction`.

| Value | Behavior |
|---|---|
| Forward | 0, 1, 2 … length−1, 0 … |
| Reverse | length−1, … 1, 0, length−1 … |
| Ping-Pong | 0→length−1→0; endpoints played once per pass |
| Random | Uniform random step each tick; never repeats the same step consecutively |

---

## Step Editing — Normal Mode (No Channel Armed)

### Hold Step to Edit

Hold any **Pages button** (step N on current page):
- All 8 encoder rotations set channel 1–8's stored value for that step.
- Multiple page buttons can be held simultaneously — encoders write to **all held steps** at once.
- Fine button applies while step is held (1-unit encoder steps).
- Encoder LED caps reflect each channel's current value for that step (color/brightness encoding).

### Hold Step During Playback (Step-Lock)

- Outputs freeze to the held step's values.
- Shadow playhead advances in the background.
- Release → all playheads snap to shadow position.

### Step Arpeggiation

Hold two or more page buttons during playback:
- Each master tick cycles outputs through held steps in ascending button-index order.
- Shadow playhead advances at full clock rate.
- Release → snap to shadow.

---

## Arming a Channel (CHAN + Page Button)

**CHAN + page button N** arms channel N (1–8) for focused editing. The page button N LED pulses to indicate armed state. Press page button N again (without CHAN held) to disarm.

Only one channel can be armed at a time. Arming a different channel via CHAN + page button M disarms the previous one.

While armed, **Shift + page button** navigates pages normally — step editing continues on the new page after releasing Shift.

---

### Armed: CV Channel

Slider records in real time. On each master clock tick, the current slider ADC value is written to channel N's current step, mapped to the slider recording window and quantized if a scale is set.

If the sequencer is **stopped**, slider value is written to the last-held step (or step 0 if none).

**Encoders while armed (CV channel):**

| Encoder | Silkscreen | Function while CV channel armed |
|---|---|---|
| 3 | Range | Slider recording span — discrete steps: **1, 2, 3, 4, 5, 10, 15** V |
| 4 | Transpose | Slider recording base (lower limit) — discrete steps: **−5, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10** V |

The slider maps from `slider_base_v` to `slider_base_v + slider_span_v`. Examples: base=1, span=2 → 1V–3V; base=−5, span=15 → full −5V–10V range.

The slider recording window (`slider_base_v`, `slider_span_v`) is **independent** of `channel.range` (the output range used for encoder step editing). A narrow recording window gives smooth, precise slider sweeps; a wide channel range preserves full encoder resolution for manual step editing.

All other encoders: no effect while CV channel is armed.

---

### Armed: Gate Channel

Page buttons 1–8 are x0x pads for channel N on the current page.

- **Press page button**: toggle that step between off (0) and on (default gate length).
- **Hold page button + turn any encoder**: set gate length for that step (0 = off/rest; 1–100 = percentage of step clock duration).
- Shift + page button: navigate to a different page (step editing continues on new page).

---

### Armed: Trigger Channel

Page buttons 1–8 are x0x pads for channel N on the current page.

- **Press page button**: toggle that step between rest and a single trigger.
- **Hold page button + turn encoder CW**: increase ratchet count (1 trigger → 2 → 3 → 4 per step clock, subdivided equally within the step).
- **Hold page button + turn encoder CCW from 1**: step returns to rest (0). Further CCW from rest: add repeats (step consumes N additional step slots, extending the pattern; equivalent to the sequencer mode repeat behavior).
- Shift + page button: navigate pages.

Step values for trigger channels are stored as `int8_t`-range integers within the `uint16_t` StepValue: positive = ratchet count, 0 = rest, negative = repeat depth. Upper byte unused (reserved for future accent flag).

---

## Glide / Gate Width / Ratchet (GLIDE button)

The GLIDE button works as a modifier with two variants: bare (GLIDE) and shifted (SHIFT+GLIDE). Both share the same interaction pattern; channel type determines the effect. This matches the Sequencer convention of GLIDE being the primary ratchet/glide modifier.

### Effect by channel type

| | CV | Gate | Trigger |
|---|---|---|---|
| **GLIDE** | Slew time | Gate width (all steps, relative offset) | Pulse width (`pulse_width_ms`) |
| **SHIFT+GLIDE** | — (no effect) | — (no effect) | Ratchet/repeat (all steps, relative offset) |

### GLIDE + encoder N (no page button held)

Adjusts channel N's parameter destructively across all steps:
- **CV channel**: sets `slew_time` for channel N (channel-level setting; not per-step)
- **Gate channel**: offsets all steps' gate length by the encoder increment (destructive relative adjust)
- **Trigger channel**: adjusts `pulse_width_ms` for channel N

### SHIFT+GLIDE + encoder N (no page button held)

- **Trigger channel**: offsets ratchet/repeat count on all steps by the encoder increment (destructive relative adjust; clamps at min/max)
- **CV / Gate**: no effect

### GLIDE + long-press page button N → Glide Step Editor

Long-press activates a held editor "screen" so you don't need to sustain the full chord:
- **CV channel N**: encoders 1–8 show and toggle the glide flag for steps 1–8 on the current page. Encoder LED lit = glide on for that step.
- **Gate channel N**: encoders 1–8 set gate length for steps 1–8 individually (same as hold-step + encoder in normal mode, but channel-focused).
- **Trigger channel**: falls through to ratchet editor (same as SHIFT+GLIDE + long-press below).

Navigate pages with SHIFT + page button while in the editor. Exit: press page button N again, or press GLIDE.

### SHIFT+GLIDE + long-press page button N → Ratchet Step Editor

Long-press activates the ratchet editor screen for channel N:
- **Trigger channel N**: encoders 1–8 set ratchet/repeat count for steps 1–8 on the current page. CW = add ratchets, CCW = add repeats (same direction convention as normal step editing).
- **CV / Gate**: no effect (exits immediately).

Navigate pages with SHIFT + page button. Exit: press page button N again, or press GLIDE.

### Slew application

When a CV step's glide flag is set, `Slew::Interface` for that channel interpolates from the previous value to the new one over `slew_time`. Gate and Trigger bypass slew entirely.

---

## Slider Performance Page

Entered via **Fine/Copy + Glide hold 1.5s** (same entry gesture as Sequencer mode). Exit via the same combo (short press) or Play/Reset.

### Mode 0 — Standard
Slider has no effect during playback. Channels advance by clock only. Slider is only active when a CV channel is armed for recording.

### Mode 1 — Granular
`orbit_center` tracks the slider position. Each clock tick advances `orbit_pos` through a window of steps around the center, per channel. Width and direction configurable. Channels can be individually excluded from orbit (page buttons toggle per-channel follow mask, same as Sequencer).

The orbit logic from `sequencer.hh` (`orbit_center`, `orbit_width`, `orbit_direction`, per-channel `orbit_step[]`) is essentially channel-type-agnostic and can be reused directly. The key adaptation: VoltSeq reads `orbit_step[ch]` instead of the clock playhead when `orbit_active && channel_follows_orbit[ch]`.

### Mode 2 / 3 — Beat Repeat
Slider divided into zones (mode 2: 8 zones with triplets; mode 3: 4 zones, no triplets). Each zone loops the current step at a beat subdivision. Shift freezes the active zone. Zone-to-subdivision tables and countdown logic are portable from `sequencer.hh`.

**Additional requirement for VoltSeq:** beat period (`3000 × 60 / bpm`) requires measuring the inter-clock-tick interval for external clock. Add a running average of the last N clock edges to the clock engine. Internal clock uses `internal_bpm` directly.

### Lock
Freezes `orbit_center` (or beat repeat zone) so a slider bump doesn't shift the loop. Pickup mode activates on unlock — slider must physically reach the locked position before taking over.

Also applies when a CV channel is armed: lock pauses writing to steps without disarming, so the slider can sit still between recording passes.

### Performance Page Settings

Entered from the performance page via the same settings gesture. Encoders:

| Encoder | Silkscreen | Setting |
|---|---|---|
| 1 | Dir. | Quantize orbit center to step boundaries (on/off) |
| 2 | Phase | Performance mode: off / granular / beat-repeat 8-zone / beat-repeat 4-zone |
| 3 | Range | Granular width (% of channel length) / beat repeat debounce delay |
| 4 | Transpose | Orbit direction: Fwd / Rev / Ping-Pong / Random |
| 8 | Random | Lock toggle |

Page buttons toggle per-channel orbit follow mask (lit = follows orbit, unlit = plays normally).

---

## Copy / Paste

Matches Sequencer mode gesture convention (`c.button.fine` = Fine/Copy):

- **Fine/Copy just-pressed while page button is held** → copy all 8 channel values for that step into clipboard.
- **Fine/Copy held, then press page button** → paste clipboard into that step.
- **Fine/Copy held, press multiple page buttons** → paste into all pressed steps.

Clipboard is volatile (lost on power cycle).

---

## Shift Mode (Global Settings)

Hold **Shift**. Page button LEDs show page status. Encoder rotations set global parameters:

| Encoder | Silkscreen | Global setting |
|---|---|---|
| 1 | Dir. | Default direction for newly initialized channels |
| 2 | Phase | Reserved |
| 3 | Range | Default voltage range for newly initialized channels |
| 4 | Transpose | Reserved (mode-select role belongs to Channel Edit only) |
| 5 | Start | Reserved |
| 6 | Length | Default step length for newly initialized channels |
| 7 | BPM/Clock Div | Internal BPM (active when no clock is patched) |
| 8 | Random | Randomize all channel random seeds (turn to regenerate) |

---

## Channel Edit Mode (Shift + CHAN)

Hold **Shift**, then press **Chan. / Quantize** → enter Channel Edit. Both buttons may be released; mode stays active. Press **Chan.** or **Shift + CHAN** to exit.

Press a **Pages button N** (1–8) → focus channel N. That channel's encoder LED cap lights solidly.

Encoder rotations set **per-channel settings** for the focused channel:

| Encoder | Silkscreen | CV channel | Gate channel | Trigger channel |
|---|---|---|---|---|
| 1 | Dir. | Direction | Direction | Direction |
| 2 | Phase | Phase rotate (destructive — see below) | Phase rotate | Phase rotate |
| 3 | Range | Output voltage range | — (ignored) | Pulse width (1–100ms; default 10ms) |
| 4 | Transpose | **Mode selector**: cycles CV-Off → scales → Gate → Trigger → CV-Off; LED = current mode | ← same | ← same |
| 5 | Start | Output delay: 0–20ms | Output delay: 0–20ms | Output delay: 0–20ms |
| 6 | Length | Step count (1–64) | Step count | Step count |
| 7 | Clock Div | Clock division | Clock division | Clock division |
| 8 | Random | Random amount (0 = deterministic, max = fully random) | Step probability | Step probability |

### Phase Rotation (Destructive)

Turning enc 2 (Phase) in Channel Edit **rewrites** the step data for the focused channel: each detent CW shifts all step values one position forward (step 0's old value wraps to the last step); CCW shifts backward. No `phase_offset` field is stored — the rotation is baked into the data immediately.

---

## Output Calculation

Per channel per tick:

```
step_idx    = clamp(playhead[ch], 0, length[ch] - 1)
raw_value   = steps[page(step_idx)][step_within_page(step_idx)][ch]

// CV channels:
ranged      = map(raw_value, 0..65535, range[ch].min, range[ch].max)
quantized   = apply_scale(ranged, scale[ch])          // if scale != Off
random_mod  = random[ch][step_idx] × random_amount[ch]
out_cv      = slew[ch].Update(quantized + random_mod) // bypass slew if glide off

// Gate channels:
gate_length = raw_value / 65535.0                     // 0 = off, 1.0 = full step
// delay fires trig_delay_ms after clock tick; CV channels on same tick fire immediately
out_gate    = 10V after trig_delay_ms, held for (gate_length × step_period), then 0V

// Trigger channels:
ratchets    = max(0, raw_value_as_int8)               // positive = ratchet count
repeats     = max(0, -raw_value_as_int8)              // negative = repeat depth
// probability check (random_amount) gates the entire step including all ratchets
// ratchets: fire `ratchets` equally-spaced 10V pulses within the step window,
//           each pulse width = channel.pulse_width_ms, first pulse after trig_delay_ms
// repeats:  hold current step, consume next `repeats` step slots before advancing
```

---

## Data Model

Replace `Macro::Data` with `VoltSeq::Data`. Bump `Macro::Data::current_tag` to force clean init.

```cpp
namespace Catalyst2::VoltSeq {

// 8 pages × 8 steps × 8 channels = 512 uint16_t = 1024 bytes
using StepGrid = std::array<                     // [page]
                   std::array<                   // [step within page]
                     std::array<StepValue, NumChans>, // [channel]
                   8>,
                 8>;

enum class Direction : uint8_t { Forward, Reverse, PingPong, Random };

struct ChannelSettings {
    uint8_t              length          = 8;     // 1..64
    Clock::Divider::type division        = {};    // reuse existing type
    Channel::Cv::Range   range           = {};    // output range (encoder editing); CV only
    int8_t               slider_base_v   = 0;     // slider recording lower limit V: −5,0,1..10; CV only
    uint8_t              slider_span_v   = 5;     // slider recording span V: 1,2,3,4,5,10,15; CV only
    Channel::Mode        scale           = {};    // reuse: Off/Chromatic/scales/Gate/Trigger
    Direction            direction       = Direction::Forward;
    float                random_amount   = 0.f;
    float                glide_time      = 0.f;   // seconds; 0 = disabled; CV only
    uint8_t              pulse_width_ms  = 10;    // trigger pulse width in ms; Trigger only
    uint8_t              output_delay_ms = 0;     // output delay in ms; all channel types
};

struct StepFlags {
    uint64_t glide = 0;   // bit N = glide flag for step N (steps 0..63)
};

struct Data {
    StepGrid                              steps{};
    std::array<ChannelSettings, NumChans> channel{};
    std::array<StepFlags, NumChans>       flags{};
    Direction                             default_direction = Direction::Forward;
    Channel::Cv::Range                    default_range     = {};
    uint8_t                               default_length    = 8;
    float                                 internal_bpm      = 120.f;
    Recorder::Data                        recorder{};   // slider record buffer, reused

    void validate() const { /* ... */ }
};

} // namespace Catalyst2::VoltSeq
```

**Size estimate:**
| Component | Size |
|---|---|
| `steps` (StepGrid) | 1024 bytes |
| `channel[8]` (ChannelSettings) | ~8 × 28 = 224 bytes |
| `flags[8]` (StepFlags) | 64 bytes |
| Scalars | ~16 bytes |
| `recorder` (reused) | ~4100 bytes |
| **Total** | **~5430 bytes** |

Current `Macro::Data` allocation: 7820 bytes. Fits comfortably.

---

## Reuse From Existing Code

| Existing component | Disposition |
|---|---|
| `Clock::Divider` | Reuse — per-channel clock division |
| `Channel::Cv::Range` | Reuse — `channel.range` for output; slider window stored as `int8_t`/`uint8_t` pair |
| `Channel::Mode` | Reuse — extend enum to include `Trigger` after `Gate` |
| `Recorder::Data` + buffer | Reuse — slider sample buffer for CV recording |
| `Slew::Interface` | Reuse — instantiate 8 (one per channel); bypass when glide off |
| `Shared::Interface` | Reuse — DAC calibration, custom scales, mode persistence |
| Multi-step hold / Fine button patterns | Adapt from `Macro::Main` UI |
| `Random::Pool` | Reshape from 8×8 to 8×64 or compute on the fly |

**Replace entirely:**
- `Bank::Interface`, `Pathway::Interface` — path-based morphing has no equivalent here
- `Mode::Interface` (Normal/Blind/Latch) — not applicable
- `Macro::Interface::Update()` / `Trig()` — replaced by clock engine + per-channel playheads

---

## Things to Build

1. **Clock engine**: Rising-edge detection on Clock In; internal timer at `internal_bpm` when unpatched. Master tick counter drives per-channel division counters. Track inter-tick interval (running average of last N edges) for beat repeat beat-period calculation under external clock.
2. **Per-channel playhead + shadow playhead**: Shadow advances unconditionally each tick; real playhead copies shadow when no steps are held. Shadow playheads are **paused** while any channel is armed or while Channel Edit / Settings mode is active — they serve no purpose in those states and pausing avoids silent drift that would surprise the user on return to normal playback.
3. **Direction state machine**: Adds `int8_t ping_pong_dir[NumChans]` to runtime state (not stored).
4. **Step-lock and arpeggiation**: Sorted held-step list; cycle through on each tick.
5. **Arm state machine**: Tracks which channel (if any) is armed, which type it is, and routes page buttons / slider / encoders accordingly.
6. **Slider-to-step recorder**: On each tick while a CV channel is armed, write `map(slider_adc, 0..4095, slider_base_v * V_scale, (slider_base_v + slider_span_v) * V_scale)` → quantize → write to `steps[page][step][ch]`. Replaces `Recorder::Interface` playback logic (buffer reused, state machine replaced).
7. **x0x step toggle + value edit**: Short-press toggles gate/trigger step on/off; long-press-hold + encoder sets gate length or ratchet/repeat count (via armed channel or glide editor).
8. **Ratchet generator**: Subdivide step window into N equal pulse slots; fire triggers at each slot boundary.
9. **Repeat handler**: When repeat depth R > 0, hold playhead for R additional ticks before advancing.
10. **Per-channel glide**: 8 `Slew::Interface` instances; bypassed when `glide_time == 0` or step's glide flag is off.
11. **Glide step editor UI**: Long-press-activated held screen entered via GLIDE + long-press page button. Encoders show/edit per-step glide flags (CV) or gate lengths (Gate) for the focused channel.
12. **Ratchet step editor UI**: Long-press-activated held screen entered via SHIFT+GLIDE + long-press page button. Encoders show/edit per-step ratchet/repeat counts for the focused Trigger channel.
13. **Phase rotation**: In Channel Edit, enc 2 detents call a `RotateChannel(ch, delta)` function that `std::rotate`s the channel's full 64-step logical sequence and writes back into the page-indexed structure.
14. **Channel Edit UI**: New UI state class analogous to `Macro::Settings`.
15. **Tap Tempo**: Running average of last 4 inter-tap intervals → `internal_bpm`. `add.just_went_high()` = tap, matching Sequencer convention.
16. **Internal clock generator**: Timer ISR or tick-counter when Clock In is unpatched.
17. **SHIFT+PLAY reset**: Direct `Reset()` call (all playheads + shadow playheads to step 0). Simpler than Sequencer's Song Mode → Stop path; same end result.
18. **Performance page**: Port orbit engine from `sequencer.hh` and `seq_common.hh`. Adapt step lookup to use `orbit_step[ch]` instead of phase-based output. Add per-channel follow mask. Entry/exit gesture matches Sequencer (Fine/Copy + Glide hold 1.5s).

---

## Resolved Decisions

| Topic | Decision |
|---|---|
| Save behavior | Mirror Sequencer mode save behavior exactly (same trigger events, same mechanism) |
| Play/Reset button | Press = play/stop; SHIFT+PLAY = reset all playheads (matches Sequencer: SHIFT→global settings→PLAY→Song Mode→release SHIFT = Stop+Reset; VoltSeq shortcuts this to a direct Reset() call) |
| BPM setting | SHIFT + enc 7 (BPM/Clock Div) = adjust internal BPM; Tap Tempo button single-tap = tap tempo. Both match Sequencer conventions. |
| Start step | Removed from ChannelSettings. Phase rotation (enc 2, destructive) covers the use case. Enc 5 = output delay for all channel types. |
| SHIFT+GLIDE | In Sequencer enters Probability mode. In VoltSeq enters Ratchet step editor (channel type determines effect). Intentional departure; Probability mode does not apply to VoltSeq. |
| Copy/paste gesture | Matches Sequencer: Fine/Copy just-pressed while step held = copy; Fine/Copy held then step pressed = paste. Same `c.button.fine` button, same timing distinction. |
| GLIDE without page button | Encoder N = destructive per-channel offset: CV → slew_time, Gate → all-step gate width, Trigger → pulse_width_ms. |
| Mode switch | Hold all 3 left-cluster buttons (Fine+Play+Glide) = switch to Controller-panel mode; hold all 3 right-cluster buttons (Shift+Tap+Chan) = switch to Sequencer-panel mode. Works from any UI state. Replaces SHIFT+CHAN long-hold modeswitcher — that mechanism only fired from Channel Settings and gave no directional control. |
| Gate output voltage | Fixed 10V, same as Sequencer mode. Not configurable per channel. |
| Trigger pulse width | 10ms default; configurable per channel via enc 3 (Range) in Channel Edit (1–100ms). |
| Trig/Gate output delay | Per-channel, 0–20ms, set via enc 5 (Start) in Channel Edit for Gate/Trigger channels. Allows CV to settle before the trig/gate fires (needed for digital voices like Akemie's Taiko, BIA, etc.). CV channels fire immediately; only Gate/Trigger outputs are delayed. |
| Slider recording window | Defined by two discrete per-channel values: base (−5, 0, 1–10 V) and span (1, 2, 3, 4, 5, 10, 15 V). Set via enc 4 (Transpose) and enc 3 (Range) while CV channel is armed. Independent of channel output range. |
| Reset (hardware jack + Play/Reset long-hold) | Resets all channel playheads **and** shadow playheads to their `start` step. Does **not** disarm armed channels — disruptive, especially at short reset intervals. |
| Shadow playhead during arm / settings | Paused while any channel is armed or while Channel Edit / Shift-Settings is active. No meaningful use in those states; avoids silent drift on return. |
| Gate amplitude per channel | Not configurable. enc 3 reserved for a future version if needed. |
| Recorder::Data buffer size | Keep as-is (2048 × uint16_t). Oversized for 64 steps but costs nothing within existing allocation. |
| Step probability scope | Probability check gates the **entire step** — if the step misses, no output fires at all, including all ratchet sub-steps. |

---

## Open Questions

- **Trigger pulse width encoding in `ChannelSettings`**: `uint8_t pulse_width_ms` stores 1–100ms directly. Confirm the DAC/GPIO output path can sustain a 1ms pulse at 3kHz main loop rate (1ms = 3 ticks). If not, floor at 3–4ms or use a hardware timer for the pulse.
