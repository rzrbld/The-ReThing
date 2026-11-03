#!/usr/bin/env python3
"""Exercise real ISO/patch installation plus recovery; no target FPS claims."""
import hashlib, importlib.util, json, os, shutil, signal, subprocess, sys, time, tempfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import install_game as installer
ROOT=Path(__file__).resolve().parents[1]
base=Path(tempfile.mkdtemp(prefix='first-run-check-',dir=ROOT/'build'))
print('Test artifacts:',base,flush=True)
root=base/'port with spaces'
root.mkdir(exist_ok=True)
for d in ('installer','source','userdata/bin','config'): (root/d).mkdir(parents=True,exist_ok=True)
for n in ('install_game.py','stage_mainui.py'):shutil.copy2(ROOT/'scripts'/n,root/'installer'/n)
shutil.copy2(ROOT/'docs/evidence/patched-v12.json',root/'installer/patched-v12.json')
shutil.copytree(ROOT/'build/installer-tools/bundle',root/'tools',dirs_exist_ok=True)
(root/'userdata/bin/Slot1.sav').write_bytes(b'PRESERVE USER SAVE')
(root/'config/launcher.cfg').write_text('resolution=640x480\n')
original_run=subprocess.run
qemu=ROOT/'build/tools/qemu-user/usr/bin/qemu-aarch64'
def run_arm(command,*args,**kwargs):
    if str(command[0]).endswith('/ld-linux-aarch64.so.1'): command=[str(qemu)]+list(command)
    return original_run(command,*args,**kwargs)
installer.subprocess.run=run_arm
# Missing and invalid ISO should not publish a payload.
for data in (None,b'WRONG ISO'):
    iso=root/'source/the_thing.iso';iso.unlink(missing_ok=True)
    if data is not None: iso.write_bytes(data)
    try: installer.install(root)
    except RuntimeError as e: print('PASS expected error:',e)
    else: raise AssertionError('Invalid ISO accepted')
    assert not (root/'config/install-v12.json').exists()
shutil.copy2(ROOT/'The_NEW_ISO_UNPROTECTED/the_thing.iso',root/'source/the_thing.iso')
start=time.monotonic();installer.install(root)
print('PASS real ARM tools under QEMU: full original -> 1.1 -> 1.2 -> executable -> MainUI in %.2fs (not device timing)'%(time.monotonic()-start))
manifest=json.loads((root/'installer/patched-v12.json').read_text());installer.verify(root,manifest)
assets=json.loads((root/'config/original-mainui-assets.json').read_text())['assets']
for item in assets:
    assert installer.digest(root/item['path'].removeprefix('the-thing/'))==item['sha256']
print('PASS all patched files and %d loose MainUI files match hashes'%len(assets))
# Existing install doesn't extract again, and neither save nor launcher settings change.
mtime=(root/'data/mainui.pak').stat().st_mtime_ns
installer.install(root)
assert (root/'data/mainui.pak').stat().st_mtime_ns==mtime
assert (root/'userdata/bin/Slot1.sav').read_bytes()==b'PRESERVE USER SAVE'
assert (root/'config/launcher.cfg').read_text()=='resolution=640x480\n'
print('PASS repeat launch skips extraction; saves/settings preserved')
# Simulate power loss after bin publication, before data publication; force ISO failure
# after rollback so we can inspect the recovered old payload.
work=root/'tmp/install-work';work.mkdir(exist_ok=True)
(root/'tmp/install-publishing.json').write_text(json.dumps({'bin':True,'data':True}))
(root/'bin').rename(work/'previous-bin');(root/'bin').mkdir();(root/'bin/bad').write_text('partial')
iso=root/'source/the_thing.iso';iso.rename(root/'source/test-held.iso')
installer.install(root) # recovered valid payload can now be adopted without the ISO
installer.verify(root,manifest)
assert not (root/'bin/bad').exists()
assert (root/'config/install-v12.json').exists()
(root/'source/test-held.iso').rename(iso)
installer.install(root)
print('PASS interrupted publication rolls back; retry fully validates and installs')
# Cancellation of an extraction child must not produce a ready marker.
cancel=base/'cancel';cancel.mkdir(exist_ok=True)
for d in ('installer','source','tools'):shutil.copytree(root/d,cancel/d,dirs_exist_ok=True)
program='''import sys\nfrom pathlib import Path\nsys.path.insert(0,sys.argv[1])\nimport install_game as m\nr=m.subprocess.run\ndef wrapped(command,*a,**kw):\n if str(command[0]).endswith('/ld-linux-aarch64.so.1'): command=[sys.argv[3]]+list(command)\n return r(command,*a,**kw)\nm.subprocess.run=wrapped\nm.install(Path(sys.argv[2]))\n'''
p=original_run # keep original available for later tools
child=subprocess.Popen([sys.executable,'-c',program,str(ROOT/'scripts'),str(cancel),str(qemu)],start_new_session=True)
end=time.monotonic()+30
while time.monotonic()<end:
    status=cancel/'tmp/install-status.txt'
    if status.exists() and 'EXTRACTING ISO' in status.read_text():break
    if child.poll() is not None:raise AssertionError('Installer exited before cancellation')
    time.sleep(.005)
else: raise AssertionError('No extraction progress')
os.killpg(child.pid,signal.SIGTERM);child.wait(timeout=10)
assert not (cancel/'config/install-v12.json').exists()
installer.install(cancel);installer.verify(cancel,manifest)
print('PASS cancel during extraction leaves no accepted partial payload; retry succeeds')
print('ALL FIRST-RUN CHECKS PASSED')
