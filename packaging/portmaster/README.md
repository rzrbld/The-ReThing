# The ReThing

An experimental ARM64 port of The Thing (2002). Thanks to Computer Artworks
for the original game, pcrecomp and the Desert Storm compatibility-layer work.
The prebuilt executable is provided for review. Supply your own game files.

## Install

Extract **the_rething.zip into your device's existing ports directory**.
The archive contains `The ReThing.sh`, `the_rething/`, `port.json`,
`README.md`, `gameinfo.xml` and `screenshot.png`. Metadata copies also remain
inside the payload for installed-port discovery. There is only one game shortcut.
Do not extract this package at the SD-card root. If upgrading the older package, first rename `ports/the-thing` to
`ports/the_rething`, then extract this ZIP into `ports` and replace packaged
files. This preserves bin/, data/, config/ and userdata/. If both folders
already exist, back them up and move the old game data, config and userdata
into the new folder before launching; do not overwrite newer saves. No saves or user settings are included in the ZIP.

Delete the obsolete `The Thing Measure.sh` shortcut. The main shortcut remains
`The ReThing.sh`.

For muOS, if its Ports menu uses `Roms/PORTS`, copy `The ReThing.sh`
there; keep the `the_rething` payload in the card's `ports` folder.
The shortcuts support both locations. Do not copy the payload into Roms/PORTS.

Launch **The ReThing**. Existing supported patched files are verified and reused
without an ISO. For a fresh installation, supply `the_rething/source/the_thing.iso`.
Supported SHA256: `1f3ac2916d8185f0e76b811fe24bc15af9412bf4da18f4bf4579ee483683fe61`.
The native launcher extracts, applies 1.1 then 1.2 and verifies the supported
executable. Allow 1.5 GiB free after copying the ISO (2 GiB recommended).
No Python or network connection is required. B cancels setup; A retries errors.

## Requirements and settings

This build targets **AArch64 H700 firmware with proprietary Mali GLES**.
Requires glibc 2.36+, Bash, firmware SDL2, SDL2_ttf, and `libmali.so.0`.
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
The guarded material-state optimization saved approximately 0.55-1.11 ms/frame
in two stationary save-slot-4 comparisons; this is not a whole-game guarantee.
Optional performance probes are disabled by default. Normal error logs stay in the port.

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
