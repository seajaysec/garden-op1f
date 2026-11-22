# Garden

A chord player for the OP-1 Field, as an [open-field](https://github.com/LaP0573/open-field) pack for OS 1.7.3
(open-field 0.12 or later).

![Garden](docs/garden.gif)

Unofficial. Not made, endorsed or supported by Teenage Engineering, and not affiliated with them or with open-field.
Use at your own risk; see [LICENSE](LICENSE).

## Try it in a browser

[Garden in the browser](https://seajaysec.github.io/garden-op1f/) runs the pack's own chord engine and screen with a
simple synth. Play it on screen, with any MIDI keyboard, or with a Field in MIDI controller mode (knobs set to
relative; hold Help for Shift, tap it for Help). On an iPhone or iPad, Share › Add to Home Screen. If the unit does
something different from the web, **Report a difference** opens an issue with the exact state attached.

## Install

Put `garden.pack` (in this repository, or from [releases](../../releases)) in the unit's `packs` folder next to open-field's `base.pack`, and
restart. Hold **Help** while switching on to start without packs.

## Play

**Shift + Sequencer** lists **garden**. Hold a piano key to play the chord on that root; more keys play notes over it.

| Key | Page | Blue | Tan | Gray | Orange |
| --- | --- | --- | --- | --- | --- |
| T1 | play | cascade; Shift: echo (push: voice leading) | style; Shift: rate (push: chord banks) | voicing, order or phrase; Shift: the other (push: hold) | Boost; Shift: pulse (push: extension banks) |
| T2 | rhythm | loop | length | swing | human |
| T3 | palette | chord bank A | chord bank B | extension bank A | extension bank B |
| T4 | gardens | garden (push: plant) | root | kind | extension square (push: next) |

- Keys 1–4: chord kinds. Keys 5–8: extensions. Tap latches, hold is momentary.
- T3 push: the next unused cell. Shift + push: swap or reset the banks.
- T4: keys 1–8 play the gardens, Shift + key plants the chord there. Shift: blue walk length, tan walk order, blue push walks.
- **Help** shows the controls of the page you're on.

## Build

Set `OPEN_FIELD`, `OPEN_XY` and `TOOLCHAIN` in `local.env` ([open-xy](https://github.com/etcetc000/open-xy) provides
the toolchain), then run `./build.sh`. The pack is written to `out/garden.pack`.
