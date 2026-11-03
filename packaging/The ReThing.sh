#!/bin/bash
# The ReThing: starts the retro settings launcher, which then launches the game
# with the chosen FPS cap / resolution / counter / worker / buffer-cache settings.
# BEGIN PORT ROOT RESOLUTION
launcher_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
if [[ -n ${THING_ROOT:-} ]]; then
    gamedir=$THING_ROOT
    if [[ ! -f "$gamedir/the-thing-env.sh" ]]; then
        printf 'The Thing: invalid THING_ROOT: %s\n' "$gamedir" >&2
        exit 1
    fi
else
    gamedir=
    # muOS shortcuts can live in Roms/PORTS while payload lives in card/ports.
    # Test the payload marker, not bin/: first-run installation has no bin/ yet.
    for candidate in "$launcher_dir" "$launcher_dir/the-thing" \
        "$launcher_dir/../../ports/the-thing" \
        "${directory:+/${directory#/}/ports/the-thing}" \
        /mnt/sdcard/ports/the-thing /mnt/mmc/ports/the-thing \
        /roms/ports/the-thing /roms2/ports/the-thing; do
        [[ -n "$candidate" && -f "$candidate/the-thing-env.sh" ]] || continue
        gamedir=$candidate
        break
    done
    if [[ -z "$gamedir" ]]; then
        printf 'The Thing: cannot find ports/the-thing/the-thing-env.sh. Install the payload in the card ports folder or set THING_ROOT.\n' >&2
        exit 1
    fi
fi
gamedir=$(cd -- "$gamedir" && pwd) || exit 1
export THING_ROOT="$gamedir"
# END PORT ROOT RESOLUTION
export GAMEDIR="$gamedir"
source "$gamedir/the-thing-env.sh" || exit 1
mkdir -p "$gamedir"/{config,userdata,cache,tmp,saves} || exit 1
export XDG_CONFIG_HOME="$gamedir/config" XDG_DATA_HOME="$gamedir/userdata"
export XDG_STATE_HOME="$gamedir/userdata" XDG_CACHE_HOME="$gamedir/cache" TMPDIR="$gamedir/tmp"
source "$gamedir/muos-session.sh"
thing_menu_finish() {
    local rc=$?
    if declare -F pm_finish >/dev/null; then pm_finish; fi
    thing_muos_resume
    return "$rc"
}
trap thing_menu_finish EXIT
thing_menu_cancel() { [[ -z ${menu_pid:-} ]] || { kill -TERM "$menu_pid" 2>/dev/null; wait "$menu_pid"; }; exit 130; }
trap thing_menu_cancel INT TERM HUP
thing_muos_suspend || exit 1
export THING_SESSION_ACTIVE=1
if declare -F pm_platform_helper >/dev/null; then pm_platform_helper "$gamedir/thething-launcher"; fi
unset THING_LAUNCHER_SKIP_MENU
"$gamedir/thething-launcher" "$gamedir" "$gamedir/run-game.sh" &
menu_pid=$!
wait "$menu_pid"
exit $?
