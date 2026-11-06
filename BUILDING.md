# Building The ReThing

Build on an x86-64 Linux workstation using your own supported original game ISO.
The handheld runs the compiled game and native installer; it needs no Python.
No game files, generated game code, developer SDK, previous executable or private
profiling capture is needed from another checkout.

## Requirements

Install Docker, Python 3.10+ with venv/pip support, a C/C++ compiler, pkg-config,
SDL2 development headers, binutils and dpkg-deb. Docker must work for your account.
For example, on Debian/Ubuntu the host packages are:

```sh
sudo apt-get install python3-venv python3-pip build-essential pkg-config libsdl2-dev binutils dpkg-dev
```

Install Docker separately. Network access is required to download public Python
packages, the cross-toolchain, Xiph audio sources and checksum-locked installer
tools. Allow at least 15 GiB of free disk space. Start with `JOBS=4`; increasing
parallelism also increases memory use.

Only this original PC release input is supported:

| Input | SHA-256 |
| --- | --- |
| ISO containing base game, 1.1 and 1.2 updates and supplied unprotected executable | `1f3ac2916d8185f0e76b811fe24bc15af9412bf4da18f4bf4579ee483683fe61` |
| Selected `bin/thething.exe` after preparation | `085d1a90b2bd57967e8e911eb1f7c2232fd4b045b7f363c5c4a3056f720abb49` |

A version label alone is insufficient. Inputs are checked; other releases require
separately reviewed translation and patch support. Do not replace expected hashes
to bypass a mismatch. The input ISO is mounted read-only and remains unchanged.

## Full build

Use a fresh checkout:

```sh
git clone https://github.com/rzrbld/The-ReThing.git
cd The-ReThing
JOBS=4 bash scripts/build_distribution.sh \
  '/absolute/path/to/the_thing.iso' "$PWD/dist/the_rething.zip"
```

The command prepares the public SDK, extracts your ISO, applies patch 1.1 then
1.2, selects the verified executable, translates the main EXE and all level DLLs,
compiles the base game, generates and applies the selected production optimizations,
builds the release executable with fixed guest registers (see *Register models*),
and builds the ARM launcher and native audio library. It downloads the installer
tools, packages the port and runs launcher/configuration/package checks.

The script rejects existing translations or game build objects and an existing
output ZIP. Use a fresh directory for a new full verification; see individual
stages below to diagnose a failed build. `JOBS` defaults to 4.

Outputs include:

- Your requested ZIP and adjacent `.sha256` file.
- `build/active/`: verified installation prepared from your ISO.
- `docs/evidence/patched-v12.json`: hashes of the prepared game files.
- `build/translated/`: regenerated game code; keep it private.
- `build/game/thething.aarch64`: baseline executable and per-unit compiler logs.
- `build/game/c1-fixed/thething.aarch64`: optimized release executable with symbols.
- `build/c1-fixed-combined-aarch64-20261106/thething-c1-fixed.aarch64`:
  stripped optimized executable used by the package.
- `build/launcher/thething-launcher.aarch64` and
  `build/native-vorbis-arm/probe.aarch64.so`.

The dates in the optimized directory names are retained for script compatibility.
The package excludes original game inputs, generated code, saves and user settings.

## Public toolchain and target firmware

`packaging/Dockerfile.cross` builds `rething-cross:bookworm`. The base image is
pinned by digest; Debian package updates can change final binary hashes.
`CROSS_IMAGE` overrides the image name consistently. The build does not need
Desert Storm, `DESERT_STORM_SDK`, or an unpublished compiler image.

SDK headers and link libraries are generated in `build/sdk-include/` and
`build/target-libs/`. The game executable links **no GPU library**:
`runtime/gles_dispatch.c` defines every GLES entry point the port uses and
resolves it through `SDL_GL_GetProcAddress` in the current SDL context, so the
driver is whatever the firmware's SDL2 loads. A GL function used by the port
but missing from `probe/rt/*.def` is a link error. The link-only GLES stub that
`prepare_cross.sh` still generates in `build/mali-link/` is not linked into the
game and is never packaged. The handheld must supply SDL2 with an OpenGL ES 3.x
driver, SDL2_ttf, and compatible AArch64 glibc (2.36+). Existing target
libraries are preserved for explicit firmware SDK builds; the clean build uses
only public Debian libraries.

