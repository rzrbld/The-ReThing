#!/usr/bin/env python3
"""Additive GL attribution package; requires the existing decal-option review build."""
import hashlib,json,subprocess,zipfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
sha=lambda b:hashlib.sha256(b).hexdigest()
base=root/'dist/the-thing-decal-option-arm-20261102.zip'
assert sha(base.read_bytes())=='2140b3e36d9e26fcaca0e99e1f4dbf69a29384935352a67b0474ee8f3ec4a895'
with zipfile.ZipFile(base)as z:
 run=z.read('ports/the-thing/run-game-decal-option-0.sh').decode()
 measure=z.read('ports/The Thing Measure Decals OFF.sh').decode()
lib=root/'build/gl-cost-preload-20261102/libthing-glcost.aarch64.so'
assert 'AArch64' in subprocess.check_output(['readelf','-h',str(lib)],text=True)
assert 'PASS: vertex/small-index' in (root/'docs/evidence/gl-cost-preload-v2-20261102/arm-selftest.txt').read_text()
# Only the game child receives LD_PRELOAD; shell/session/telemetry helpers do not.
command='"$gamedir/thething-decal-option.aarch64" "$@"'
assert run.count(command)==1
run=run.replace(command,'LD_PRELOAD="$gamedir/libthing-glcost.aarch64.so${LD_PRELOAD:+:$LD_PRELOAD}" '+command)
measure=measure.replace('decal-option-0-cache','glcost-cache').replace('run-game-decal-option-0.sh','run-game-glcost.sh')
anchor='mode=0\n'
check='''for required in thething-decal-option.aarch64 libthing-decal-audio.aarch64.so libthing-glcost.aarch64.so sample_device_child.sh sample_device.sh; do
    [[ -f "$gamedir/$required" ]] || { echo "Missing $required: install the decal-option review build first." >&2; exit 1; }
done
'''
assert measure.count(anchor)==1;measure=measure.replace(anchor,check+anchor)
measure=measure.replace('files=(log.txt','files=(gl-cost.csv log.txt')
measure=measure.replace('    trap - EXIT INT TERM HUP','''    trap - EXIT INT TERM HUP
    if [[ -n ${glcost_trigger_pid:-} ]]; then
        kill -TERM "$glcost_trigger_pid" 2>/dev/null || true
        wait "$glcost_trigger_pid" 2>/dev/null || true
    fi''',1)
old='if [[ -s "$run/profile.csv" && -s "$run/host-cpu-profile.csv" && -s "$run/render.csv" ]]; then'
new=old.replace(' ]]; then', ''' && -s "$run/gl-cost.csv" ]] &&
       awk -F, 'NR>1 && $2==120 {seen[$1]=1} END {exit !(seen[1] && seen[2])}' "$run/gl-cost.csv"; then''')
assert old in measure;measure=measure.replace(old,new)
measure=measure.replace('export THING_CPU_PROFILE=1','''export THING_CAPTURE_REQUEST=1 THING_GLCOST_START_FRAME=0
export THING_GLCOST_FILE="$gamedir/gl-cost.csv"
export THING_CPU_PROFILE=1''',1)
measure=measure.replace('    date; uname -a','''    date; uname -a
    sha256sum "$gamedir/libthing-glcost.aarch64.so"
    echo "GL API attribution:120 Present windows requested90s/150s after launch; sampled timings include overhead"''')
measure=measure.replace('sampler_pid=$!\nwait "$measure_pid"','''sampler_pid=$!
bash "$gamedir/trigger-glcost.sh" "$gamedir" > "$run/glcost-trigger.log" 2>&1 &
glcost_trigger_pid=$!
wait "$measure_pid"''')
trigger='''#!/bin/bash
set -eu
root=$1
timer=
pending="$root/.glcost-command-$$"
finish() { [[ -z $timer ]] || kill "$timer" 2>/dev/null || true; rm -f -- "$pending"; }
trap finish EXIT
trap 'exit 0' INT TERM HUP
for delay in 90 60; do
    sleep "$delay" & timer=$!
    wait "$timer"; timer=
    if [[ -e "$root/render.control" ]]; then
        echo "Command mailbox busy; capture incomplete" >&2; exit 1
    fi
    printf 'glcost\\n' > "$pending"
    mv -f -- "$pending" "$root/render.control"
    echo "Requested GL cost window at $(date +%H:%M:%S)"
done
'''
readme='''The Thing: optional Mali GL-call diagnostic

Requires the existing decal-option review package and your working installation.
This add-on contains NO game executable, original assets, saves or settings.
Extract at SD-card root. It adds The Thing Measure GL Cost alongside the other
shortcuts; payload remains ports/the-thing. Normal launchers are unchanged.

Use this only when a GL-call capture is requested; it is not another FPS A/B.
Run The Thing Measure GL Cost. Load the same slow save promptly (within60seconds).
Stand at the slow view by80seconds and hold it through105seconds. Then reproduce
the problematic movement/footprints and hold the same view through165seconds.
Two120-frame sampling windows start about90seconds and150seconds after launch.
Let the240-second run exit. Send measurements/*glcost-*.tar.gz. If loading takes
longer, report which scene was visible at those times. Keep the entire archive.

This uses the existing decal-option executable with native audio,640x480,SINGLE,
cacheOFF,batchingOFF and uncappedFPS, exactly the DecalsOFF diagnostic settings.
No save/config changes. Native audio remains enabled. The profiler samples about
1/16GLcalls and splits vertex/small-index/large-index uploads. CPU and wall timings
include profiler overhead; FPS during this run is NOT an optimization comparison.

Host gameplay and emulated ARM forwarding tests passed. Actual Mali execution
of this add-on and stable30FPS remain unverified. Missing windows mark the archive
INCOMPLETE; a COMPLETE archive does not prove it captured gameplay rather than menus.
Use the normal shortcut afterwards. Existing decalOFF/ON tests remain the priority.

Rebuild: bash scripts/build_gl_cost_preload_20261102.sh aarch64
Package: python3 scripts/package_gl_cost_addon_20261102.py
'''
files={'ports/the-thing/libthing-glcost.aarch64.so':lib.read_bytes(),
 'ports/the-thing/run-game-glcost.sh':run.encode(),'ports/the-thing/trigger-glcost.sh':trigger.encode(),
 'ports/the-thing/GL_COST_TEST.txt':readme.encode()}
for directory in ('ports','Roms/PORTS'):files[directory+'/The Thing Measure GL Cost.sh']=measure.encode()
manifest={'base_package_sha256':sha(base.read_bytes()),'hardware_execution':'unverified','files':{n:sha(b)for n,b in files.items()}}
files['ports/the-thing/gl-cost-addon-manifest.json']=json.dumps(manifest,indent=2).encode()
out=root/'dist/the-thing-gl-cost-addon-20261102.zip'
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED)as z:
 for name,data in sorted(files.items()):
  if name.endswith('.sh'):subprocess.run(['bash','-n'],input=data,check=True)
  i=zipfile.ZipInfo(name,(1980,1,1,0,0,0));i.create_system=3;i.external_attr=(0o100755 if name.endswith(('.sh','.so'))else 0o100644)<<16;i.compress_type=zipfile.ZIP_DEFLATED;z.writestr(i,data)
with zipfile.ZipFile(out)as z:assert z.testzip() is None
out.with_suffix('.zip.sha256').write_text(sha(out.read_bytes())+'  '+out.name+'\n')
print(out,out.stat().st_size,sha(out.read_bytes()))
