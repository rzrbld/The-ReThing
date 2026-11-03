#!/usr/bin/env bash
# Host-only load-independent A/B: exact user+sys CPU seconds of the game
# process for a fixed 60 s presentation window, divided by presented frames.
# Runs on a dedicated Xvfb so Xvfb CPU is not counted. This is host x86 Mesa
# llvmpipe evidence, never H700 performance.
# Usage: measure_host_cpu.sh TAG BINARY [SECONDS] [extra env K=V ...]
set -euo pipefail
cd "$(dirname "$0")/.."
tag=$1; binary=$2; seconds=${3:-60}; shift 3 || true
root="build/host-cpu/$tag"; out="docs/evidence/host-cpu/$tag"
rm -rf "$root" "$out"; mkdir -p "$root/bin" "$root/data" "$root/config" "$out"
cp build/active/bin/thething.exe "$root/bin/thething.exe"
cp build/font-candidate/config/registry.txt "$root/config/registry.txt"
for s in build/active/data/*; do ln -s "$(realpath "$s")" "$root/data/$(basename "$s")"; done
: > "$root/input.commands"
LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
  __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
  THING_ROOT="$PWD/$root" SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy \
  THING_WINDOWED=1 THING_TEST_INPUT=1 THING_PROFILE_SECONDS="$seconds" \
  THING_PROFILE_FRAMES="${MEASURE_FRAMES:-0}" \
  THING_CPU_PROFILE=1 "$@" \
  xvfb-run -a -s '-screen 0 800x600x24' \
  sh -c '/usr/bin/time -f "%U %S" -o "$1/cpu.txt" "$2" > "$1/run.log" 2>&1' sh "$PWD/$out" "$binary" || true
frame=$(tail -1 "$root/profile.csv" 2>/dev/null | cut -d, -f1)
seconds_done=$(tail -1 "$root/profile.csv" 2>/dev/null | cut -d, -f2)
read -r user sys < "$out/cpu.txt" || { user=0; sys=0; }
python3 - "$out" "$tag" "$binary" "$user" "$sys" "$frame" "$seconds_done" "$root" <<'PY'
import csv,json,sys,hashlib
out,tag,binary,user,sys_,frame,secs,root=sys.argv[1],sys.argv[2],sys.argv[3],float(sys.argv[4]),float(sys.argv[5]),sys.argv[6],sys.argv[7],sys.argv[8]
cpu=user+sys_
frames=int(frame) if frame else 0
samples=0
try:
    for row in csv.DictReader(open(root+'/host-cpu-profile.csv')):
        samples+=int(row['cpu_samples'])
except FileNotFoundError:
    pass
main_cpu_s=samples*0.010
res={'tag':tag,'binary_sha256':hashlib.sha256(open(binary,'rb').read()).hexdigest(),
     'process_cpu_user_s':user,'process_cpu_sys_s':sys_,'process_cpu_s':cpu,'frames':frames,
     'presentation_s':float(secs) if secs else 0,
     'main_thread_samples':samples,'main_thread_cpu_s':main_cpu_s,
     'main_ms_per_frame':(main_cpu_s/frames*1000) if frames else None,
     'frames_per_main_cpu_s':(frames/main_cpu_s) if main_cpu_s else None,
     'scope':'host x86_64 Mesa llvmpipe; main-thread CPU from 10 ms CLOCK_THREAD_CPUTIME_ID samples (guest+renderer main thread, not llvmpipe workers); not H700 evidence'}
open(out+'/result.json','w').write(json.dumps(res,indent=2)+'\n')
print(json.dumps({k:res[k] for k in ('tag','main_thread_samples','frames','main_ms_per_frame','presentation_s')}))
PY
