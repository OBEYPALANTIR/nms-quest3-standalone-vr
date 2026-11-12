# No Man's Sky VR on a standalone Quest 3

An experiment in running the Windows version of No Man's Sky in VR on a stock
Meta Quest 3, on the headset itself, with no PC streaming. It uses
[WinlatorXR](https://github.com/WinlatorXR/WinlatorXR) (Wine and Box64 on
Android) and a patched copy of its OpenXR runtime,
[OpenXR-WXR](https://github.com/WinlatorXR/OpenXR-WXR).

## Status

- On the project's own headset the game reaches a planet in VR at about
  **32 fps** (measured range 30 to 33 fps; the headset shows 72 Hz with
  repeated frames), with head tracking and controller input. It is not stable:
  launches sometimes exit early, and long sessions have frozen.
- **This repository is not a complete setup.** The Vulkan memory shim and the
  Steam launch step that the working stack needs are not published (see
  [docs/SETUP.md](docs/SETUP.md#what-is-not-included)). What is here
  will not, on its own, get the game running.
- The setup guide has **never been tested on a clean headset**.
- `tools/openxr-wxr/fetch-inputs.sh` has **never been run against the real
  download URLs**; it was only tested against local copies of the recorded
  inputs.

## What is here

| Path | What it is |
| --- | --- |
| [docs/SETUP.md](docs/SETUP.md) | Setup guide for the published parts, with each step marked verified or not |
| [tools/openxr-wxr](tools/openxr-wxr) | Patches to OpenXR-WXR (stereo IPD, tracking merge, eye size, input priority) with a reproducible build pinned by SHA-256 and host-side checks |
| [tools/nms-globals](tools/nms-globals) | A checked editor for game globals exported to EXML, and the QuestLowLOD3 recipe (field names and values only) |
| [tools/nms-halftex](tools/nms-halftex) | Builds the QuestHalfTex mod (drops the top mip of listed textures) from your own game files; the list holds file paths only |

## You need your own game

You need your own, legally bought copy of No Man's Sky on Steam, signed in with
your own account. This repository holds no game files, no built mods, no
decompiled code and nothing that removes copy protection. Please keep it that
way in contributions: do not commit game files, saves, crash dumps, raw game
logs or Steam login files (`.gitignore` blocks the common ones).

## Licence

GPL-3.0-or-later; see [LICENSE](LICENSE). The patches in `tools/openxr-wxr`
modify GPL-3.0 code from OpenXR-WXR and are distributed under that upstream
licence, which is in [tools/openxr-wxr/LICENSE](tools/openxr-wxr/LICENSE).

## Not affiliated

This project is not affiliated with or endorsed by Hello Games, Valve or Meta.
No Man's Sky, Steam, Meta Quest and other trademarks belong to their owners.

## Security

See [SECURITY.md](SECURITY.md).
