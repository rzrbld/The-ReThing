# Building The ReThing

These are workstation instructions. The handheld only runs the resulting
executable and native installer; it does not need Python.

## Prerequisites

Use an x86-64 Linux development machine with Python 3, a C/C++ compiler,
Docker, binutils, pkg-config, 7z, dpkg-deb and the SDL2/GLES development headers.
Allow substantial disk space and RAM for generated C and compilation. Use a
small parallel job count initially.

You must supply the supported original game ISO listed in README.md. No game
executable, resources, generated translation or proprietary SDK is checked in.

## ARM toolchain and SDK

The required cross-toolchain setup is included in this repository, adapted from
Desert Storm. No Desert Storm checkout or `DESERT_STORM_SDK` variable is needed.

```sh
bash scripts/prepare_sdk.sh aarch64
```

This builds `rething-cross:bookworm` from `packaging/Dockerfile.cross` and copies
AArch64 SDL2/SDL2_ttf headers and SDL2/SDL2_ttf/GLES linker libraries into
`build/sdk-include/` and `build/target-libs/`. It downloads public Debian packages;
Docker and network access are required for setup. The base image is digest-pinned;
APT package updates mean this is not a bit-for-bit toolchain lock.
Subsequent compilation uses the local image with networking disabled.
`CROSS_IMAGE` can override the image name consistently across the build scripts.

Existing target link libraries are preserved by setup. For an H700 firmware
build, matching firmware libraries may be placed in `build/target-libs/` before
setup. In particular, the review release linked against the Mali driver through
`libGLESv2.so` with SONAME `libmali.so.0`. The public Debian SDK instead links to
`libGLESv2.so.2`; the target firmware must provide that library and its dependencies.
Inspect `readelf -d` before deployment. GPU drivers are not shipped in source.

Host preparation still uses the pinned Debian/Ubuntu package versions in
`scripts/prepare.sh` and `scripts/prepare_sdk.sh host`; those versions must be
available from the workstation's configured repositories.

## Prepare your game and Python dependencies

From the repository root:

```sh
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements-audit.txt
mkdir -p The_NEW_ISO_UNPROTECTED
# Copy your supported ISO to The_NEW_ISO_UNPROTECTED/the_thing.iso
bash scripts/prepare.sh
bash scripts/prepare_sdk.sh host
bash scripts/prepare_sdk.sh aarch64
```

Preparation checks the ISO, installs its original files, applies 1.1 then 1.2,
and selects/verifies the supported executable supplied with those inputs.
It creates `build/active/bin` and `build/active/data`. Original input is retained.
Generated manifests and analysis outputs go into ignored `docs/evidence/`.

## Translate and compile

```sh
python3 -u scripts/translate_game.py --all-modules
bash scripts/build_game.sh host --allow-traps --local-regs \
  --translated-opt=-O2 --render-thread --jobs 4
bash scripts/build_game.sh aarch64 --allow-traps --local-regs \
  --translated-opt=-O2 --render-thread --jobs 4
```

Outputs are `build/game/thething.host` and `build/game/thething.aarch64`.
`--allow-traps` permits compilation of unsupported paths; it does not make them
implemented. Reaching gameplay still requires functional testing.
The render-worker option compiles that capability; the production launcher
selects SINGLE by default.

These commands build the base port. The current review release also includes
the selected local-x87-top/native-audio/early-queue changes assembled by
`scripts/build_optimized_game.py`. That historical builder depends
on locally generated selection/patch data and retained objects. Exact recreation
of that optimized executable from a fresh checkout is not yet a one-command
workflow. The release executable should not be described as a clean rebuild of
the base commands above. Generated original-game code stays out of Git.

Verify the resulting ELF and imports:

```sh
readelf -h build/game/thething.aarch64
readelf -d build/game/thething.aarch64
THING_ROOT="$PWD/build/active" build/game/thething.host --check-imports
```

## Native launcher and installation tools

```sh
bash scripts/build_launcher.sh host
bash scripts/build_launcher.sh aarch64
python3 scripts/prepare_installer_tools.py
```

The launcher embeds setup logic and file identities in
`launcher/native_setup.cpp` and `launcher/install_manifest.h`. To regenerate
identities after preparing your files:

```sh
python3 scripts/generate_installer_manifest.py
```

The tool staging script downloads checksum-locked ARM packages and retains their
licenses. It does not download game files. Package URLs, versions and hashes are
in `config/installer-packages.lock.json`.

Native audio is built by `scripts/build_vorbis_arm_codec.sh`, run inside the
cross container with this repository mounted at `/src`. It expects unpacked
libogg 1.3.6 and libvorbis 1.3.7 sources under `build/vorbis-arm-sources/`, and
produces `build/native-vorbis-arm/probe.aarch64.so` plus the upstream licenses.
Those source archives are separate Xiph.Org dependencies, not game files.

## Create the production package

Once the game, launcher, audio library and installer tools are built:

```sh
python3 scripts/package_portmaster.py --game build/game/thething.aarch64
```

This directly assembles `dist/the_rething.zip` and its SHA-256 file from the
specified executable and local component builds. It does not depend on an older
release ZIP. Supply a stripped game executable through `--game` to reduce size.
The ZIP includes the PortMaster metadata, native launcher, licenses and no
original game input files, saves, user configuration or diagnostic shortcuts.

Local validation scripts include `check_launcher_settings.py`,
`check_portmaster_launch.py` and `check_native_setup.py`. The setup tests use
user-supplied game resources and the prepared QEMU/tool environment; `--arm`
runs the ARM launcher and `--full` exercises ISO installation. These are functional
checks, not hardware performance measurements.

Test gameplay, saving/loading, audio and controller input on the actual target
before claiming hardware compatibility or a frame-rate target.
