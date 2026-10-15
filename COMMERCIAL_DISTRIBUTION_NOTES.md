# Commercial Distribution Notes

This is the release-facing checklist.

## Current Practical Path

For a commercial release of the NES and Game Boy firmware:

1. Keep the firmware source available for the exact version shipped.
2. Keep all upstream copyright and license notices intact.
3. Include `THIRD_PARTY_NOTICES.md`, `LICENSE` and `licenses/GPL-3.0.txt`. The firmware image is GPLv3 as a whole (see `LICENSE`), so every binary you ship needs the matching source available. Selling devices with the firmware installed counts as distribution.
4. Include `licenses/LGPL-2.0.txt` for Nofrendo.
5. Include `licenses/MIT-Walnut-CGB.txt` and `licenses/MIT-minigb_apu.txt`
   when the release contains Game Boy support.
6. Include `SOURCE_AND_RELINKING_OFFER.md` or equivalent release text.

## Nofrendo

Nofrendo is under GNU Library General Public License version 2. Commercial
distribution is possible, but firmware releases that include Nofrendo need the
LGPL v2 materials described in `SOURCE_AND_RELINKING_OFFER.md`.

## Walnut-CGB and Game Boy Audio

The Game Boy and Game Boy Color path contains Walnut-CGB, which is based on
Peanut-GB and includes portions identified in its header as coming from
SameBoy. The bundled Game Boy audio implementation is minigb_apu, based on
MiniGBS. These components use the MIT license.

Commercial distribution is permitted by the MIT terms, provided the relevant
copyright and permission notices travel with copies or substantial portions of
the software. Ship the two MIT files listed above, keep the notices in the
source files, and include `THIRD_PARTY_NOTICES.md` in the release materials.

## VidPix Visualizer

The music visualizer files (`midi_visualizer.cpp`, `vidpix_data.h`, `vidpix_vector_font.h`) are
GPL-3.0-or-later, ported from VID PIX PAL v1.2.2. They are why the firmware as a whole is distributed under GPLv3. Ship `licenses/GPL-3.0.txt` and `licenses/MIT-Parallax-Graphics.txt` (the font data comes from Parallax's Graphics.spin) and keep the notices in the files.

This file is an engineering compliance aid, not legal advice.