## Individual stages

Run these from the checkout root. Use the same Python environment and source
revision throughout; generated-function hashes deliberately reject drift.

```sh
python3 -m venv .venv
.venv/bin/pip install -r requirements-audit.txt
export PATH="$PWD/.venv/bin:$PATH"
export CROSS_IMAGE=rething-cross:bookworm
export JOBS=4
bash scripts/prepare_cross.sh
mkdir -p build docs/evidence
ISO='/absolute/path/to/the_thing.iso'
docker run --rm --network none --read-only --tmpfs /tmp \
  --user "$(id -u):$(id -g)" \
  -v "$PWD:/src:ro" -v "$PWD/build:/src/build:rw" \
  -v "$PWD/docs/evidence:/src/docs/evidence:rw" -v "$ISO:/input/game.iso:ro" \
  -e THING_ISO=/input/game.iso -e THING_SYSTEM_TOOLS=1 --workdir /src \
  "$CROSS_IMAGE" python3 scripts/prepare_v12.py
python3 -u scripts/translate_game.py --all-modules
bash scripts/build_game.sh aarch64 --allow-traps --local-regs \
  --translated-opt=-O2 --render-thread --jobs "$JOBS"
python3 scripts/generate_optimization_data.py
python3 scripts/build_optimized_game.py --target=aarch64 --fixed-regs
bash scripts/strip_optimized_game.sh --fixed-regs
bash scripts/build_launcher.sh aarch64
bash scripts/build_launcher.sh host
python3 scripts/fetch_audio_sources.py
docker run --rm --network none --read-only --tmpfs /tmp \
  --user "$(id -u):$(id -g)" -v "$PWD:/src:ro" -v "$PWD/build:/src/build:rw" \
  --workdir /src "$CROSS_IMAGE" bash scripts/build_vorbis_arm_codec.sh
python3 scripts/prepare_installer_tools.py
python3 scripts/package_portmaster.py \
  --game build/c1-fixed-combined-aarch64-20261106/thething-c1-fixed.aarch64 \
  --output "$PWD/dist/the_rething.zip"
python3 scripts/check_portmaster_package.py --archive "$PWD/dist/the_rething.zip"
```

`--allow-traps` permits the diagnostic traps retained for unsupported translated
paths. It does not implement them. The render-worker capability is compiled,
while the production launcher defaults to SINGLE rendering and buffer cache OFF.

## Register models

Releases use **fixed guest registers** (`--fixed-regs`): the eight x86 guest
registers stay in AArch64 registers x19..x26 for the whole run instead of being
copied to and from memory around every translated call. The base build always
uses `--local-regs`; `build_optimized_game.py --fixed-regs` then compiles the
optimized composition with fixed registers into its own output root
(`build/game/c1-fixed`), leaving the base build and its object cache unchanged.
Guest `longjmp` is not supported in this mode and stops with a clear error; no
translated code uses it today.

The previous local-register optimized executable can still be built, for
comparison or troubleshooting, after the base build:

```sh
python3 scripts/build_optimized_game.py --target=aarch64
bash scripts/strip_optimized_game.sh
# -> build/early-queue-combined-aarch64-20261103/thething-queue-bounds.aarch64
```

On an H700 (1.8 GHz), a heavy level-1 view with about 410 draws per frame took
39.7 ms per frame with local registers and 35.0 ms with fixed registers.

## Runtime renderer features

These need no build options; they are part of every build:

- **Shader cache.** Compiled GPU-path shader programs are saved as driver
  binaries in `userdata/shader-cache` on the handheld and prebuilt at the next
  start, so a shader first needed in play is compiled only once.
  `THING_SHADER_CACHE=0` disables it.
- **GPU streaming ring.** Per-draw geometry is copied into one persistently
  mapped buffer split into fenced per-frame segments instead of a buffer upload
  per draw. It needs `GL_EXT_buffer_storage` and falls back automatically;
  `THING_GPU_RING=0` disables it. In the same H700 view this reduced frame time
  from 35.4 to 32.5 ms.

Both are off with the THREADED renderer, which is not recommended on H700.

