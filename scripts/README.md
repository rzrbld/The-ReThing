# Production tooling

This directory contains the supported build/package path and focused validation.
Experiments, benchmark launchers and one-off analysis scripts are not public tooling.

- **Inputs and SDK:** `prepare.sh`, `prepare_v12.py`, `prepare_sdk.sh`.
- **Recompilation:** `translate_game.py`, `build_game.sh`, `build_game.py`, the
  `patch_*`, `stage_*`, `recover_targets.py` and `generate_modules.py` helpers.
- **Selected production optimizations:** `build_optimized_game.py`,
  `stage_queue_bounds.py`, `strip_optimized_game.sh`. These retain their existing
  generated-input prerequisites; see BUILDING.md for current reproducibility limits.
- **Launcher/audio/tools:** `build_launcher.sh`, `build_vorbis_arm_codec.sh`,
  `generate_installer_manifest.py`, `prepare_installer_tools.py`.
- **Distribution:** `package_portmaster.py`.
- **Validation:** `check_*`, `prepare_qemu_arm64.sh`, `run_qemu_aarch64.sh`.

The files in `packaging/` are the main launcher, its internal run-game script,
and the environment/CPU/frontend helpers actually shipped by the production ZIP.
