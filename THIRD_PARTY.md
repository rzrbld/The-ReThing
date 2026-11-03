# Third-party notices

The project's MIT license applies to original contributions, not to every file
or dependency. Preserve the notices shipped with each component.

| Component | Use | License / notice |
|---|---|---|
| [pcrecomp](https://github.com/sp00nznet/pcrecomp) | PE analysis, x86 lifting and Win32 runtime | MIT; [LICENSE](third_party/pcrecomp/LICENSE), [NOTICE](third_party/pcrecomp/NOTICE) and its LICENSES directory |
| ReConflict / Desert Storm compatibility work | SDL controller bridge and adapted runtime/build techniques | MIT; [ReConflict license](third_party/reconflict/LICENSE), [retained controller-source notice](probe/LICENSE.desert-pad-source); original notices remain in adapted files |
| [pl_mpeg](https://github.com/phoboslab/pl_mpeg) | MPEG video/audio decoding | MIT; [vendored header](third_party/pl_mpeg/pl_mpeg.h) and [local changes](third_party/pl_mpeg/README.md) |
| [SDL2](https://www.libsdl.org/) and SDL2_ttf | Window, input, audio and text support | zlib; provided by device firmware, not relicensed by this project |
| [libogg / libvorbis](https://xiph.org/) | Native Ogg/Vorbis audio decoding | BSD-style Xiph.Org licenses; release includes LICENSE.ogg and LICENSE.vorbis |
| [7-Zip / p7zip](https://www.7-zip.org/) | ISO and archive extraction | Component-specific LGPL and other terms; see bundled p7zip copyright/license files, including any applicable unRAR restriction |
| [unshield](https://github.com/twogood/unshield) | InstallShield cabinet extraction | MIT; bundled package copyright notice |
| [xdelta 1](https://github.com/jmacd/xdelta) | Apply user-supplied patch deltas | GPL-2.0-or-later; bundled package copyright notice |
| GLib, PCRE2, zlib | Private extraction-tool dependencies | Respectively LGPL, BSD-style and zlib terms; bundled package notices |
| GNU libc and GCC runtime libraries | Private extraction-tool support and compiled runtime support | LGPL/GPL with applicable GCC Runtime Library Exception; bundled notices and common license texts |
| [pefile](https://github.com/erocarrera/pefile), [Capstone](https://www.capstone-engine.org/) | Workstation analysis/build dependencies | MIT and BSD-style respectively; installed separately |
| PortMaster | Packaging conventions and optional firmware helpers | PortMaster is a separate project; this package does not imply catalogue inclusion or endorsement |

Release archives retain component licenses under `the_rething/licenses/` and
`the_rething/tools/licenses/`. Installer package versions, download URLs and
hashes are recorded in `config/installer-packages.lock.json` and in the release's
tool manifest. Private tool libraries are not installed into the operating system
or used to replace its SDL/GPU libraries.

The original game is **The Thing (2002)**, developed by Computer Artworks.
Its code and content are not MIT-licensed by this project. Original-game names
and images identify the game being ported; their rights remain with their
respective holders. Gameplay screenshots are not grants of rights to game art.
