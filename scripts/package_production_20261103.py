#!/usr/bin/env python3
"""Reproduce the consolidated own-copy distribution from validated artifacts."""
import hashlib,json,subprocess,zipfile
from pathlib import Path
R=Path(__file__).resolve().parents[1]
P='ports/the-thing/'
sha=lambda b:hashlib.sha256(b).hexdigest()
with zipfile.ZipFile(R/'dist/the-thing-own-copy-review-20261102.zip') as z:
    files={n:z.read(n) for n in z.namelist() if not n.endswith('/')}
for n in list(files):
    if n.endswith(('.py','MANIFEST.json','PERFORMANCE_PAUSED.txt','PREBUILT-FOR-REVIEW.txt','INSTALLER-TESTS.txt','INSTALLER-VALIDATION.txt')) or 'Measure Cache' in n:
        del files[n]
def add(dst,src):files[dst]=(R/src).read_bytes()
exe=R/'build/early-queue-combined-aarch64-20261103/thething-queue-bounds.aarch64'
assert sha(exe.read_bytes())=='db5298c8538c288bb960984b21d2b20a6211f08b69221264afb2d061a3a29cea'
files[P+'thething.aarch64']=exe.read_bytes()
add(P+'thething-launcher','build/launcher/thething-launcher.aarch64')
add(P+'libthing-audio.aarch64.so','build/native-vorbis-arm/probe.aarch64.so')
for n in ('LICENSE.ogg','LICENSE.vorbis'):add(P+'licenses/'+n,'build/native-vorbis-arm/'+n)
for n in ('the-thing-env.sh','cpu-session.sh','muos-session.sh'):add(P+n,'packaging/'+n)
add(P+'run-game.sh','packaging/The Thing.sh')
add(P+'FIRST_RUN_INSTALL.txt','docs/FIRST_RUN_INSTALL.txt')
add(P+'NATIVE_SETUP.txt','docs/NATIVE_SETUP_20261103.txt')
for folder in ('ports/','Roms/PORTS/'):
    for n in ('The ReThing.sh','The Thing Measure.sh'):add(folder+n,'packaging/'+n)
for n in ('sample_device.sh','sample_device_child.sh'):add(P+n,'scripts/'+n)
readme='''THE RETHING — consolidated play-test distribution, 2026-11-03

The prebuilt ARM executable is provided for review. Supply your own supported
original game files. No original game assets or saves are included.

INSTALL / UPDATE
Extract the archive at the SD-card root, merging ports and Roms directories.
Replace packaged files; keep your existing ports/the-thing/bin, data, config,
userdata and source directories. This archive contains no registry, launcher
configuration or save files, so those are not overwritten by extraction.
On muOS launch Roms/PORTS/The ReThing.sh. On PortMaster layouts use
ports/The ReThing.sh. All payload files stay in ports/the-thing.
This is a manual PortMaster-layout package, not a PortMaster catalogue release.

For a fresh install, place your supported ISO at
ports/the-thing/source/the_thing.iso and start The ReThing.
The first-run UI extracts and applies 1.1 then 1.2 and verifies the supported
executable. See FIRST_RUN_INSTALL.txt for the supported disc hash/requirements.
Existing valid installations are verified and reused even without an installer receipt or ISO. Bash, SDL2 and GLES are required from the firmware. The launcher contains
native setup and shadow configuration; no Python is needed on the handheld.
Extraction tools are bundled.

RECOMMENDED SETTINGS
640X480, SINGLE renderer, BUFFER CACHE OFF, SHADOWS SIMPLE.
Existing launcher settings are preserved: select these explicitly after update
if you previously enabled threaded rendering or buffer caching.
FPS cap is optional; UNCAPPED is useful to observe performance.
SHADOWS SIMPLE sets only the original CircleShadowLOD threshold to 1;
ORIGINAL restores 0.2. It does not disable lights, weather, decals or textures.

INCLUDED
Previously tested save/load, rendering, colour and scaling corrections in the
combined executable; 83 local x87-TOP routines; native streaming/whole-sound
Vorbis; early ordinary-terrain bounds rejection enabled. Rejected SIMD,
late CPU clipping, index-range and draw/decal batching experiments are off.
No added brightness pass. Controller instructions: CONTROLS.md.

VALIDATION / LIMITS
The exact game executable was previously exercised through level 1 under
AArch64 QEMU and measured on H700. That H700 comparison measured about
17.0 -> 17.8 FPS in its late slow section; it did not reach stable 30 FPS.
Simple shadows passed a local 115-second gameplay screening, saving about
0.36-0.38 ms CPU/frame in one comparison. This new shadow setting and complete
package have not been tested on H700. No guaranteed 30 FPS claim.
Host launcher settings, shortcut lifecycle, registry preservation and ARM ELF
architecture checks passed. Archive contents are listed with hashes in
MANIFEST.json. The launcher was rebuilt for AArch64 and visually inspected.

TESTING
Launch normally and try gameplay, saving and loading. For diagnostics enable
FPS COUNTER, or run The Thing Measure at the same location. It writes under
ports/the-thing/measurements. Preserve log.txt immediately after a failure.
Do not use old experimental shortcuts: those can select older executables.
'''
files['READ-ME.txt']=readme.encode();files[P+'RELEASE-20261103.txt']=readme.encode()
files[P+'PREBUILT-FOR-REVIEW.txt']=b'The prebuilt ARM executable is provided for review. Users must supply their own supported original game files. See OWN_COPY_BUILD.txt for the source build workflow.\n'
for n in ('thething.aarch64','thething-launcher','libthing-audio.aarch64.so'):
    b=files[P+n];assert b[:6]==b'\x7fELF\x02\x01' and int.from_bytes(b[18:20],'little')==183,n
for n,b in files.items():
    assert not n.endswith('.py'), n
    assert not any(n.startswith(P+x) for x in ('bin/','data/','userdata/','config/','extracted-iso/')),n
    assert not n.endswith(('.iso','.sav')),n
    if n.endswith('.sh'):subprocess.run(['bash','-n'],input=b,check=True)
files[P+'MANIFEST.json']=json.dumps({'release':'20261103-consolidated-simple-shadows','device_status':'combined executable previously tested; new shadow default and full package H700 unverified','files':{n:{'sha256':sha(b),'size':len(b)} for n,b in sorted(files.items())}},indent=2).encode()
out=R/'dist/the-thing-production-review-20261103-r3.zip'
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
    for n,b in sorted(files.items()):
        i=zipfile.ZipInfo(n,(2026,11,3,0,0,0));i.create_system=3
        executable=n.endswith(('.sh','.aarch64','.so')) or n.endswith(('thething-launcher','/7z','/unshield','/xdelta','/ld-linux-aarch64.so.1'))
        i.external_attr=(0o100755 if executable else 0o100644)<<16;i.compress_type=zipfile.ZIP_DEFLATED
        z.writestr(i,b)
with zipfile.ZipFile(out) as z:
    assert z.testzip() is None
    for n,m in json.loads(z.read(P+'MANIFEST.json'))['files'].items():assert sha(z.read(n))==m['sha256']
out.with_suffix('.zip.sha256').write_text(sha(out.read_bytes())+'  '+out.name+'\n')
print(out, out.stat().st_size,sha(out.read_bytes()))
