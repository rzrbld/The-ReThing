# The ReThing

The ReThing is a community ARM64 Linux port of **The Thing (2002)** using static
binary recompilation, OpenGL ES graphics and SDL2 controller input.
It targets H700 handhelds with proprietary Mali graphics drivers.

The game is running, but performance is still being improved. Demanding gameplay
can fall below 30 FPS. This is not a port of the 2024 remaster.

## Quick start

1. Download `the_rething.zip` from [Releases](https://github.com/rzrbld/The-ReThing/releases).
2. Extract it into your device's **ports** directory.
3. Supply your own supported game ISO as `the_rething/source/the_thing.iso`.
4. Launch **The ReThing**. The native launcher installs and verifies the game,
   then opens its settings menu. No Python is needed on the handheld.

Existing supported `bin/` and `data/` files are reused without an ISO.
Upgrading an older package? Rename `ports/the-thing` to `ports/the_rething`
before extracting, and preserve `config/` and `userdata/` for settings and saves.
If both folders already exist, back up and merge your user files carefully.

On muOS, copy `The ReThing.sh` to `Roms/PORTS` if that is where your menu looks
for shortcuts. Keep the game folder in `ports/the_rething`.

Recommended settings: **640×480, SINGLE, buffer cache OFF, shadows SIMPLE**.
Requires AArch64 firmware, glibc 2.36+, Bash, SDL2, SDL2_ttf and `libmali.so.0`.
Other GPU drivers and 32-bit firmware are not supported by this binary.

Only this ISO revision is currently supported (SHA-256):

```text
1f3ac2916d8185f0e76b811fe24bc15af9412bf4da18f4bf4579ee483683fe61
```

It contains the original game and the 1.1/1.2 update inputs expected by the
installer. Other revisions are rejected. Allow at least 1.5 GiB free after
copying the ISO; 2 GiB is recommended.

## Controls and saves

Left stick moves; right stick controls the camera. A operates/confirms, B
crouches, X reloads, Y opens inventory, and Start pauses/goes back.
See [controller mappings](docs/CONTROLS.md) for details.

Saves: `the_rething/userdata/bin/`. Settings: `the_rething/config/`.
For problems, retain `the_rething/log.txt` or `the_rething/tmp/install.log`.

## Build

The cross-toolchain setup and SDK preparation scripts are included; no Desert
Storm checkout is required. Start with `bash scripts/prepare_sdk.sh aarch64`.
See [BUILDING.md](BUILDING.md) for prerequisites and commands. Recompilation
requires your own original game files and a development PC; the handheld
installer prepares resources, it does not compile the executable.
Optimization patch data is generated from your own matching executable and
translated files; the required hashes and commands are in BUILDING.md.

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
