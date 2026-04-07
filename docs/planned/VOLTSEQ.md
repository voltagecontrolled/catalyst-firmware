# VoltSeq — Catalyst as a Step Sequencer

Replaces Macro mode in one of two release variants (see Release Strategy below). The mode-switch shortcut (Sequencer ↔ Macro) becomes Sequencer ↔ VoltSeq. All existing Macro data is abandoned on first boot; bump `Macro::Data::current_tag` (or rename it) to force a clean init. Consider renaming the `Macro` enum value in `params.shared.mode` to `VoltSeq` in place — same underlying integer, no flash migration needed.

## Release Strategy

Two WAV files will be published per release:

| Variant | Second mode | Filename |
|---|---|---|
| `catalyst-vX.Y.Z-voltseq.wav` | VoltSeq (this spec) | For users who want the step sequencer |
| `catalyst-vX.Y.Z-classic.wav` | Original Macro mode | For users who want Sequencer improvements but prefer Macro |

Both variants are built from the **same commit** using a compile-time flag in `conf/build_options.hh` (e.g. `SECOND_MODE_VOLTSEQ` vs `SECOND_MODE_MACRO`). `MacroSeq` in `src/app.hh` delegates to `VoltSeq::App` or `Macro::App` at compile time based on this flag. No separate branches needed.

The GitHub Actions release workflow (`release.yml`) will need a small update to run two build passes per tag and attach both WAVs to the release.

Flash note: the two variants share the same `Macro::Data` storage slot. Switching between them (flashing one then the other) will trigger a clean init and wipe that slot's data. This is expected and acceptable — users will generally pick one variant and stay on it.

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
| Play/Reset button | Short press: play/stop; long press: soft reset all playheads to step 0 |
| Shift | Global settings modifier; page navigation modifier |
| Chan. / Quantize | Arm channel modifier (CHAN + page button); Channel Edit entry (Shift + CHAN) |
| Tap Tempo | Internal BPM (when no clock patched); 3+ taps sets tempo |
| Glide (Ratchet) | Glide modifier |
| Fine | Fine encoder adjustment (1-unit steps) while editing |
| Copy | Step copy/paste |

Encoders are **not** press encoders. All interactions are rotation only.

---

## Step and Page Model

- **Page**: 8 steps. Navigate via **Shift + Pages button 1–8**.
- **Global step index** (0-based): `page × 8 + step_within_page`, indices 0–63.
- **Per-channel length**: Independent step count per channel (1–64). Channels wrap at their own length, enabling polyrhythms.
- **Per-channel start step**: First active step, 0-based global index.
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

## Glide

### Per-Step Glide Toggle

Hold **Glide** + press a **Pages button** → toggle the glide flag for that step across all channels. Page button LED dimly lit when glide is on for that step; off = no glide.

### Per-Channel Glide Amount

Hold **Glide** + turn **encoder N** → set slew time for channel N (0 = off; max ≈ several seconds). Uses one `Slew::Interface` instance per channel. When a step's glide flag is set, the channel slides from the previous value to the new value over the configured slew time instead of jumping.

Gate and Trigger channels ignore glide (slew is bypassed).

---

## Copy / Paste

- Hold **Copy** + press **Pages button A** → copy all 8 channel values for step A into clipboard.
- Hold **Copy** + press **Pages button B** → paste clipboard into step B.
- Fine + Copy while holding a page button → paste into all currently held steps.

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
| 5 | Start | Start step (global index 0–63) | Output delay: 0–20ms (CV settles before gate fires) | Output delay: 0–20ms (CV settles before trigger fires) |
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
    uint8_t              start           = 0;     // 0..63 global step index
    Clock::Divider::type division        = {};    // reuse existing type
    Channel::Cv::Range   range           = {};    // output range (encoder editing); CV only
    int8_t               slider_base_v   = 0;     // slider recording lower limit V: −5,0,1..10; CV only
    uint8_t              slider_span_v   = 5;     // slider recording span V: 1,2,3,4,5,10,15; CV only
    Channel::Mode        scale           = {};    // reuse: Off/Chromatic/scales/Gate/Trigger
    Direction            direction       = Direction::Forward;
    float                random_amount   = 0.f;
    float                glide_time      = 0.f;   // seconds; 0 = disabled; CV only
    uint8_t              pulse_width_ms  = 10;    // trigger pulse width in ms; Trigger only
    uint8_t              trig_delay_ms   = 0;     // output delay in ms; Gate and Trigger only
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

1. **Clock engine**: Rising-edge detection on Clock In; internal timer at `internal_bpm` when unpatched. Master tick counter drives per-channel division counters.
2. **Per-channel playhead + shadow playhead**: Shadow advances unconditionally each tick; real playhead copies shadow when no steps are held. Shadow playheads are **paused** while any channel is armed or while Channel Edit / Settings mode is active — they serve no purpose in those states and pausing avoids silent drift that would surprise the user on return to normal playback.
3. **Direction state machine**: Adds `int8_t ping_pong_dir[NumChans]` to runtime state (not stored).
4. **Step-lock and arpeggiation**: Sorted held-step list; cycle through on each tick.
5. **Arm state machine**: Tracks which channel (if any) is armed, which type it is, and routes page buttons / slider / encoders accordingly.
6. **Slider-to-step recorder**: On each tick while a CV channel is armed, write `map(slider_adc, 0..4095, slider_base_v * V_scale, (slider_base_v + slider_span_v) * V_scale)` → quantize → write to `steps[page][step][ch]`. Replaces `Recorder::Interface` playback logic (buffer reused, state machine replaced).
7. **x0x step toggle + value edit**: Short-press toggles gate/trigger step on/off; hold + encoder sets gate length or ratchet/repeat count.
8. **Ratchet generator**: Subdivide step window into N equal pulse slots; fire triggers at each slot boundary.
9. **Repeat handler**: When repeat depth R > 0, hold playhead for R additional ticks before advancing.
10. **Per-channel glide**: 8 `Slew::Interface` instances; bypassed when `glide_time == 0` or step's glide flag is off.
11. **Phase rotation**: In Channel Edit, enc 2 detents call a `RotateChannel(ch, delta)` function that `std::rotate`s the relevant slice of `steps` for that channel across all pages.
12. **Destructive phase rotate across pages**: `RotateChannel` treats the channel's full 64-step logical sequence as a flat array, rotates it, and writes back into the page-indexed structure.
13. **Channel Edit UI**: New UI state class analogous to `Macro::Settings`.
14. **Tap Tempo**: Running average of last 4 inter-tap intervals → `internal_bpm`.
15. **Internal clock generator**: Timer ISR or tick-counter when Clock In is unpatched.

---

## Resolved Decisions

| Topic | Decision |
|---|---|
| Save behavior | Mirror Sequencer mode save behavior exactly (same trigger events, same mechanism) |
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
