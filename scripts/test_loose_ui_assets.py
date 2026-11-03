#!/usr/bin/env python3
"""Local isolated original-asset VFS fallback experiment; invoke with xvfb-run.
Does not modify active assets, builds, package or device. The deliberately
incomplete ResourceDir tests loose resolution without permanent MainUI mount.
"""
import hashlib,json,os,pathlib,shutil,subprocess,time,zipfile
from PIL import Image
project=pathlib.Path(__file__).resolve().parents[1]
all_ui=os.environ.get('THING_LOOSE_UI_ALL')=='1'
full=os.environ.get('THING_LOOSE_UI_CCTV')=='1'
case='loose-ui-complete' if all_ui else 'loose-ui-full-strip' if full else 'loose-ui-fallback'
limit=120 if full and not all_ui else 215
fixture=project/'build'/case;out=project/'docs/evidence'/case
for d in [fixture,out,fixture/'bin',fixture/'data',fixture/'config',fixture/'data/Textures']:d.mkdir(parents=True,exist_ok=True)
shutil.copy2(project/'build/font-candidate/thething.host',fixture/'thething.host')
shutil.copy2(project/'build/active/bin/thething.exe',fixture/'bin/thething.exe')
# Private directory overlay: never extract through an active-data symlink.
for src in (project/'build/active/data').rglob('*'):
 dest=fixture/'data'/src.relative_to(project/'build/active/data')
 if src.is_dir():dest.mkdir(parents=True,exist_ok=True)
 elif not dest.exists():dest.symlink_to(src)
lines=(project/'build/font-candidate/config/registry.txt').read_text().splitlines()
lines=[s for s in lines if '\\resourcedir=' not in s.lower()]
for root in ['HKEY_LOCAL_MACHINE','HKEY_CURRENT_USER']:
 lines.append(root+'\\SOFTWARE\\Computer Artworks\\The Thing\\1.0\\ResourceDir=sz:C:\\data')
(fixture/'config/registry.txt').write_text('\n'.join(lines)+'\n')
selected=['fnt_handelgothic.3df','fnt_handelgothic.png','fnt_hootie.3df','fnt_hootie.png','gui_newloadingscreen.png']
if full:selected+=['eff_cctvnoise.dds','eff_cctvstatic.dds','eff_cctvdarkband.dds']
manifest={'archive_sha256':hashlib.sha256((project/'build/active/data/mainui.pak').read_bytes()).hexdigest(),'binary_sha256':hashlib.sha256((fixture/'thething.host').read_bytes()).hexdigest(),'registry':'ResourceDir=REG_SZ C:\\data in HKCU and HKLM','assets':[]}
with zipfile.ZipFile(project/'build/active/data/mainui.pak') as archive:
 members=archive.namelist() if all_ui else ['textures/'+name for name in selected]
 paths={};seen=set()
 for member in members:
  if archive.getinfo(member).is_dir():continue
  parts=pathlib.PurePosixPath(member).parts
  assert parts and not member.startswith('/') and all(p not in ('.','..') for p in parts)
  key=member.casefold();assert key not in seen;seen.add(key)
  first={'textures':'Textures','levels':'Levels','sound':'Sound'}[parts[0].casefold()]
  dest=fixture/'data'/first/pathlib.Path(*parts[1:]);dest.parent.mkdir(parents=True,exist_ok=True)
  data=archive.read(member)
  if dest.exists():
   assert dest.read_bytes()==data, 'archive/loose collision: '+str(dest)
   if dest.is_symlink():dest.unlink()
  dest.write_bytes(data)
  manifest['assets'].append({'archive':'build/active/data/mainui.pak','member':member,'loose_file':str(dest.relative_to(project)),'size':len(data),'sha256':hashlib.sha256(data).hexdigest()})
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
shutil.copy2(fixture/'config/registry.txt',out/'registry-before.txt')
for n in ['input.commands','capture.request','render.control','capture.bmp','render-frame.txt','capture-frame.txt']:(fixture/n).unlink(missing_ok=True)
env=os.environ.copy();env.update(THING_ROOT=str(fixture),SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',THING_WINDOWED='1',THING_TEST_INPUT='1',THING_TRACE_INPUT='1',THING_CAPTURE_REQUEST='1',THING_TRACE_RENDER='1',THING_GPU_VERTEX='1',THING_GPU_BUFFER_CACHE='1',THING_PROFILE_SECONDS=str(limit),HLE_FILETRACE='1')
start=time.monotonic();sent=set();captures=[];rss_samples=[];next_rss=0
with (out/'run.log').open('w') as log:
 proc=subprocess.Popen([str(fixture/'thething.host')],env=env,cwd=project,stdout=log,stderr=subprocess.STDOUT)
 try:
  while proc.poll() is None and time.monotonic()-start<limit+10:
   elapsed=time.monotonic()-start
   if elapsed>=next_rss:
    try:
     fields=dict(line.split(':',1) for line in pathlib.Path('/proc',str(proc.pid),'status').read_text().splitlines() if ':' in line)
     rss_samples.append({'seconds':elapsed,'rss_kib':int(fields['VmRSS'].split()[0]),'peak_rss_kib':int(fields['VmHWM'].split()[0])})
    except (FileNotFoundError,KeyError):pass
    next_rss=elapsed+1
   for at in [35,42]:
    if elapsed>=at and at not in sent:
     with (fixture/'input.commands').open('a') as f:f.write('tap a\n')
     sent.add(at)
   for at in [43,96,100,104,108,112,170,180,190,205]:
    key='request'+str(at)
    if elapsed>=at and key not in sent:
     (fixture/'capture.request').touch();(fixture/'render.control').write_text('trace\n');sent.add(key)
    saved='saved'+str(at)
    if elapsed>=at+0.8 and saved not in sent and (fixture/'capture.bmp').exists():
     stem=out/('after%03d'%at)
     Image.open(fixture/'capture.bmp').save(str(stem)+'.png')
     for name in ['render-frame.txt','capture-frame.txt']:
      if (fixture/name).exists():shutil.copy2(fixture/name,out/('after%03d-'%at+name))
     captures.append(at);sent.add(saved)
   time.sleep(.05)
 finally:
  if proc.poll() is None:proc.terminate()
  code=proc.wait(timeout=5)
(out/'memory.json').write_text(json.dumps({'samples':rss_samples,'peak_rss_kib':max((r['peak_rss_kib'] for r in rss_samples),default=None)},indent=2)+'\n')
(out/'result.json').write_text(json.dumps({'exit':code,'seconds':time.monotonic()-start,'captures':captures},indent=2)+'\n')
shutil.copy2(fixture/'config/registry.txt',out/'registry-after.txt')
print(json.dumps({'exit':code,'seconds':time.monotonic()-start,'directory':str(out)}))
