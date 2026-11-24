# Garden

A chord player for the OP-1 Field, as an [open-field](https://github.com/LaP0573/open-field) pack for OS 1.7.3
(open-field 0.12 or later).

![Garden](docs/garden.gif)

Unofficial. Not made, endorsed or supported by Teenage Engineering, and not affiliated with them or with open-field.
Use at your own risk; see [LICENSE](LICENSE).

## Try it in a browser

[Garden in the browser](https://seajaysec.github.io/garden-web/) runs the pack's own chord engine and screen: Garden
synth on a simple synth, Garden drum on 909 and 808 voices, both at once on one clock, with a coach that explains what
is happening. Play it on screen, with the computer's keys, any MIDI keyboard, or a Field in MIDI controller mode (knobs
set to relative; hold Help for Shift, tap it for Help). On an iPhone or iPad, Share › Add to Home Screen. If the unit
does something different from the web, **Report a difference** opens an issue with the exact state attached. Its
source is [garden-web](https://github.com/seajaysec/garden-web) (GPL-3.0, for the drum voices).

## Install

Put `garden.pack` (in this repository, or from [releases](../../releases)) in the unit's `packs` folder next to open-field's `base.pack`, and
restart. Hold **Help** while switching on to start without packs.

## Play

**Shift + Sequencer** lists **garden**, on the synth and on the drum. Hold a piano key to play the chord on that root;
more keys play notes over it.

| Key | Page | Blue | Tan | Gray | Orange |
| --- | --- | --- | --- | --- | --- |
| T1 | play | cascade; Shift: echo (push: voice leading) | style; Shift: rate (push: chord banks) | voicing, order or phrase; Shift: the other (push: latch) | Boost; Shift: pulse (push: extension banks) |
| T2 | rhythm | loop; Shift: ending | length; Shift: cycle | swing; Shift: downtime | human |
| T3 | palette | chord bank A | chord bank B | extension bank A | extension bank B |
| T4 | gardens | garden (push: plant) | root | kind | extension square (push: next) |

- Keys 1–4: chord kinds. Keys 5–8: extensions. Tap latches, hold is momentary.
- T3 push: the next unused cell. Shift + push: swap or reset the banks.
- T4: keys 1–8 play the gardens, Shift + key plants the chord there. Shift: blue walk length, tan walk order, blue push
  walks, Shift + tan push loads the next of four demos (bloom, night, drift, run): four gardens and a style to walk,
  a synth half and a drum half meant to play together. On the drum, Shift + gray push sows the empty gardens with
  something random, Shift + orange push reaps them all.
- **Latch** (gray push on T1) keeps a chord going after you let go: an arp, farp, pattern or harp repeats, a chord or
  strum rings. Press the same key again, or push gray, to let it go.
- **Ending** (T2, Shift + blue): how a chord let go ends: cut, finish its pass (the first way), or finish and land on
  the root for a step, 1/4, 1/2 or a bar.
- **Cycle** (T2, Shift + tan): how often a pattern starts over: free, every 1/2 bar, bar or 2 bars. **Downtime** (T2,
  Shift + gray): what a line shorter than its cycle does once played: repeat, rest, or hold its last note.
- **On the drum** Garden plays the drum's 24 keys. **Drum mode** (the first): a chord's notes hit keys across the
  kit, octaves land on the other half, and the styles read hit, flam, drag, loose, roll, run, groove, sweep.
  **Shift + tan push on T1**: **melodic mode**, Garden as on the synth, for one sound laid a semitone a key.
- **Help** shows the controls of the page you're on.

## Build

Set `OPEN_FIELD`, `OPEN_XY` and `TOOLCHAIN` in `local.env` ([open-xy](https://github.com/etcetc000/open-xy) provides
the toolchain), then run `./build.sh`. The pack is written to `out/garden.pack`.
