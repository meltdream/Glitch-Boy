# Walnut-CGB integration

This directory contains the Game Boy and Game Boy Color emulation core and its
audio support. It combines upstream MIT-licensed code with the small adapters
Glitch Boy needs to connect that code to the rest of the firmware.

## File map

| Files | Role | Provenance |
| --- | --- | --- |
| `walnut_cgb.h` | Game Boy and Game Boy Color emulation core | Walnut-CGB by Mr. Paul, based on Peanut-GB by Mahyar Koshkouei; the header marks portions taken from SameBoy by Lior Halphon |
| `walnut_cgb_threaded_dual_first.h`, `walnut_cgb_threaded_dual_second.h` | Threaded dual-fetch opcode bodies | Mechanically generated from `walnut_cgb.h` |
| `walnut_cgb_threaded_dual_first_dispatch.h`, `walnut_cgb_threaded_dual_second_dispatch.h` | Dispatch tables for the generated opcode bodies | Mechanically generated from the corresponding threaded headers |
| `minigb_apu.c`, `minigb_apu.h` | Game Boy audio processing unit | minigb_apu by Mahyar Koshkouei, based on MiniGBS by Alex Baines |
| `walnut_cgb_core.c` | Glitch Boy adapter that binds Walnut-CGB audio access to the active minigb_apu context | Local integration code |

The generated threaded headers should be regenerated from their source when
the core changes, rather than edited independently.

## Upstream projects

- [Walnut-CGB](https://github.com/Mr-PauI/Walnut-CGB)
- [Peanut-GB](https://github.com/deltabeard/Peanut-GB)
- [SameBoy](https://github.com/LIJI32/SameBoy)
- [minigb_apu](https://github.com/deltabeard/minigb_apu)
- [MiniGBS](https://github.com/baines/MiniGBS)

## License

Walnut-CGB and minigb_apu are distributed under the MIT license. Keep the
copyright and license notices in `walnut_cgb.h` and `minigb_apu.c` intact. The
repository also bundles release-facing copies at:

- `licenses/MIT-Walnut-CGB.txt`
- `licenses/MIT-minigb_apu.txt`

Glitch Boy's top-level `LICENSE` applies only to project-owned integration code
that does not carry a different upstream notice.
