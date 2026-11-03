#!/usr/bin/env python3
"""Validate the installable ZIP, not repository submission policy."""
import argparse,hashlib,io,json,os,subprocess,sys,tempfile,zipfile
import xml.etree.ElementTree as ET
from pathlib import Path
from PIL import Image
R=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--archive',type=Path,default=R/'dist/the_rething.zip')
args=parser.parse_args()
with zipfile.ZipFile(args.archive) as z:
 names=z.namelist();top={n.split('/')[0] for n in names}
 assert not any(Path(n).name.startswith(('libmali.so','libGLESv2.so')) for n in names), 'GPU driver or link-only stub must not be packaged'
 assert top=={'The ReThing.sh','the_rething','port.json','README.md','gameinfo.xml','screenshot.png'}
 for n in ('port.json','README.md','gameinfo.xml','screenshot.png'):
  assert z.read(n)==z.read('the_rething/'+n)
 assert z.testzip() is None
 assert not any('Measure' in n or 'sample_device' in n for n in names)
 assert b'THING_PERF_PROBE:-0' in z.read('the_rething/the-thing-env.sh')
 meta=json.loads(z.read('the_rething/port.json'))
 assert meta['version']==4 and meta['name']=='the_rething.zip'
 assert isinstance(meta['items'],list) and isinstance(meta['items_opt'],list)
 assert meta['attr']['title']=='The ReThing' and isinstance(meta['attr']['porter'],list)
 assert isinstance(meta['attr']['genres'],list) and isinstance(meta['attr']['exp'],bool)
 assert set(meta['items'])=={'The ReThing.sh','the_rething/'}
 assert meta['attr']['arch']==['aarch64'] and meta['attr']['runtime']==[] and not meta['attr']['rtr']
 for game in ET.fromstring(z.read('the_rething/gameinfo.xml')).findall('game'):
  for tag in ('path','image'):assert game.findtext(tag).removeprefix('./') in names
 assert Image.open(io.BytesIO(z.read('the_rething/screenshot.png'))).size==(640,480)
 for n,m in json.loads(z.read('the_rething/MANIFEST.json'))['files'].items():
  b=z.read(n);assert len(b)==m['size'] and hashlib.sha256(b).hexdigest()==m['sha256']
 for n in names:
  assert not n.startswith(('ports/','Roms/','the_rething/bin/','the_rething/data/','the_rething/config/','the_rething/userdata/'))
  assert not n.endswith(('.iso','.sav','.py'))
  if n.endswith('.sh'):subprocess.run(['bash','-n'],input=z.read(n),check=True);assert b'\r\n' not in z.read(n)
 for n in ('thething.aarch64','thething-launcher','libs.aarch64/libthing-audio.aarch64.so'):
  b=z.read('the_rething/'+n);assert b[:6]==b'\x7fELF\x02\x01' and int.from_bytes(b[18:20],'little')==183
 print('PASS layout, PortMaster metadata, XML paths, screenshot, manifest, ELF architecture, no user data/Python')
 with tempfile.TemporaryDirectory(prefix='thing-package-',dir=R/'build') as temp:
  base=Path(temp);ports=base/'SD card/ports';game=ports/'the_rething';pm=base/'PortMaster';pm.mkdir(parents=True)
  # Test actual packaged scripts with fake firmware/processes; never run services.
  for n in names:
   if n.endswith('.sh') or n=='the_rething/libs.aarch64/libthing-audio.aarch64.so':
    p=ports/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(z.read(n))
  (pm/'control.txt').write_text('CFW_NAME=Test\nget_controls() { sdl_controllerconfig=testmapping; }\npm_platform_helper() { echo "helper:$(basename "$1")" >> "$PM_TEST_LOG"; }\npm_finish() { echo finish >> "$PM_TEST_LOG"; }\n')
  (game/'muos-session.sh').write_text('thing_muos_suspend() { :; }; thing_muos_resume() { :; }\n')
  (game/'cpu-session.sh').write_text('thing_cpu_begin() { :; }; thing_cpu_restore() { :; }\n')
  (game/'thething.aarch64').write_text('#!/bin/bash\n[[ "$THING_NATIVE_STREAM_PROBE" == "$THING_ROOT/libs.aarch64/libthing-audio.aarch64.so" && -f "$THING_NATIVE_STREAM_PROBE" ]] || exit 90\necho game >> "$PM_TEST_LOG"\nexit 7\n')
  (game/'thething-launcher').write_text('#!/bin/bash\nif [[ "$1" == --apply-shadows ]]; then exec "$PM_TEST_NATIVE" "$@"; fi\n[[ "$THING_ROOT" == "$1" ]] || exit 91\nif [[ "$PM_TEST_MODE" == game ]]; then exec bash "$2"; fi\nexit 0\n')
  shortcuts=base/'SD card/Roms/PORTS';shortcuts.mkdir(parents=True)
  (shortcuts/'The ReThing.sh').write_bytes(z.read('The ReThing.sh'))
  for folder in (ports,shortcuts):
   for mode,expected in [('cancel',0),('game',7)]:
    # Deliberately clear executable bits; entry must restore them.
    for n in ('thething.aarch64','thething-launcher'):(game/n).chmod(0o644)
    log=base/'events';log.unlink(missing_ok=True)
    env={k:v for k,v in os.environ.items() if not k.startswith(('THING_','DS_','PM_'))}
    env.update(DEVICE_ARCH='aarch64',controlfolder=str(pm),PM_TEST_LOG=str(log),PM_TEST_MODE=mode,PM_TEST_NATIVE=str(R/'build/launcher/thething-launcher.host'))
    p=subprocess.run(['bash',str(folder/'The ReThing.sh')],env=env,cwd='/',capture_output=True,text=True)
    assert p.returncode==expected,(p.returncode,p.stdout,p.stderr)
    assert log.read_text().splitlines().count('finish')==1
    if mode=='game':assert 'game' in log.read_text().splitlines()
  print('PASS packaged entry from ports and muOS shortcut folder, spaces, chmod repair, native settings, audio path, exit code and cleanup')
print('ALL INSTALLABLE PACKAGE CHECKS PASSED (host fixtures; no new H700 run)')
