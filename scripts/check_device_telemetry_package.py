#!/usr/bin/env python3
import os,subprocess,tempfile,zipfile,tarfile,json,hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(dir=root/'build',prefix='telemetry-package-check-')as t:
 base=Path(t)
 with zipfile.ZipFile(root/'dist/the-thing-device-telemetry-20261102.zip')as z:
  z.extractall(base)
  manifest=json.loads(z.read('ports/the-thing/device-telemetry-manifest.json'))
  for name,sha in manifest['files'].items():assert hashlib.sha256(z.read(name)).hexdigest()==sha
 (base/'fixture.c').write_text('#include <unistd.h>\nint main(void){sleep(4);return 0;}\n')
 subprocess.run(['cc',str(base/'fixture.c'),'-o',str(base/'fixture')],check=True)
 g=base/'ports/the-thing';(g/'bin').mkdir();(g/'the-thing-env.sh').touch();(g/'libthing-audio.aarch64.so').write_bytes(b'test fixture')
 for kind,binary,launcher in [('Baseline','thething.aarch64','run-game.sh'),('CPU Combined','thething-cpu-combined.aarch64','run-game-cpu-combined.sh')]:
  (g/binary).symlink_to(base/'fixture')
  (g/launcher).write_text('''#!/bin/bash
for name in log.txt profile.csv render.csv host-cpu-profile.csv; do echo test-fixture > "$THING_ROOT/$name"; done
"$THING_ROOT/'''+binary+'''" 4
exit 7
''')
  env={k:v for k,v in os.environ.items()if not k.startswith(('THING_','DS_'))}
  r=subprocess.run(['bash',str(base/'Roms/PORTS'/f'The Thing Measure {kind} Telemetry.sh')],env=env,capture_output=True,text=True,timeout=12)
  assert r.returncode==7,(r.returncode,r.stdout,r.stderr)
 archives=list((g/'measurements').glob('*.tar.gz'));assert len(archives)==2
 for a in archives:
  with tarfile.open(a)as tar:
   names=tar.getnames();assert not any('/previous' in n or n.endswith('/RUNNING')for n in names)
   for leaf in ('exit.txt','COMPLETE.txt','device-telemetry.csv','device-telemetry.log','settings.txt'):
    matches=[n for n in names if n.endswith('/'+leaf)]
    assert matches,(leaf,names,[(n,tar.extractfile(n).read().decode(errors='replace'))for n in names if n.endswith('.log')],r.stdout,r.stderr)
    n=matches[0];data=tar.extractfile(n).read()
    if leaf=='exit.txt':assert data==b'7\n'
    if leaf=='device-telemetry.csv':assert b'VmRSS' in data and b'MemAvailable' in data
    if leaf=='device-telemetry.log':assert b'telemetry pid=' in data
 e=root/'docs/evidence/device-child-sampler-20261102';e.mkdir(exist_ok=True)
 (e/'package-test.txt').write_text('PASS two paired shortcut fixtures: root resolution, exact process telemetry, exit7 preserved, archive finalized, previous/RUNNING excluded, manifest hashes verified. Fixtures are not real gameplay evidence.\n')
 print((e/'package-test.txt').read_text())
