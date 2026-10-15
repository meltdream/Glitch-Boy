# Source and Relinking Offer

This file is intended to travel with firmware releases that contain the
Nofrendo NES core.

## GPLv3 note

The firmware as a whole is distributed under the GNU GPL version 3 or later (see
`LICENSE` and `THIRD_PARTY_NOTICES.md`). Giving the recipient the complete
buildable source, as described below, covers both the GPLv3 and the Nofrendo
LGPL v2 obligations. For devices that ship with the firmware installed, put the
URL of the source repository and the exact release or commit in the manual or
the box.

## Covered Component

The Nofrendo core under `firmware/GlitchBoy/src/nofrendo/` is distributed under the GNU Library
General Public License version 2, as stated in its source headers.

The full LGPL v2 text is included at:

- `licenses/LGPL-2.0.txt`

This offer applies to Nofrendo only. Walnut-CGB and minigb_apu use the MIT
license and do not require LGPL relinking materials. Their notices are covered
by `THIRD_PARTY_NOTICES.md` and the MIT files under `licenses/`.

## Source Availability

For a firmware release that includes Nofrendo, provide recipients with the
corresponding source code for the exact firmware version shipped, including:

- the Nofrendo source under `firmware/GlitchBoy/src/nofrendo/`
- all local changes made to that source
- the application source that links with it
- build scripts, project files, and configuration needed to reproduce the
  firmware build

If the source is distributed by download, the download location should identify
the exact release, tag, or commit used for the shipped firmware.

## Relinking Materials

Because this is embedded firmware and Nofrendo is linked into the firmware
image, a release should let recipients rebuild or relink the firmware using a
modified Nofrendo core.

For this project, that means shipping or publishing:

- the complete buildable source tree for the released firmware
- the Arduino/ESP32 board package and library version information needed to
  build it
- any generated configuration files required by the build
- object files only if the shipped release withholds application source needed
  to relink

When the full firmware source and reproducible build instructions are shipped,
that is the preferred route for satisfying the relinking requirement.

## Written Offer Template

If a binary-only firmware image is distributed separately from the source, place
this offer in the release notes or product documentation:

```text
This product includes the Nofrendo NES core, which is distributed under the
GNU Library General Public License version 2.

For at least three years from the date of distribution, we will provide the
corresponding source code and relinking materials for the Nofrendo-containing
firmware release, including our modifications to Nofrendo and the materials
needed to rebuild or relink the firmware with a modified Nofrendo core.

The LGPL v2 license text is included with this release as licenses/LGPL-2.0.txt.
```

Replace this template with the actual contact address or download URL before a
public product release.
