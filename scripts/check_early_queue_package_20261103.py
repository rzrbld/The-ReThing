#!/usr/bin/env python3
"""Check ZIP layout, hashes and real launcher root resolution using a mock game."""
import hashlib,json,os,subprocess,tempfile,zipfile
from pathlib import Path
root=Path(__file__).resolve().parents[1];archive=root/'dist/the-thing-queue-bounds-arm-20261103.zip'
with zipfile.ZipFile(archive) as z:
 assert z.testzip() is None
 names=z.namelist();assert len(names)==len(set(names))
 assert not any('/data/' in n or '/bin/' in n or '/userdata/' in n or n.endswith(('.sav','.iso','.exe','.dll')) for n in names)
 manifest=json.loads(z.read('ports/the-thing/queue-bounds-manifest.json'))
 for n,h in manifest['files'].items():assert hashlib.sha256(z.read(n)).hexdigest()==h,n
 for n in names:
  assert not n.startswith('/') and '..' not in Path(n).parts
  if n.endswith('.sh'):subprocess.run(['bash','-n'],input=z.read(n),check=True)
 with tempfile.TemporaryDirectory(prefix='thing-queue-package-') as tmp:
  card=Path(tmp);game=card/'ports/the-thing';game.mkdir(parents=True)
  (game/'the-thing-env.sh').write_text('# Mock installed port environment; no system changes.\n')
  exe=game/'thething-queue-bounds.aarch64'
  exe.write_text('''#!/bin/bash
printf '%s|%s|%s|%s|%s|%s|%s\\n' "$THING_ROOT" "$THING_EARLY_QUEUE_BOUNDS" "$DS_RENDER_THREAD" "$THING_GPU_BUFFER_CACHE" "$THING_RENDER_WIDTH" "$THING_RENDER_HEIGHT" "$THING_CPU_CLIP" > "$THING_ROOT/mock-settings.txt"
''');exe.chmod(0o755)
  for folder in ('Roms/PORTS','ports'):
   for mode,label in ((0,'OFF'),(1,'ON')):
    name=f'{folder}/The Thing Queue Bounds {label}.sh';script=card/name;script.parent.mkdir(parents=True,exist_ok=True);script.write_bytes(z.read(name))
    env={k:v for k,v in os.environ.items() if not k.startswith(('THING_','DS_'))};env.pop('GAMEDIR',None)
    subprocess.run(['bash',str(script)],cwd=card,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,check=True)
    assert (game/'mock-settings.txt').read_text().strip()==f'{game}|{mode}|0|0|640|480|0',name
print('PASS: ZIP hashes/assets/layout; shell syntax; OFF/ON launcher root resolution from both Roms/PORTS and ports (mock executable).')
