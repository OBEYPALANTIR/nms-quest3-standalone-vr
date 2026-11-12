# Set up No Man's Sky VR on your own Quest 3

This guide covers the parts of the project that are published here: the
patched OpenXR runtime, two performance mods you build from your own game
files, and the container and runtime settings the project uses. It runs No
Man's Sky (NMS) in VR on the headset itself, through WinlatorXR (Wine and
Box64).

**This repository is not a complete, ready-to-play setup.** Two parts of the
working stack are not included (see [What is not included](#what-is-not-included)):
the Vulkan memory shim and the Steam launch step. Without them, this guide does
not get you to a running game.

**Status.** Nobody has done a clean install on a fresh headset from this guide.
Every step carries one of two marks:

- **[repo-verified]**: the build or check was run from the scripts in this
  repository on a Mac on 2026-11-11, and the output hash matched the pinned one.
- **[not verified on a clean device]**: this is what the project's own headset
  runs, written down from its run records. It has not been repeated on another
  headset. Expect problems, and please report them.

## What you need

- Your own, legally bought copy of No Man's Sky on Steam. You sign in with your
  own account. Nothing here removes copy protection or emulates Steam, and this
  repository holds no game files.
- A Meta Quest 3 in developer mode, with USB debugging allowed for your computer.
- A computer with `adb` (Android platform tools). The builds below were run on
  macOS (Apple silicon) with Homebrew.
- About 30 GB free on the headset for the game files.
- Build tools: `x86_64-w64-mingw32-gcc` / `g++` **16.2.0** and GNU ld
  **2.47.20260726** (the runtime build checks these exact versions so the output
  hash matches), Python 3, Bash and Git.

## The stack

| Part | What the project uses | Where it comes from |
| --- | --- | --- |
| Winlator | WinlatorXR `cmod` build **dawn-42** (versionName `dawn-42`, versionCode 24; APK SHA-256 `94416eafc629a3a6a8937d0098c577b6020f7a5f9e25f47e3338d5accdf7eedb`; source commit `39b270c6401a69e7c28ea6ffd61de3dbac9e9b1e` on branch `cmod_bionic`) | https://github.com/WinlatorXR/WinlatorXR (GPLv3/MIT, per its LICENSE). Release page link not confirmed. |
| Container | Container 1: `proton-9.0-x86_64` (ships inside the APK), Box64 **0.4.2** (classic, not FEX), Turnip graphics driver (recorded as "Turnip T26") | Chosen in the Winlator app |
| Game | NMS Windows build **183877**: Steam app 275850, depot 275851, manifest 7436204008759126530. `Binaries/NMS.exe` SHA-256 `d393c6782d29e1915854c0d5cf42505c8093740c05dc53705dbed7e0d1d4ac37` | Your own Steam library |
| OpenXR runtime | OpenXR-WXR with our patches, profile `text-priority-input` | `tools/openxr-wxr` (GPL-3.0, upstream https://github.com/WinlatorXR/OpenXR-WXR commit 5f7a5c4) |
| Mods | QuestLowLOD3 (fewer draws, about +7 % fps) and QuestHalfTex (smaller textures, saves memory) | Built from your own game files with `tools/nms-globals` and `tools/nms-halftex` |
| Vulkan shim | Not included | See below |
| Steam launch | Not included | See below |

**About the game build.** Steam updates NMS. A newer build may behave
differently, and the mod recipe refuses a build whose stock values differ.
Check your hash:

```sh
shasum -a 256 "<your NMS folder>/Binaries/NMS.exe"
```

If it differs, you are on a different build. That is worth reporting, but your
results are not directly comparable with ours.

## 1. Install WinlatorXR and set up Container 1 [not verified on a clean device]

1. Install the WinlatorXR dawn-42 APK on the headset with
   `adb install <apk>`. Check its SHA-256 against the table above first.
2. In the Winlator app, create Container 1 with the `proton-9.0-x86_64` Wine
   version, the classic Box64 0.4.2 emulator and the Turnip driver.
3. Turn **off** the Quest GPU profiler. If `debug.egl.profiler` is set, the
   profiler layer gets injected into the game and Wine's Vulkan loader asserts.
   Check with `adb shell getprop debug.egl.profiler` (should be empty or 0).
   If you used `ovrgpuprofiler`, disable it again with `ovrgpuprofiler -d`.
4. In the container's environment, `BOX64_DYNAREC_STRONGMEM=1` made a planet
   freeze go away in 8 of 8 long runs (about 2 fps slower).

## 2. Put the game files on the headset [not verified on a clean device]

Install NMS through Steam with your own account, then copy the whole game
folder to the headset, for example:

```sh
adb push "<your NMS folder>/." /sdcard/Download/NMS-Standalone-Test/
```

Winlator maps `D:` to the headset's `Download` folder. Check the `NMS.exe` hash
as above. Never copy anyone else's Steam files (`config`, `userdata`,
`*.vdf`, `ssfn*`).

## 3. Build the OpenXR runtime [repo-verified; input fetch verified offline only]

First assemble the build inputs (this downloads from GitHub):

```sh
tools/openxr-wxr/fetch-inputs.sh "$PWD/build/oxr-inputs"
```

| Input | Source | Confirmed? |
| --- | --- | --- |
| `src/` (4 files) | WinlatorXR/OpenXR-WXR commit `5f7a5c455050e320578539f9009333a336599659` | Yes, recorded in `tools/openxr-wxr/README.md` |
| `OpenXR-SDK/include` | KhronosGroup/OpenXR-SDK tag `release-1.1.43` | **Unconfirmed**: inferred from the headers' version 1.1.43 |
| `original-include/` (`loader_interfaces.h`, `xr_linear.h`) | `src/common/` of the same OpenXR-SDK checkout | **Unconfirmed** path and revision. The pinned `loader_interfaces.h` has the same hash as `include/openxr/openxr_loader_negotiation.h` in the recorded headers |
| `Vulkan-Headers/include` | KhronosGroup/Vulkan-Headers tag `v1.3.296` | **Unconfirmed**: inferred from `VK_HEADER_VERSION 296` |
| `libvulkan-1.a` | Generated locally with MinGW `dlltool` from `tools/openxr-wxr/vulkan-1.def` (no download) | Yes: links to the same pinned DLL |

The script checks every file against `tools/openxr-wxr/inputs.sha256`, so a
wrong tag or path fails loudly instead of building something different. If a
check fails on an unconfirmed source, open an issue with the failing file names.
**The script has never been run against the real GitHub URLs.** It was run
against local mirrors of the recorded inputs, and all hashes matched.

Then build and check:

```sh
tools/openxr-wxr/rebuild.sh "$PWD/build/oxr-inputs" "$PWD/build/oxr" text-priority-input
tools/openxr-wxr/check-host.sh "$PWD/build/oxr"
bash tools/openxr-wxr/check-eye-size.sh "$PWD/build/oxr"
```

Expected output: `rebuild.sh` ends with `openxr_wxr_64.textprio-input.dll: OK`
(the DLL has SHA-256
`cb299dbf848c64e29b051e8faf072340987fb44387b07bb47b6b4e0f2b239f65`,
1,518,592 bytes; compiler warnings before it are normal), `check-host.sh` ends
its first block with `RESULT PASS`, and `check-eye-size.sh` prints three `PASS`
lines. Each command exits with status 0. `tools/openxr-wxr/README.md` lists the
other build profiles and their checks.

This is the runtime the project plays with. It has an optional conf key,
`text_priority_input=true`, that lets scripted input replace the physical
controllers. **Do not put that line in the `conf.txt` you play with**: while a
scripted input stream is active, the physical controllers' sticks, triggers,
grips and buttons do nothing. The line is for scripted test runs only.

WinlatorXR loads its OpenXR runtime from `C:\oxrwxr\openxr_wxr.dll` inside the
container; the project's own launcher copied this build there before each start.

## 4. Build the mods from your own game files [not verified on a clean device]

The mods change a few values in stock game files. You make them from **your
own** copy of the game; this repository has no game files or built mods.

**QuestLowLOD3** checks 23 level-of-detail values in
`GCENVIRONMENTGLOBALS.GLOBAL` and changes 20 of them (the other 3 keep their
stock value). The recipe is `tools/nms-globals/recipes/questlowlod3.json`.

1. Follow `tools/nms-globals/README.md` to extract
   `GCENVIRONMENTGLOBALS.GLOBAL.MBIN` with HGPAKtool and convert it to EXML
   with MBINCompiler.
2. Apply the recipe:
   `python3 tools/nms-globals/merge_overrides.py "$shipped" tools/nms-globals/recipes/questlowlod3.json "$output"`
3. Put the result on the headset as
   `GAMEDATA/MODS/QuestLowLOD3/GLOBALS/GCENVIRONMENTGLOBALS.GLOBAL.EXML` inside
   the game folder.

The recipe checks every stock value before it changes it, so it refuses a
game build whose values differ.

**QuestHalfTex** (optional; saves about 0.25 GB of GPU memory) drops the top
mip level of 2,692 textures. It does not re-encode anything: mip 1 becomes
mip 0 and the DDS header is fixed up. Skip it for a first run.

1. Install HGPAKtool **1.1.3** (https://github.com/monkeyman192/HGPAKtool, PyPI
   `hgpaktool`; the build uses its Python API) in a virtual environment.
2. Run it with that environment's Python, pointing at your game's
   `GAMEDATA/PCBANKS` folder:
   `python tools/nms-halftex/build_questhalftex.py "<your NMS folder>/GAMEDATA/PCBANKS" "$PWD/build/QuestHalfTex"`
   It reads `tools/nms-halftex/questhalftex-files.tsv` (paths only) and writes
   `QuestHalfTex/TEXTURES/...`. `python3 tools/nms-halftex/halve_dds.py --selftest`
   checks the mip logic without game files.
3. Put the folder on the headset as `GAMEDATA/MODS/QuestHalfTex/` inside the
   game folder.

On 2026-11-11 the first 4 listed files were built from a local build 183877
copy and matched the project's reference build byte for byte. The full set and
its in-game effect are not verified on a clean device.

## 5. Runtime config [not verified on a clean device]

The runtime reads `Winlator/oxrwxr/conf.txt` (under `/sdcard/Download/`). Keep
the stock lines WinlatorXR writes and add:

```ini
eye_size=480x540
tracking_source=merge
text_priority_right_poses=true
```

Do not add `text_priority_input=true` (see step 3).
`eye_size=480x540` is about 5 % faster than 640x720 and uses less memory.

## What is not included

- **The Vulkan memory shim.** On the project's headset a Vulkan layer DLL moves
  large game allocations into a file-backed pool (saving about 1 GB of RAM;
  without it the game runs out of memory on planets) and logs frame and memory
  data. It is not published in this repository.
- **The Steam launch step.** The project's launcher and its helpers are not
  published here, and this guide does not describe a replacement.
- **Container settings** that exist only on the project's headset.
- **Measurement and test tooling** that depends on the two items above.

## Known issues (from the project's own headset)

- **Early exit about 5 s after start** (exit 0 during `vkCreateInstance`, or a
  segfault). Intermittent.
- **GPU profiler must be off** (see step 1). With it on, Wine's Vulkan loader
  asserts at `loader.c:526` and the game exits with code 3.
- **"Allow USB debugging?" dialog** in the headset blocks a Winlator launch.
  Answer it yourself first.
- **Planet freeze** (main thread spinning, no crash) hit 2 of 6 long runs
  without `BOX64_DYNAREC_STRONGMEM=1`; 0 of 8 with it.
- **Performance**: about 30 to 33 fps on a planet with QuestLowLOD3 and eye size
  480x540 (with the full stack, including the shim); the headset shows 72 Hz
  with repeated frames. Walking costs about 3 to 5 %.
- **A stuck controller trigger** (the host reports the right trigger as pressed)
  can block the menu. `text_priority_input=true` lets scripted input win over
  the host, but turns the physical controllers off while it runs, so it is for
  scripted test runs only.

Do not post raw game stdout logs anywhere: the game writes network request
headers into them, including keys tied to your account.

## How to help

- Try the builds and open an issue with what failed and your `NMS.exe` hash.
- Run `fetch-inputs.sh` against the real GitHub URLs and report whether the
  hashes match.
- Open problems: the early-exit launches, memory use on planets without a
  custom allocator, and the draw-call cost (about 7 microseconds per draw; NMS
  issues about 1,900 draws per frame with QuestLowLOD3).
