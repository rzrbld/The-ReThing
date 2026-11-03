#!/bin/bash
# Read-only Linux telemetry for a specific game process. Run on the target.
set -eu
if [[ $# != 2 || ! $1 =~ ^[0-9]+$ ]]; then
    echo "usage: bash sample_device.sh GAME_PID NEW_OUTPUT.csv" >&2
    exit 2
fi
game_pid=$1
output=$2
start_id() {
    local record
    IFS= read -r record < "/proc/$game_pid/stat" || return 1
    record=${record##*) }
    local -a fields
    read -ra fields <<< "$record"
    [[ ${fields[0]} != Z && ${fields[0]} != X ]] || return 1
    printf '%s' "${fields[19]}"
}
identity=$(start_id) || exit 1
# Refuse to replace a previous measurement.
set -o noclobber
exec 3> "$output"
printf 'uptime_seconds,metric,value,unit\n' >&3
while [[ -r /proc/$game_pid/stat ]] && [[ $(start_id) == "$identity" ]]; do
    read -r uptime unused < /proc/uptime
    while read -r key value unit; do
        case "$key" in
            VmRSS:|VmHWM:|VmSwap:|Threads:)
                printf '%s,%s,%s,%s\n' "$uptime" "${key%:}" "$value" "${unit:-count}" >&3 ;;
        esac
    done < "/proc/$game_pid/status" || break
    while read -r key value unit; do
        case "$key" in
            MemAvailable:|SwapFree:)
                printf '%s,%s,%s,%s\n' "$uptime" "${key%:}" "$value" "$unit" >&3 ;;
        esac
    done < /proc/meminfo
    for file in /sys/class/thermal/thermal_zone*/temp \
                /sys/devices/system/cpu/cpufreq/policy*/scaling_cur_freq; do
        [[ -r $file ]] || continue
        read -r value < "$file" || continue
        parent=${file%/*}
        sensor=${parent##*/}
        case "$file" in
            */temp) metric="${sensor}_temperature"; unit=millidegrees_C ;;
            *) metric="${sensor}_cpu_frequency"; unit=kHz ;;
        esac
        printf '%s,%s,%s,%s\n' "$uptime" "$metric" "$value" "$unit" >&3
    done
    sleep 2
done
