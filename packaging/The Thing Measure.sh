#!/bin/bash
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
[[ -d "$gamedir/bin" ]] || { echo "Install the game through The ReThing first." >&2; exit 1; }
mode=${THING_MEASURE_CACHE:-0}
export THING_INDEX_RANGE=${THING_INDEX_RANGE:-0}
run="$gamedir/measurements/$(date +%Y%m%d-%H%M%S)-cache${mode}-scan${THING_INDEX_RANGE}-$$"
mkdir -p "$run/previous" || exit 1
files=(log.txt profile.csv render.csv host-cpu-profile.csv host-call-profile.csv host-cpu-timeline.csv cpu-timeline-info.txt host-profile-maps.txt cpu-profile.csv cpu-profile-info.txt)
finished=0
measurement_finish() {
    local rc=$?
    [[ $finished == 0 ]] || return
    finished=1
    trap - EXIT INT TERM HUP
    printf '%s\n' "$rc" > "$run/exit.txt"
    for file in "${files[@]}"; do
        [[ ! -f "$gamedir/$file" ]] || cp -p -- "$gamedir/$file" "$run/"
    done
    rm -f "$run/RUNNING"
    if [[ -s "$run/profile.csv" && -s "$run/host-cpu-profile.csv" && -s "$run/render.csv" ]]; then
        printf 'Capture finalized; game exit=%s. Profiles belong to this run.\n' "$rc" > "$run/COMPLETE.txt"
    else
        printf 'INCOMPLETE capture; game exit=%s. Inspect measurement-launcher.log and log.txt. Do not use previous/ as this run.\n' "$rc" > "$run/INCOMPLETE.txt"
    fi
    tar -czf "$run.tar.gz" --exclude=previous -C "$(dirname -- "$run")" "$(basename -- "$run")"
    printf 'Measurement saved: %s.tar.gz\n' "$run"
    exit "$rc"
}
measurement_cancel() {
    [[ -z ${measure_pid:-} ]] || { kill -TERM "$measure_pid" 2>/dev/null; wait "$measure_pid"; }
    exit 130
}
trap measurement_finish EXIT
trap measurement_cancel INT TERM HUP
for file in "${files[@]}"; do
    [[ ! -f "$gamedir/$file" ]] || mv -- "$gamedir/$file" "$run/previous/" || exit 1
done
printf 'Measurement has started but has NOT finalized. Wait for the game to exit; send the .tar.gz, not this running directory.\n' > "$run/RUNNING"
export DS_RENDER_THREAD=0 THING_GPU_VERTEX=1 DS_GPU_VERTEX=1
export THING_GPU_BUFFER_CACHE="$mode" THING_FPS_LIMIT=0 THING_FPS_OVERLAY=0
export THING_FAST_SHADER=1 THING_BIND_CACHE=1 THING_DRAW_ERROR_CHECK_EVERY=16 THING_RING_VB=0
export THING_RENDER_WIDTH=640 THING_RENDER_HEIGHT=480 DS_RENDER_WIDTH=640 DS_RENDER_HEIGHT=480
export THING_DRAW_BATCH=0 DS_DRAW_BATCH=0 THING_MATRIX_SIMD=0 THING_SKIN_SIMD=0
export THING_CPU_PROFILE=1 THING_TRACE_RENDER=1 THING_GL_STATS=1
export THING_COPY_STATS=1 THING_PERF_PROBE=1 THING_PROFILE_SECONDS=240
{
    date; uname -a
    sha256sum "$gamedir/thething.aarch64"
    printf 'root=%s\nlauncher=%s\n' "$gamedir" "$gamedir/run-game.sh"
    echo "REQUESTED SINGLE GPU 640x480 uncapped overlay=off cache=$mode; verify actual mode in log.txt"
    echo "index_range=$THING_INDEX_RANGE; CPU timeline enabled; cutscenes may be skipped"
} > "$run/settings.txt"
bash "$gamedir/run-game.sh" > "$run/measurement-launcher.log" 2>&1 &
measure_pid=$!
wait "$measure_pid"
exit $?
