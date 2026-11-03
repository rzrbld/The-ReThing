#!/usr/bin/env python3
"""Additive, user-owned-assets H700 early terrain queue experiment."""
import hashlib,json,re,subprocess,zipfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
sha=lambda x:hashlib.sha256(x).hexdigest()
b=root/'build/early-queue-combined-aarch64-20261103'
manifest=json.loads((root/'docs/evidence/early-queue-combined-aarch64-20261103/build-manifest.json').read_text())
assert sha((b/'diagnostic.aarch64').read_bytes())==manifest['diagnostic_sha256']
verification=root/'docs/evidence/early-queue-combined-arm-play-20261103'
assert (verification/'exit.txt').read_text().strip()=='0'
log=(verification/'run.log').read_text()
assert 'level1.dll' in log and 'GAME FAULT' not in log and 'RaiseException' not in log and '[d3d8-render-error]' not in log
metadata=json.loads((verification/'metadata.json').read_text())
assert metadata['binary_sha256']==manifest['diagnostic_sha256']
assert 'QEMU' in metadata['platform']
exe=(b/'thething-queue-bounds.aarch64').read_bytes()
assert exe[:4]==b'\x7fELF' and exe[4]==2 and int.from_bytes(exe[18:20],'little')==183
# Verify stripping preserved loadable program code and architecture, using readelf.
for flag in ('-h','-d'):
 result=subprocess.run(['readelf',flag,str(b/'thething-queue-bounds.aarch64')],text=True,capture_output=True,check=True)
 (b/('readelf-header.txt' if flag=='-h' else 'readelf-dynamic.txt')).write_text(result.stdout)
with zipfile.ZipFile(root/'dist/the-thing-decal-option-arm-20261102.zip') as z:
 run=z.read('ports/the-thing/run-game-decal-option-0.sh').decode()
 measure=z.read('Roms/PORTS/The Thing Measure Decals OFF.sh').decode()
prefix='ports/the-thing/'
files={prefix+'thething-queue-bounds.aarch64':exe,prefix+'libthing-queue-audio.aarch64.so':(root/'build/native-vorbis-arm/probe.aarch64.so').read_bytes()}
for n in ('sample_device.sh','sample_device_child.sh'):files[prefix+n]=(root/'scripts'/n).read_bytes()
for mode,label in ((0,'OFF'),(1,'ON')):
 launcher=f'run-game-queue-bounds-{mode}.sh'
 def adapt(s):
  s=s.replace('thething-decal-option.aarch64','thething-queue-bounds.aarch64').replace('libthing-decal-audio.aarch64.so','libthing-queue-audio.aarch64.so').replace('run-game-decal-option-0.sh',launcher).replace('decal-option-0-',f'queue-bounds-{mode}-')
  # Disable the rejected old per-vertex clip experiment in inherited environments.
  needle='export THING_GPU_VERTEX=1 DS_GPU_VERTEX=1'
  s=s.replace(needle,f'export THING_EARLY_QUEUE_BOUNDS={mode} THING_CPU_CLIP=0 DS_CPU_CLIP=0\n'+needle)
  s=s.replace('export DS_RENDER_THREAD=0 THING_GPU_VERTEX=1 DS_GPU_VERTEX=1',f'export THING_EARLY_QUEUE_BOUNDS={mode} THING_CPU_CLIP=0 DS_CPU_CLIP=0\nexport DS_RENDER_THREAD=0 THING_GPU_VERTEX=1 DS_GPU_VERTEX=1')
  s=s.replace('decal_batch=0"',f'decal_batch=0 early_queue_bounds={mode}"')
  s=s.replace('source "$gamedir/the-thing-env.sh" || exit 1', 'source "$gamedir/the-thing-env.sh" || exit 1\nexport THING_FPS_LIMIT=0 DS_FPS_LIMIT=0 THING_DRAW_ERROR_CHECK_EVERY=16')
  if '[package] executable=' in s:s=s.replace("printf '[package] executable=",f"printf '[queue-bounds] requested mode={mode} SINGLE cache=OFF 640x480\\n'\nprintf '[package] executable=")
  return s
 r=adapt(run);m=adapt(measure)
 # Both plain and measured runs explicitly select the same experiment mode.
 assert f'THING_EARLY_QUEUE_BOUNDS={mode}' in r and f'THING_EARLY_QUEUE_BOUNDS={mode}' in m
 files[prefix+launcher]=r.encode()
 for folder in ('Roms/PORTS','ports'):
  files[f'{folder}/The Thing Queue Bounds {label}.sh']=r.encode()
  files[f'{folder}/The Thing Measure Queue Bounds {label}.sh']=m.encode()
for n in ('LICENSE.ogg','LICENSE.vorbis'):files[prefix+'queue-audio-'+n]=(root/'build/native-vorbis-arm'/n).read_bytes()
files[prefix+'QUEUE_BOUNDS_README.txt']=(root/'docs/QUEUE_BOUNDS_TEST_20261103.txt').read_bytes()
files[prefix+'queue-bounds-manifest.json']=json.dumps(dict(debug_elf_sha256=manifest['diagnostic_sha256'],stripped_elf_sha256=sha(exe),composition=manifest['composition'],hardware_performance='unverified',validation='AArch64 QEMU/software GLES saved level; not H700',files={n:sha(d) for n,d in files.items()}),indent=2).encode()
out=root/'dist/the-thing-queue-bounds-arm-20261103.zip'
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for n,d in sorted(files.items()):
  if n.endswith('.sh'):subprocess.run(['bash','-n'],input=d,check=True)
  i=zipfile.ZipInfo(n,(1980,1,1,0,0,0));i.create_system=3;i.external_attr=(0o100755 if n.endswith(('.sh','.aarch64','.so')) else 0o100644)<<16;i.compress_type=zipfile.ZIP_DEFLATED;z.writestr(i,d)
with zipfile.ZipFile(out) as z:assert z.testzip() is None
out.with_suffix('.zip.sha256').write_text(sha(out.read_bytes())+'  '+out.name+'\n')
print(out,out.stat().st_size)
