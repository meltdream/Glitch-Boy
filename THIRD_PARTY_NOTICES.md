# Third-Party Notices

This repository is a mixed-license codebase. The top-level `LICENSE` covers
only Glitch Boy project-owned files and additions that do not already carry a
different upstream notice.

Third-party files keep their original upstream licenses and notices. Keep those
notices intact when distributing source or firmware binaries.

## License Matrix

| Component | Paths | License status from file headers | Release note |
| --- | --- | --- | --- |
| Glitch Boy project-owned additions | project-owned files without separate upstream notices | ISC-style, per top-level `LICENSE` | Permissive on their own; see "How the licenses combine" below for the firmware as a whole |
| Peter Barrett `esp_8_bit` foundation | `firmware/GlitchBoy/GlitchBoy.ino`, `firmware/GlitchBoy/src/emu*.cpp`, `firmware/GlitchBoy/src/gui.cpp`, `firmware/GlitchBoy/src/video_out.h`, `firmware/GlitchBoy/src/hid_server/*`, `firmware/GlitchBoy/src/ir_input.h`, and similar files carrying the same notice | ISC-style permissive notice in-file | Keep the notices and acknowledge the project lineage |
| Walnut-CGB, based on Peanut-GB and containing marked SameBoy-derived portions | `firmware/GlitchBoy/src/walnut/walnut_cgb.h` and the mechanically generated `walnut_cgb_threaded_*` headers | MIT, per the Walnut-CGB license and the notice in `walnut_cgb.h` | Keep the Mr. Paul, Mahyar Koshkouei, and Lior Halphon notices with source and binary distributions |
| minigb_apu, based on MiniGBS | `firmware/GlitchBoy/src/walnut/minigb_apu.c`, `firmware/GlitchBoy/src/walnut/minigb_apu.h` | MIT, per the source header and upstream project | Keep the Mahyar Koshkouei and Alex Baines notices with source and binary distributions |
| Nofrendo core | `firmware/GlitchBoy/src/nofrendo/*` | GNU Library General Public License v2, per file headers | Commercial use is possible, but LGPL v2 obligations apply |
| VidPix music visualizer port, derived from VID PIX PAL v1.2.2 by Synthrotek Sound Study Modular | `firmware/GlitchBoy/src/midi_visualizer.cpp`, `firmware/GlitchBoy/src/vidpix_data.h`, `firmware/GlitchBoy/src/vidpix_vector_font.h` | GNU General Public License v3 or later (SPDX tag in each file) | Keep the notices. This is the component that makes the firmware as a whole GPLv3 (see below) |
| Parallax Graphics Driver font data | `firmware/GlitchBoy/src/vidpix_vector_font.h` (generated from the font in `Graphics.spin`, which ships with VID PIX) | MIT, Copyright (c) 2005 Parallax, Inc. | Keep `licenses/MIT-Parallax-Graphics.txt` with the source |
| Bundled sample games | `firmware/GlitchBoy/src/media.h` (Postie for Game Boy Color by invertedHat, Sliding Blaster for NES by NovaSquirrel) | Per the author permissions recorded in `licenses/Postie-GB.txt` and `licenses/Sliding-Blaster-NES.txt` | Keep those files with the source |

## Bundled License Texts

This repository includes these release license texts:

- `licenses/LGPL-2.0.txt`
- `licenses/GPL-3.0.txt`
- `licenses/MIT-Parallax-Graphics.txt`
- `licenses/MIT-Walnut-CGB.txt`
- `licenses/MIT-minigb_apu.txt`
- `licenses/Postie-GB.txt`
- `licenses/Sliding-Blaster-NES.txt`

The Walnut directory also has a local component map at
`firmware/GlitchBoy/src/walnut/README.md`. The copyright and provenance notices
inside the source files remain authoritative and should not be removed.

## How the licenses combine

The firmware image links everything above into one binary, so one license has to
cover the image as a whole. These are the pieces:

- ISC-style and MIT code can be included in a GPLv3 work.
- The visualizer files are GPL-3.0-or-later. Their upstream header allows any
  later version of the GPL, so GPLv3 is the license the whole image is
  distributed under.
- Nofrendo's headers say "version 2" of the LGPL, with no "or later". Section 6
  of that license lets a work that uses the library be combined with it and
  distributed under other terms, as long as those terms allow modification and
  reverse engineering for debugging. GPLv3 does. Nofrendo's own files stay
  under the LGPL v2, and the relinking and source obligations below still
  apply to them.
- Distributing the firmware (including a device that ships with it installed)
  therefore means: GPLv3 for the image, the full corresponding source, the
  license texts in `licenses/`, and all the notices kept intact.

GPLv3 also asks for installation information when the firmware is delivered in
a consumer product. Glitch Boy can be reflashed over USB and has no signing or
lock, so users can install their own builds; keep it that way.

If a permissive or LGPL-only firmware is ever wanted, the visualizer files
would have to be replaced with code that is not derived from VID PIX.

This is an engineering note, not legal advice.

## Nofrendo LGPL v2 Distribution Notes

Firmware binaries that include the Nofrendo core should be distributed with:

- a copy of `licenses/LGPL-2.0.txt`
- this `THIRD_PARTY_NOTICES.md` file
- the corresponding source for the exact Nofrendo version used
- any local modifications to Nofrendo
- the build scripts, configuration, and object/source materials needed to
  relink the firmware with a modified Nofrendo core

The practical source/relinking offer for this repository is in
`SOURCE_AND_RELINKING_OFFER.md`.
