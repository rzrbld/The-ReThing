# Production tooling

This directory contains the supported build/package path and focused validation.
Experiments, benchmark launchers and one-off analysis scripts are not public tooling.

- **Full build:** `build_distribution.sh` (fresh checkout plus supported ISO).
- **Inputs and SDK:** `prepare.sh`, `prepare_v12.py`, `prepare_sdk.sh`, `prepare_cross.sh`, `prepare_mali_link.sh`.
- **Recompilation:** `translate_game.py`, `build_game.sh`, `build_game.py`, the
  `patch_*`, `stage_*`, `recover_targets.py` and `generate_modules.py` helpers.
- **Selected production optimizations:** `build_optimized_game.py`,
  `generate_optimization_data.py`, `stage_queue_bounds.py`, `stage_material_state_reuse.py`,
  `strip_optimized_game.sh`. Generate patch data from your own translated files
  after a base build; see BUILDING.md for exact input hashes and command order.
- **Launcher/audio/tools:** `build_launcher.sh`, `build_vorbis_arm_codec.sh`,
  `generate_installer_manifest.py`, `prepare_installer_tools.py`, `fetch_audio_sources.py`.
- **Distribution:** `package_portmaster.py`.
- **Validation:** `check_*`, `prepare_qemu_arm64.sh`, `run_qemu_aarch64.sh`.

The files in `packaging/` are the main launcher, its internal run-game script,
and the environment/CPU/frontend helpers actually shipped by the production ZIP.

`prepare_cross.sh` builds the included public cross-compiler image and stages
SDK headers and link libraries. `prepare_sdk.sh aarch64` invokes it. Existing
firmware link libraries are preserved.
