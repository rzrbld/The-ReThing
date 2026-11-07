# The ReThing

An ARM64 port of The Thing (2002). Thanks to Computer Artworks
for the original game, pcrecomp and the Desert Storm compatibility-layer work.
The prebuilt executable is provided for review. Supply your own game files.

## Install

**Fresh install**

1. Copy `The ReThing.sh` and the `the_rething` folder from the ZIP into the
   **ports** folder on your SD card, so you have `ports/The ReThing.sh` and
   `ports/the_rething/`. The other files in the ZIP (`port.json`, `README.md`,
   `gameinfo.xml`, `screenshot.png`) are PortMaster metadata; leave them out of
   `ports`. Do not use the SD-card root or an extra sub-folder.
2. Copy your game ISO into `ports/the_rething/source/` and name it exactly
   `the_thing.iso`. Supported SHA-256:
   `1f3ac2916d8185f0e76b811fe24bc15af9412bf4da18f4bf4579ee483683fe61`.
   Allow 1.5 GiB free after copying it (2 GiB recommended).
3. Start **The ReThing** from the Ports menu. On muOS, if it does not appear,
   also copy `The ReThing.sh` into `Roms/PORTS` (keep `the_rething` in `ports`).
4. The first launch unpacks the ISO, applies the 1.1 and 1.2 updates and checks
   the result. This takes a while; keep the device charged. **A** retries an
   error, **B** cancels. No network or Python is needed.
5. The settings menu opens with the recommended defaults; choose
   **SAVE & LAUNCH**.

Once installed, the game runs without the ISO; you may delete it. The first
time you visit an area the game may pause briefly while it prepares graphics;
that happens only once.

**Upgrading**

Copy `The ReThing.sh` and `the_rething` from the new ZIP into the same **ports**
folder and replace existing files.
Game files (`bin/`, `data/`), settings (`config/`) and saves (`userdata/`) are
kept; the ZIP contains none of them. From the old `ports/the-thing` folder,
rename it to `ports/the_rething` first; if both folders exist, back up and move
`bin/`, `data/`, `config/` and `userdata/` into `the_rething` before launching.
The obsolete `The Thing Measure.sh` shortcut can be deleted. If an earlier ZIP
was extracted straight into `ports`, also delete `ports/port.json`,
`ports/README.md`, `ports/gameinfo.xml` and `ports/screenshot.png` when they
belong to The ReThing.

## Requirements and settings

This build targets **AArch64 H700 firmware**; it is tested with the Mali-G31 GLES driver.
Requires glibc 2.36+, Bash, firmware SDL2, SDL2_ttf and an OpenGL ES 3.x driver that
SDL2 can load. The game links no GPU library; other drivers may start but are untested.
It is not a universal Panfrost/Adreno/ARM32 build. No firmware GPU/SDL library is
installed or overridden. Bundled extraction tools use their own private loader
and libraries; that library path is never exported to the game or launcher.
PortMaster integration uses control.txt, firmware mods, get_controls,
pm_platform_helper and pm_finish. It does not require a downloaded runtime.

Recommended: 640x480, SINGLE, BUFFER CACHE OFF, SHADOWS SIMPLE.
Existing preferences are preserved; change them in the launcher if needed.
Open RENDER OPTIONS for renderer, resolution, shadows, buffer cache and the
additional render switches. Unavailable combinations show BLOCKED and preserve
your preferences. See RENDER_OPTIONS.md for each switch. SINGLE blocks ASYNC PRESENT; THREADED blocks RANGED DRAWS
and DEPTH INVALIDATE. 512x384 renders below screen resolution and scales to fit.

H700 gameplay has been tested, but demanding scenes still fall below 30 FPS.
Compiled shaders are kept in `userdata/shader-cache` (safe to delete; rebuilt
as you play). Optional performance probes are disabled by default. Normal
error logs stay in the port.

## Controls

| Control | Action |
|---|---|
| Left stick | Move |
| Right stick | Camera / menu pointer |
| A | Operate / confirm |
| B | Crouch |
| X | Reload |
| Y | Inventory |
| L / R | Squad manager / documents |
| Triggers | Primary / secondary action |
| Start | Pause / back |
| D-pad | Menu navigation / equipment |

See CONTROLS.md for mappings and configuration. Saves remain in
`the_rething/userdata/bin/`; settings in `the_rething/config/`.
For setup failures, inspect `the_rething/tmp/install.log` and install-tools.log.
For game failures preserve `the_rething/log.txt` before launching again.

## Build

See OWN_COPY_BUILD.txt for game recompilation with user-owned files.
Package reproduction in the development repository:

    bash scripts/build_launcher.sh aarch64
    python3 scripts/package_portmaster.py --game build/game/thething.aarch64
    python3 scripts/check_portmaster_package.py

Python here is a workstation build dependency only.
