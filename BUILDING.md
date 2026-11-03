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

The existing ARM build expects:

- An AArch64 GCC/G++ cross-compiler container with Python 3. Set `CROSS_IMAGE`
  to its Docker image name. The retained reference image is local, identified
  by `sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49`;
  this is not a publicly downloadable image name.
- Target SDK headers in `build/sdk-include/` and linker libraries in
  `build/target-libs/`: SDL2, SDL2_ttf, GLES and their required target headers.
  Use libraries compatible with your firmware. Do not substitute x86 libraries.
  `scripts/prepare_sdk.sh aarch64` can copy an existing compatible SDK by setting
  `DESERT_STORM_SDK` to its build directory. It does not download that SDK.
- The pinned host package versions used by `scripts/prepare.sh` and
  `scripts/prepare_sdk.sh host`. Those scripts use Debian/Ubuntu package tools;
  their pinned versions must be available from your configured repositories.

A completely self-contained, clean-machine SDK bootstrap is not yet provided.
Do not interpret the commands below as a promise of an exact release rebuild
without these prerequisites.

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
DESERT_STORM_SDK=/absolute/path/to/compatible/sdk/build \
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
CROSS_IMAGE=your-aarch64-compiler-image \
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
`scripts/build_early_queue_combined_20261103.py`. That historical builder depends
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
CROSS_IMAGE=your-aarch64-compiler-image bash scripts/build_launcher.sh aarch64
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
