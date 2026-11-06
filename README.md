# The ReThing

The ReThing is a community ARM64 Linux port of **The Thing (2002)** using static
binary recompilation, OpenGL ES graphics and SDL2 controller input.
It targets H700 handhelds with proprietary Mali graphics drivers.

The game is running, but performance is still being improved. Demanding gameplay
can fall below 30 FPS. This is not a port of the 2024 remaster.

## Quick start

**You need:** an H700 handheld with 64-bit firmware and Mali graphics drivers
(tested on muOS), your own copy of the supported game ISO (see below), and
about 2 GiB free on the SD card.

### Fresh install

1. Open [Releases](https://github.com/rzrbld/The-ReThing/releases) and download
   `the_rething.zip` from the newest release at the top of the list (releases
   are marked *Pre-release*).
2. Extract the ZIP on your PC or into a temporary folder, then copy
   **`The ReThing.sh`** and the **`the_rething`** folder into the **ports**
   folder on your SD card. Leave the other files from the ZIP (`port.json`,
   `README.md`, `gameinfo.xml`, `screenshot.png`) out of `ports`; they are
   PortMaster metadata. You should end up with:

   ```text
   ports/
   ├── The ReThing.sh
   └── the_rething/
       ├── source/
       └── ...
   ```

   Do not put them at the SD-card root or into an extra sub-folder.
3. Copy your game ISO into `ports/the_rething/source/` and name it exactly
   `the_thing.iso`.
4. Start **The ReThing** from the Ports menu. On muOS, if it does not appear,
   also copy `The ReThing.sh` into `Roms/PORTS` (keep the `the_rething` folder
   in `ports`).
5. The first launch unpacks the ISO, applies the 1.1 and 1.2 updates and checks
   the result. This takes a while; keep the device charged. If an error appears,
   press **A** to retry or **B** to cancel. No network or Python is needed.
6. The settings menu opens. The defaults are the recommended ones
   (**640×480, SINGLE, buffer cache OFF, shadows SIMPLE**); start the game.

Once installed, the game runs without the ISO, so you may delete it to free
space. The first time you visit an area the game may pause briefly while it
prepares graphics; that happens only once.

### Upgrading

Download the new `the_rething.zip` and copy its `The ReThing.sh` and
`the_rething` folder into the same **ports** folder, replacing existing files. Your game files (`bin/`, `data/`), settings
(`config/`) and saves (`userdata/`) are kept; no ISO is needed.

Extracted an earlier package straight into `ports`? It also left four PortMaster
metadata files there: `ports/port.json`, `ports/README.md`, `ports/gameinfo.xml`
and `ports/screenshot.png`. Delete them if they belong to The ReThing (its
`gameinfo.xml` names *The ReThing*); they can hide other ports' metadata.

Coming from the old `ports/the-thing` folder? Rename it to
`ports/the_rething` first. If both folders exist, back up and move your
`bin/`, `data/`, `config/` and `userdata/` into `the_rething` before launching.

### Supported ISO

Only this ISO revision works (SHA-256):

```text
1f3ac2916d8185f0e76b811fe24bc15af9412bf4da18f4bf4579ee483683fe61
```

It contains the original game and the 1.1/1.2 update inputs the installer
needs; other revisions are rejected. To check yours on a PC, run
`sha256sum the_thing.iso` (Linux/macOS: `shasum -a 256 the_thing.iso`) or
`certutil -hashfile the_thing.iso SHA256` (Windows).

### Settings and requirements

Open **RENDER OPTIONS** in the launcher for graphics controls. Options that
require a different renderer are blocked; saved preferences are preserved.
See [render options](docs/RENDER_OPTIONS.md).
Requires AArch64 firmware, glibc 2.36+, Bash, SDL2, SDL2_ttf and an OpenGL ES 3.x
driver that SDL2 can load. The game does not link a GPU library; it has been
tested on H700 with the Mali-G31 driver. Other drivers may start but are
untested, and 32-bit firmware is not supported.

## Controls and saves

Left stick moves; right stick controls the camera. A operates/confirms, B
crouches, X reloads, Y opens inventory, and Start pauses/goes back.
See [controller mappings](docs/CONTROLS.md) for details.

Saves: `the_rething/userdata/bin/`. Settings: `the_rething/config/`.
For problems, retain `the_rething/log.txt` or `the_rething/tmp/install.log`.

## Build

Use a Linux development PC and your own supported ISO:

```sh
JOBS=4 bash scripts/build_distribution.sh /path/to/the_thing.iso "$PWD/dist/the_rething.zip"
```

See [BUILDING.md](BUILDING.md) for dependencies, exact input hashes and individual
stages. The script builds the launcher, game and distribution from source; it
requires no Desert Storm checkout or previous build. Optimization data is generated
from your own game files. The handheld installer prepares resources, it does not
compile the executable.

## License and acknowledgements

Original code contributed to The ReThing is available under the [MIT license](LICENSE).
Third-party components retain their own licenses; see [THIRD_PARTY.md](THIRD_PARTY.md).
Thanks to Computer Artworks, pcrecomp, the ReConflict/Desert Storm work, SDL,
pl_mpeg and Xiph.Org.

**The Thing**, its original game code, characters, trademarks, artwork, audio
and other content belong to their respective rights holders. This project is
unofficial and is not affiliated with or endorsed by those rights holders.
The MIT license does not cover the original game or relicense third-party code.
No original ISO, Windows executable, patches or resource archives are included
in the source repository. Users must supply their own game files.

The prebuilt executable in Releases is provided **for review**. This description
does not grant rights to the original game or its translated code.