## Generated optimization data

`generate_optimization_data.py` verifies your executable and reads your translated
function bodies. `config/optimization-functions.json` contains only 83 addresses
and expected before/after hashes; the transformation algorithm is public.
The generated `build/optimization/patches.json` contains your own game-derived
code and stays ignored by Git. The optimized builder invokes the generator too.

The base build for the **same target** must finish first: the optimized builder
backs up those locally built objects and executable, applies local-x87-top,
native Vorbis, queue-bounds and guarded material-state reuse changes, compiles,
and restores the base files.
No earlier release objects or profiling logs are used.

## Validation and limits

Inspect the actual optimized ELF, not just the baseline:

```sh
readelf -h build/c1-fixed-combined-aarch64-20261106/thething-c1-fixed.aarch64
readelf -d build/c1-fixed-combined-aarch64-20261106/thething-c1-fixed.aarch64
python3 scripts/check_launcher_settings.py
python3 scripts/check_portmaster_launch.py
```

Optional `check_native_setup.py --arm --full` tests installation with your ISO
and freshly built ARM launcher after `prepare_qemu_arm64.sh` prepares the QEMU
runtime. QEMU/Mesa testing is not a measurement of Mali/H700 performance.
Test gameplay, rendering, saving/loading, audio and controls on the actual device
before claiming hardware compatibility or a frame-rate target.

## Verification record (2026-11-07)

A fresh clone of this repository built the full distribution from the supported
ISO with `JOBS=16 bash scripts/build_distribution.sh`, without existing
translations, objects or executables, in 11 min 49 s on a 24-thread x86-64
workstation (Docker image and public downloads cached). It regenerated all 578
translated C units across 22 modules, compiled 653 base units, built the
fixed-register optimized executable, the ARM launcher and audio library, and
passed the launcher, launch-lifecycle and package checks. The stripped
fixed-register executable was 73,021,576 bytes with SHA-256:

```text
7505bd4f37bf53db90719d159b88d756d65da3ba8b682614ac20cccb76ab678a
```

It links to `libmali.so.0`, SDL2, SDL2_ttf, libm and libc. On an H700 it loaded
level 1 without faults with the shader cache and GPU streaming ring active.
This executable is the one in the GPU Streaming release package. Different
public package or compiler versions may produce a different hash; it is not a
build input requirement.

## Verification record (2026-11-03)

This record covers the earlier package, which used the local-register optimized
executable; the full build now packages the fixed-register executable.

Two isolated source directories regenerated the game from the supported ISO,
without existing translations, objects, SDK directories, audio sources, installer
tool bundles or earlier executables. One run used individual stages; the other
completed the exact `build_distribution.sh` command above through a validated ZIP.
Public dependency downloads and Docker's public image cache were permitted.

Both runs produced identical 29-file prepared payloads, all 578 translated C
units across 22 modules (56,279 functions), and byte-identical baseline and
optimized ARM executables. Each baseline compiled 653 C units. The optimized,
stripped executable was 100,284,536 bytes with SHA-256:

```text
79b4cb56cc03b528b2af5d2b0fc6ca32f50d96cbf2b6ceb974de0f288c82fa39
```

The ARM ELF links to `libmali.so.0`, SDL2 and SDL2_ttf. Package checks verified the
layout, metadata, hashes, ARM architecture, no original game files/GPU stub, and
launcher behavior from both ports and muOS shortcut directories. Launcher settings,
controller/runtime and GLES/movie host tests passed. The newly built ARM installer
passed full ISO installation, adoption, rollback, cancellation and input rejection
tests under QEMU. The optimized executable resolved all 133 imports and rendered
120 startup frames under QEMU/Mesa before its configured normal exit.

These results verify the build and startup/install paths. They do not establish
new H700 gameplay compatibility, sustained performance or 30 FPS. No H700 was used
for this verification. Different public package/compiler versions may produce a
different executable hash; this recorded hash is not a build input requirement.

The material-state reuse check requires Unicorn in addition to the normal build
dependencies. With your supported original executable prepared, run
`python3 scripts/check_material_state_reuse.py`. It compares the guard against
the original x86 instructions; no original code is included as a test fixture.
