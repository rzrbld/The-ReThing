#!/bin/bash
# Experimental static port. Device gameplay/performance is not yet qualified.
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

export THING_EARLY_QUEUE_BOUNDS=1 THING_CPU_CLIP=0 DS_CPU_CLIP=0
export THING_NATIVE_STREAM_PROBE="$gamedir/libthing-audio.aarch64.so"
export THING_NATIVE_VORBIS_PROBE="$THING_NATIVE_STREAM_PROBE"
export THING_SKIN_SIMD=0 THING_MATRIX_SIMD=0 THING_INDEX_RANGE=0
export THING_DRAW_BATCH=0 DS_DRAW_BATCH=0 THING_DECAL_BATCH=0 DS_DECAL_BATCH=0
cd "$gamedir" || exit 1
exec > >(tee "$gamedir/log.txt") 2>&1
printf '[package] executable=%s\n' "$gamedir/thething.aarch64"
if command -v sha256sum >/dev/null 2>&1; then
    printf '[package] sha256 '
    sha256sum "$gamedir/thething.aarch64"
fi
"$gamedir/thething-launcher" --apply-shadows "$gamedir" || exit 1
thing_finish() {
    local rc=$?
    if [[ ${THING_SESSION_ACTIVE:-0} != 1 ]] && declare -F pm_finish >/dev/null; then pm_finish; fi
    if declare -F thing_cpu_restore >/dev/null; then thing_cpu_restore; fi
    if declare -F thing_muos_resume >/dev/null; then thing_muos_resume; fi
    return "$rc"
}
trap thing_finish EXIT
trap 'exit 130' INT TERM HUP
if [[ ${THING_SESSION_ACTIVE:-0} != 1 && -f "$gamedir/muos-session.sh" ]]; then
    source "$gamedir/muos-session.sh"
    thing_muos_suspend || exit 1
fi
if [[ -f "$gamedir/cpu-session.sh" ]]; then
    source "$gamedir/cpu-session.sh"
    thing_cpu_begin
fi
if declare -F pm_platform_helper >/dev/null; then pm_platform_helper "$gamedir/thething.aarch64"; fi
"$gamedir/thething.aarch64" "$@"
exit $?
