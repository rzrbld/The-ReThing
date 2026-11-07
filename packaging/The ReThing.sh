#!/bin/bash
# The ReThing - PortMaster launch script.
#
# Follows the PortMaster port template: the firmware's PortMaster control/mod
# scripts handle frontend handover, input focus and cleanup, gptokeyb gives the
# Start+Select quit, and SDL picks the video and audio drivers. Starts the retro
# settings launcher, which then launches the game (run-game.sh) with the chosen
# FPS cap / resolution / counter / worker / buffer-cache settings.
#
# Set THING_ROOT to use a non-standard payload directory.

# PortMaster control/mod scripts permit unset variables.
set +u

# ---- PortMaster -------------------------------------------------------
XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}
if [ -n "${controlfolder:-}" ] && [ -f "$controlfolder/control.txt" ]; then
    :   # set by the caller (firmware or a test harness)
elif [ -d "/opt/system/Tools/PortMaster/" ]; then
    controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
    controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
    controlfolder="$XDG_DATA_HOME/PortMaster"
else
    controlfolder="/roms/ports/PortMaster"
fi
if [ ! -f "$controlfolder/control.txt" ]; then
    # muOS and others keep PortMaster elsewhere when started outside the menu
    for candidate in /PortMaster /mnt/mmc/MUOS/PortMaster /mnt/sdcard/PortMaster /roms/tools/PortMaster \
        /roms2/tools/PortMaster /userdata/system/PortMaster /userdata/roms/ports/PortMaster; do
        if [ -f "$candidate/control.txt" ]; then controlfolder=$candidate; break; fi
    done
fi
if [ -f "$controlfolder/control.txt" ]; then
    export controlfolder
    # shellcheck disable=SC1090,SC1091
    source "$controlfolder/control.txt"
    # shellcheck disable=SC1090
    [ -f "$controlfolder/mod_${CFW_NAME}.txt" ] && source "$controlfolder/mod_${CFW_NAME}.txt"
    get_controls
    export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
    # the-thing-env.sh does not source PortMaster again in this shell (a plain
    # variable: the game stage is a child process and loads it itself)
    thing_portmaster_ready=1
else
    echo "The ReThing: PortMaster not found; running without it" >&2
fi

# ---- the payload ------------------------------------------------------
# BEGIN PORT ROOT RESOLUTION
launcher_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
gamedir=
for candidate in "${THING_ROOT:-}" "${directory:+/${directory#/}/ports/the-thing}" \
    "$launcher_dir" "$launcher_dir/the-thing" "$launcher_dir/../ports/the-thing" \
    "$launcher_dir/../../ports/the-thing" /mnt/sdcard/ports/the-thing /mnt/mmc/ports/the-thing \
    /roms/ports/the-thing /roms2/ports/the-thing /userdata/roms/ports/the-thing; do
    # Test the payload marker, not bin/: first-run installation has no bin/ yet.
    [[ -n "$candidate" && -f "$candidate/the-thing-env.sh" ]] || continue
    gamedir=$candidate
    break
done
if [[ -z "$gamedir" ]]; then
    printf 'The ReThing: cannot find ports/the-thing/the-thing-env.sh. Install the payload in the ports folder or set THING_ROOT.\n' >&2
    exit 1
fi
gamedir=$(cd -- "$gamedir" && pwd) || exit 1
export THING_ROOT="$gamedir"
# END PORT ROOT RESOLUTION
export GAMEDIR="$gamedir"
cd "$gamedir" || exit 1
# One log for the whole session (menu and game); run-game.sh writes into it.
> "$gamedir/log.txt" && exec > >(tee "$gamedir/log.txt") 2>&1
export THING_LOG_ACTIVE=1
printf '[the-thing] payload=%s controlfolder=%s cfw=%s device=%s display=%sx%s\n' "$gamedir" \
    "${controlfolder:-}" "${CFW_NAME:-?}" "${DEVICE_NAME:-?}" "${DISPLAY_WIDTH:-?}" "${DISPLAY_HEIGHT:-?}"
source "$gamedir/the-thing-env.sh" || exit 1
mkdir -p "$gamedir"/{config,userdata,cache,tmp,saves} || exit 1
export XDG_CONFIG_HOME="$gamedir/config" XDG_DATA_HOME="$gamedir/userdata"
export XDG_STATE_HOME="$gamedir/userdata" XDG_CACHE_HOME="$gamedir/cache" TMPDIR="$gamedir/tmp"
thing_menu_finish() {
    local rc=$?
    if declare -F pm_finish >/dev/null; then pm_finish; fi
    return "$rc"
}
trap thing_menu_finish EXIT
thing_menu_cancel() { [[ -z ${menu_pid:-} ]] || { kill -TERM "$menu_pid" 2>/dev/null; wait "$menu_pid"; }; exit 130; }
trap thing_menu_cancel INT TERM HUP
export THING_SESSION_ACTIVE=1
if declare -F pm_platform_helper >/dev/null; then pm_platform_helper "$gamedir/thething-launcher"; fi
unset THING_LAUNCHER_SKIP_MENU
"$gamedir/thething-launcher" "$gamedir" "$gamedir/run-game.sh" &
menu_pid=$!
wait "$menu_pid"
exit $?
