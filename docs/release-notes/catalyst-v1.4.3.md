**_Note: this is a pre-release firmware, use at your own risk. This is not supported or endoorsed by 4ms Company and flashing third party could void your warranty or render your module inoperable. The author assumes no liability for any damage resulting from use of this firmware_**

**_WARNING: Old presets from previous releases will NOT work with this firmware._** 

v1.4.1
- Added Phase Scrub slider lock (COPY+GLIDE). Locks the slider at its current position; a second press unlocks with pickup behavior so the playhead doesn't jump. Encoder 8 LED blinks red while locked or during pickup.

v1.4.2
- Extended ratchet count from 4x to 8x on gate tracks
- Added step repeats on gate tracks: turn the encoder CCW instead of CW while holding Glide to add repeats rather than ratchets. Repeats extend the effective pattern length rather than subdividing the step. Ratchets display dim salmon → bright red; repeats display dim teal → blue.

v1.4.3
- Added a sub-step editor for gate tracks (GLIDE+TAP TEMPO to toggle). While in sub-step edit mode:
  - Touch any step encoder to focus it, its sub-step mask displays on the Page buttons
  - Page buttons toggle individual sub-steps on or off (sub-step 1 always fires)
  - Continue turning the focused encoder within 300ms to adjust ratchet/repeat count
  - Focused step blinks yellow; adjusting count resumes normal ratchet/repeat color feedback
