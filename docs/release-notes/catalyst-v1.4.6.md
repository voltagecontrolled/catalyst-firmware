**Note: this is a pre-release firmware, use at your own risk. This is not supported or endorsed by 4ms Company and flashing third party firmware could void your warranty or render your module inoperable. The author assumes no liability for any damage resulting from use of this firmware.**

---

## Phase Scrub Performance Page (v1.4.6)

**Entry:** hold Copy + Glide for 1.5 seconds. **Exit:** Copy + Glide (any duration) or Play/Reset.  
**Lock toggle:** Copy + Glide, release either button before 1.5s (fires on release; continuing to hold enters the menu).

A dedicated performance page replacing the basic lock toggle combo. All settings persist across reboots.

| Encoder | Function | Colors |
|---------|----------|--------|
| 1 | Quantize | orange = on, off = off |
| 2 | Slider Performance Mode | off / green / blue / cyan |
| 3 | Granular Width *or* Debounce Delay | dim→bright orange / dim→bright white |
| 4 | Orbit Direction | green / blue / orange / lavender |
| 8 | Phase Scrub Lock | red = locked, off = unlocked |

**Page buttons:** toggle per-track scrub participation. Lit = track follows scrub, unlit = track ignores scrub and plays normally.

**Quantize (Enc 1):** in standard/granular mode, snaps the scrub phase to the nearest step boundary. In beat repeat, controls entry timing: off = fires immediately on SHIFT release (precise, timing-dependent); on = snaps to nearest step boundary (max half-step wait, always musical).

**Slider Performance Mode (Enc 2):**
- **Off:** standard phase scrub
- **Green — Granular:** slider positions a looping orbit window within the sequence. Enc 3 sets window width (0–100%); Enc 4 sets playback direction (green=fwd, blue=bck, orange=ping-pong, lavender=random). Width=0 or slider at minimum disables orbit.
- **Blue — Beat Repeat (8 zones, triplets):** slider selects a subdivision rate from 8 zones (1/2, 1/4, 1/4T, 1/8, 1/8T, 1/16, 1/16T, 1/32). Enc 3 sets debounce delay (50ms–1500ms, default 150ms, transient). Slider at minimum turns beat repeat off.
- **Cyan — Beat Repeat (4 zones, no triplets):** wider zones for easier live targeting (1/2, 1/4, 1/8, 1/16). Enc 3 same as Blue. Enc 4 unlit/inactive in both beat repeat modes.

**Beat repeat SHIFT staging:** Hold SHIFT to freeze the active division and looped step — slider moves freely without scrubbing or committing a new zone. Release to commit. Cleanest way to return to zero without chaos in transit.

**Beat repeat entry:** starts on the next master clock tick after entry (step-grid aligned). The first step that fires becomes the looped step automatically — release SHIFT when you hear the hit you want, and it locks to that beat.

Phase Scrub Lock (Enc 8) works in all modes. Beat repeat and granular both respect the per-track scrub ignore mask.

## Phase Scrub lock persistence (v1.4.6)

Phase Scrub lock state and locked slider position now survive power cycles. On boot with lock restored, the slider enters pickup mode automatically.

## Sub-step page navigation (v1.4.6)

**Combo:** Shift + Page button while in sub-step mask edit mode.

Changes page without exiting sub-step edit mode. The focused step clears so you can focus a step on the new page.
