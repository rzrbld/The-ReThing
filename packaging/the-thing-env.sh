#!/bin/bash
# Shared environment for the Ports launchers (controller DB, audio/video,
# renderer defaults and persisted launcher settings). Callers must export
# THING_ROOT and GAMEDIR first.
if [[ -z ${controlfolder:-} ]]; then
    for candidate in /PortMaster /opt/system/Tools/PortMaster /opt/tools/PortMaster \
        "${XDG_DATA_HOME:-$HOME/.local/share}/PortMaster" /mnt/mmc/MUOS/PortMaster /roms/ports/PortMaster; do
        if [[ -f "$candidate/control.txt" ]]; then controlfolder=$candidate; break; fi
    done
fi
if [[ -f ${controlfolder:-}/control.txt ]]; then
    export controlfolder
    source "$controlfolder/control.txt"
    [[ ! -f "$controlfolder/mod_${CFW_NAME}.txt" ]] || source "$controlfolder/mod_${CFW_NAME}.txt"
    if declare -F get_controls >/dev/null; then get_controls; fi
    export SDL_GAMECONTROLLERCONFIG="${sdl_controllerconfig:-${SDL_GAMECONTROLLERCONFIG:-}}"
fi
if [[ -f /usr/lib/gamecontrollerdb.txt ]]; then
    export THING_CONTROLLER_DB=${THING_CONTROLLER_DB:-/usr/lib/gamecontrollerdb.txt}
    # The H700 firmware's generic same-GUID mapping can omit its right stick.
    if grep -q 'Name="Deeplay-keys"' /proc/bus/input/devices; then
        mapping=$(grep -m1 ',Deeplay-keys,' /usr/lib/gamecontrollerdb.txt)
        [[ -z "$mapping" ]] || export SDL_GAMECONTROLLERCONFIG="$mapping"
    fi
fi
if [[ -S /run/pipewire-0 ]]; then export XDG_RUNTIME_DIR=/run; fi
export SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-alsa}
export THING_PAD_CONFIG=${THING_PAD_CONFIG:-$gamedir/controller-buttons.cfg}
# Overlay off by default: it costs several FPS on the H700 and is a diagnostic
# only. "The Thing Overlay.sh" turns it back on.
# Lightweight perf counters (draws/lit/index scan) for tuning; negligible cost.
export THING_PERF_PROBE=${THING_PERF_PROBE:-1}
# Renderer defaults after the 2026-10-31 device A/B: GPU vertex path + cached
# and batched GL state + fragment-shader specialisation. The streaming vertex
# ring stays off (Mali serialisation hazard). "The Thing Safe.sh" restores the
# conservative CPU-reference renderer if a device ever misbehaves, and
# THING_FAST_SHADER=0 / DS_GPU_VERTEX=0 are the A/B switches.
export DS_GPU_VERTEX=${DS_GPU_VERTEX:-1}
export THING_BIND_CACHE=${THING_BIND_CACHE:-1}
export THING_DRAW_ERROR_CHECK_EVERY=${THING_DRAW_ERROR_CHECK_EVERY:-16}
export THING_RING_VB=${THING_RING_VB:-0}
# SINGLE remains the default; explicit settings can select the render worker.
if [[ ${CFW_NAME:-} == muOS && -d /run/muos/device/sdl ]]; then
    [[ -n ${SDL_ROTATION+x} ]] || export SDL_ROTATION="$(cat /run/muos/device/sdl/rotation)"
    [[ -n ${SDL_HQ_SCALER+x} ]] || export SDL_HQ_SCALER="$(cat /run/muos/device/sdl/scaler)"
    [[ -n ${SDL_BLITTER_DISABLED+x} ]] || export SDL_BLITTER_DISABLED="$(cat /run/muos/device/sdl/blitter_disabled)"
fi
# Persisted settings from the retro launcher (config/launcher.cfg). Explicit
# environment values win, so the launcher's own export is preserved.
thing_cfg="$gamedir/config/launcher.cfg"
if [[ -f "$thing_cfg" ]]; then
    v=$(sed -n 's/^fps_limit=//p' "$thing_cfg" | head -n1)
    [[ -n "$v" ]] && export THING_FPS_LIMIT=${THING_FPS_LIMIT:-$v}
    v=$(sed -n 's/^fps_counter=//p' "$thing_cfg" | head -n1)
    [[ -n "$v" ]] && export THING_FPS_OVERLAY=${THING_FPS_OVERLAY:-$v}
    v=$(sed -n 's/^resolution=//p' "$thing_cfg" | head -n1)
    if [[ -n "$v" ]]; then
        if [[ "$v" == "640x480" ]]; then export THING_RENDER_WIDTH=${THING_RENDER_WIDTH:-640}; export THING_RENDER_HEIGHT=${THING_RENDER_HEIGHT:-480}; fi
        if [[ "$v" == "512x384" ]]; then export THING_RENDER_WIDTH=${THING_RENDER_WIDTH:-512}; export THING_RENDER_HEIGHT=${THING_RENDER_HEIGHT:-384}; fi
    fi
    v=$(sed -n 's/^render_thread=//p' "$thing_cfg" | head -n1)
    if [[ "$v" == "threaded" || "$v" == "1" ]]; then export DS_RENDER_THREAD=${DS_RENDER_THREAD:-1}
    elif [[ "$v" == "single" || "$v" == "0" ]]; then export DS_RENDER_THREAD=${DS_RENDER_THREAD:-0}; fi
fi

# Settings are read before defaults; diagnostic launchers' explicit values win.
if [[ -f "$thing_cfg" ]]; then
    v=$(sed -n 's/^buffer_cache=//p' "$thing_cfg" | head -n1)
    if [[ "$v" == "0" || "$v" == "1" ]]; then export THING_GPU_BUFFER_CACHE=${THING_GPU_BUFFER_CACHE:-${DS_GPU_BUFFER_CACHE:-$v}}; fi
fi
export THING_FPS_OVERLAY=${THING_FPS_OVERLAY:-0}
export DS_RENDER_THREAD=${DS_RENDER_THREAD:-0}
export THING_GPU_BUFFER_CACHE=${THING_GPU_BUFFER_CACHE:-${DS_GPU_BUFFER_CACHE:-0}}

# Render-options preferences also apply when entering run-game.sh directly.
# The UI blocks irrelevant controls without erasing their saved preferences.
if [[ -f "$thing_cfg" ]]; then
    for pair in async_present:DS_RT_ASYNC_SWAP one_upload:DS_ONE_UPLOAD ranged_draws:DS_RANGE_DRAW alpha_elide:DS_ALPHA_ELIDE invalidate_depth:DS_INVALIDATE_DEPTH; do
        key=${pair%%:*}; name=${pair#*:}
        v=$(sed -n "s/^${key}=//p" "$thing_cfg" | head -n1)
        if [[ "$v" == "0" || "$v" == "1" ]]; then export "$name=${!name:-$v}"; fi
    done
fi

# Retired launcher brightness override: ignore legacy config and inherited values.
unset THING_BRIGHTNESS DS_BRIGHTNESS
