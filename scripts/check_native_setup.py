#!/usr/bin/env python3
"""Exercise compiled setup with real assets and bundled ARM tools under QEMU."""
import hashlib,json,os,shutil,subprocess,tempfile,sys,fcntl,time,signal
from pathlib import Path
R=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(R/'scripts'));from install_game import existing_path
exe=R/'build/launcher/thething-launcher.host'
command=[str(exe)]
if '--arm' in sys.argv:
 command=['bash',str(R/'scripts/run_qemu_aarch64.sh')]
env=os.environ.copy();env['THING_SETUP_QEMU']=str(R/'build/tools/qemu-user/usr/bin/qemu-aarch64')
if '--arm' in sys.argv:env['THING_QEMU_BINARY']=str(R/'build/launcher/thething-launcher.aarch64')
manifest=json.loads((R/'docs/evidence/patched-v12.json').read_text())
def invoke(root,mode='--setup',ok=True):
 p=subprocess.run(command+[mode,str(root)],env=env,capture_output=True,text=True)
 if (p.returncode==0)!=ok:raise AssertionError(p.stdout+p.stderr)
 return p
with tempfile.TemporaryDirectory(prefix='native-setup-',dir=R/'build') as temp:
 base=Path(temp);root=base/'existing game';root.mkdir()
 shutil.copytree(R/'build/installer-tools/bundle',root/'tools')
 for i in manifest['files']:
  src=existing_path(R/'build/active',i['path']);dst=root/i['path'];dst.parent.mkdir(parents=True,exist_ok=True);os.link(src,dst)
 for d in ('config','userdata/bin'): (root/d).mkdir(parents=True,exist_ok=True)
 (root/'userdata/bin/Slot1.sav').write_bytes(b'KEEP')
 (root/'config/launcher.cfg').write_text('resolution=640x480\nshadow_quality=0\n')
 (root/'config/registry.txt').write_text('unrelated=sz:KEEP\n')
 invoke(root);assert (root/'config/install-native-v1.txt').exists();invoke(root)
 assert (root/'userdata/bin/Slot1.sav').read_bytes()==b'KEEP'
 assert (root/'config/registry.txt').read_text()=='unrelated=sz:KEEP\n'
 invoke(root,'--apply-shadows');a=(root/'config/registry.txt').read_text();assert 'CircleShadowLOD=sz:1' in a and 'unrelated=sz:KEEP' in a
 invoke(root,'--apply-shadows');assert (root/'config/registry.txt').read_text()==a
 env['THING_SHADOW_QUALITY']='1';invoke(root,'--apply-shadows');env.pop('THING_SHADOW_QUALITY');assert 'CircleShadowLOD=sz:0.2' in (root/'config/registry.txt').read_text()
 print('PASS native adoption without ISO/receipt, MainUI extraction, repeat, save/config preservation, shadow toggle',flush=True)
 # Journal recovery restores the previous valid binary and then adopts it.
 work=root/'tmp/native-install-work';work.mkdir();(root/'bin').rename(work/'previous-bin');(root/'bin').mkdir();(root/'bin/bad').write_text('partial')
 (root/'tmp/native-publishing.txt').write_text('11');invoke(root);assert not (root/'bin/bad').exists()
 print('PASS interrupted publication recovery',flush=True)
 # Old Python journal is also recovered before validating the restored payload.
 legacy=root/'tmp/install-work';legacy.mkdir();(root/'bin').rename(legacy/'previous-bin');(root/'bin').mkdir();(root/'bin/bad').write_text('partial')
 (root/'tmp/install-publishing.json').write_text('{"bin": true, "data": true}')
 invoke(root);assert not (root/'bin/bad').exists()
 with (root/'tmp/install.lock').open('w') as lock:
  fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB);assert 'already running' in invoke(root,ok=False).stderr
 (root/'data/test-link').symlink_to(root/'config/launcher.cfg');assert 'Symlink' in invoke(root,ok=False).stderr;(root/'data/test-link').unlink()
 print('PASS legacy rollback, concurrent setup rejection and symlink rejection',flush=True)
 (root/'config/install-native-v1.txt').unlink();p=root/'bin/thething.exe';p.unlink();p.write_bytes(b'wrong');invoke(root,ok=False);assert not (root/'config/install-native-v1.txt').exists()
 print('PASS wrong executable rejected',flush=True)
 empty=base/'fresh';empty.mkdir();shutil.copytree(root/'tools',empty/'tools');invoke(empty,ok=False)
 (empty/'source/the_thing.iso').write_bytes(b'wrong');invoke(empty,ok=False);(empty/'source/the_thing.iso').unlink()
 print('PASS missing/wrong ISO rejected',flush=True)
 if '--full' in sys.argv:
  os.link(R/'The_NEW_ISO_UNPROTECTED/the_thing.iso',empty/'source/the_thing.iso')
  process=subprocess.Popen(command+['--setup',str(empty)],env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,start_new_session=True)
  try:
   deadline=time.monotonic()+60
   while time.monotonic()<deadline:
    status=empty/'tmp/install-status.txt'
    if status.exists() and 'EXTRACTING ISO' in status.read_text():break
    if process.poll() is not None:raise AssertionError('Installer exited before cancellation')
    time.sleep(.005)
   else:raise AssertionError('No extraction progress')
   os.killpg(process.pid,signal.SIGTERM);process.wait(timeout=10)
  finally:
   if process.poll() is None:os.killpg(process.pid,signal.SIGKILL);process.wait()
  assert not (empty/'config/install-native-v1.txt').exists()
  print('PASS cancellation leaves no completed installation receipt',flush=True)
  invoke(empty)
  for i in manifest['files']:assert hashlib.sha256(existing_path(empty,i['path']).read_bytes()).hexdigest()==i['sha256']
  assert (empty/'config/install-native-v1.txt').exists()
  print('PASS full ISO -> 1.1 -> 1.2 -> supported exe -> MainUI; all payload hashes match',flush=True)
print('ALL NATIVE SETUP CHECKS PASSED')
