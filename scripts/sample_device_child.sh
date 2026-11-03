#!/bin/bash
# Find only this launcher's descendant with the exact executable, then sample it.
set -eu
[[ $# == 3 && $1 =~ ^[0-9]+$ ]] || { echo 'usage: sample_device_child.sh LAUNCHER_PID EXECUTABLE NEW_OUTPUT.csv' >&2; exit 2; }
root_pid=$1
wanted=$(readlink -f -- "$2")
output=$3
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
identity() {
 local record
 IFS= read -r record < "/proc/$1/stat" || return 1
 record=${record##*) }
 local -a fields
 read -ra fields <<< "$record"
 [[ ${fields[0]} != Z && ${fields[0]} != X ]] || return 1
 printf '%s' "${fields[19]}"
}
root_start=$(identity "$root_pid") || exit 1
printf 'telemetry search launcher=%s start=%s wanted=%s method=proc-stat-ppid\n' "$root_pid" "$root_start" "$wanted" >&2
while [[ -r /proc/$root_pid/stat && $(identity "$root_pid") == "$root_start" ]]; do
 # /proc/PID/task/PID/children is optional on older handheld kernels.
 # Build a parent map from stat instead; never attach by process name alone.
 declare -A child_map=()
 for stat in /proc/[0-9]*/stat; do
  record=
  { IFS= read -r record < "$stat"; } 2>/dev/null || continue
  tail=${record##*) }
  read -ra fields <<< "$tail"
  [[ ${#fields[@]} -ge 20 && ${fields[1]} =~ ^[0-9]+$ ]] || continue
  id=${stat#/proc/};id=${id%/stat};parent=${fields[1]}
  child_map[$parent]="${child_map[$parent]:-} $id"
 done
 queue=("$root_pid")
 for ((i=0;i<${#queue[@]} && i<128;i++)); do
  pid=${queue[i]}
  actual=$(readlink -f -- "/proc/$pid/exe" 2>/dev/null || true)
  if [[ $actual == "$wanted" || /proc/$pid/exe -ef "$wanted" ]]; then
   printf 'telemetry pid=%s executable=%s launcher=%s\n' "$pid" "$actual" "$root_pid" >&2
   exec bash "$script_dir/sample_device.sh" "$pid" "$output"
  fi
  for child in ${child_map[$pid]:-}; do queue+=("$child"); done
 done
 printf 'telemetry waiting: examined=%s descendants; no matching executable yet\n' "${#queue[@]}" >&2
 sleep 2
done
echo 'telemetry: launcher exited before matching game process was observed' >&2
exit 1
