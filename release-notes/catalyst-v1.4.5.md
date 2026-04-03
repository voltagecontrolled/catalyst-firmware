**Note: this is a pre-release firmware, use at your own risk. This is not supported or endorsed by 4ms Company and flashing third party firmware could void your warranty or render your module inoperable. The author assumes no liability for any damage resulting from use of this firmware.**

## Linked Tracks: new follow modes and quirk fixes (v1.4.5)

Two new follow modes have been added:

- **Teal -- Gate clock, step only:** the CV track advances once per gate step, ignoring ratchets and repeats. Useful when a ratcheted gate track drives rhythm but you want steady one-pitch-per-step CV movement.
- **Lavender -- CV replace:** replaces this track's step with the source track's pitch whenever both tracks step simultaneously (intersection) and the source step is non-zero. Steps at 0V on the source are treated as pass-through, leaving the target's own programmed pitch intact. Use this for hard pitch coupling at step intersections, or combine with random playback on the source track and a curated set of non-zero pitches to add structured randomness to a sequence.

The page button quirk when switching between gate clock follow modes has also been fixed. Previously the button for an assigned track would remain lit when switching modes, and pressing it would not correctly deactivate the old assignment. Page buttons now only light in the mode where the assignment was made, and pressing a lit button correctly unlinks.

## Bugfixes and small changes (v1.4.5)

- Fixed an issue where entering Channel Settings (SHIFT + Chan.) would sometimes immediately jump to Follow Assign mode without requiring a Glide tap. Caused by Glide button bounce or overlap during the entry combo.
- Sub-step 1 can now be silenced in the sub-step mask editor. Previously it was forced on. Silencing all sub-steps on a step produces no output for that step's duration, enabling microtiming effects (e.g. a 2x ratchet with only sub-step 2 active fires halfway through the step).
