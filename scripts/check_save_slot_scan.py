#!/usr/bin/env python3
"""Drive the actual translated slot scanner in an initialized game under GDB."""
import os,pathlib,subprocess,shutil
p=pathlib.Path(__file__).resolve().parents[1];r=p/'build/save-slot-scan';e=p/'docs/evidence/save-fix'/os.environ.get('SLOT_SCAN_EVIDENCE','scan');r.mkdir(exist_ok=True);e.mkdir(parents=True,exist_ok=True)
for n in ('bin','data','config'):(r/n).mkdir(exist_ok=True)
shutil.copy2(p/'build/active/bin/thething.exe',r/'bin/thething.exe')
for f in (p/'build/active/data').iterdir():
 d=r/'data'/f.name
 if not d.exists():d.symlink_to(f)
script='''set pagination off
set confirm off
handle SIGPIPE nostop noprint pass
break present if frames >= 120
run
python
import gdb,struct
base=int(gdb.parse_and_eval('g_mem_base'))
saved=int(gdb.parse_and_eval('g_ecx'))
inf=gdb.selected_inferior()
for routine,offset,name in ((0x406280,0x90,'SAVE'),(0x402800,0x88,'LOAD')):
 ptr=int(gdb.parse_and_eval('(void*)hle_alloc(4096)'))
 inf.write_memory(ptr,bytes(4096))
 for repeat in range(2):
  gdb.execute('set g_ecx = %d'%(ptr-base))
  gdb.execute('call (unsigned int)hle_call_guest(%d,0,0)'%routine)
  for i in range(8):
   a=struct.unpack('<I',inf.read_memory(ptr+offset+i*16+4,4))[0]
   label=bytes(inf.read_memory(base+a,64)).decode('utf-16-le').split('\\0')[0] if a else ''
   print(name+'_SLOT',repeat,i+1,repr(label))
   assert label == 'Empty Slot', 'unexpected empty-slot label: '+repr(label)
gdb.execute('set g_ecx = %d'%saved)
print('SLOT_SCAN_PASS save and load scans repeated; all eight empty labels')
end
kill
quit
'''
(e/'scan.gdb').write_text(script)
env=os.environ.copy();env.update(THING_ROOT=str(r),SDL_VIDEODRIVER='x11',SDL_AUDIODRIVER='dummy',THING_WINDOWED='1',THING_GPU_VERTEX='1',DS_RENDER_THREAD='0',THING_PROFILE_SECONDS='90',LIBGL_ALWAYS_SOFTWARE='1',SDL_VIDEO_X11_FORCE_EGL='1')
with (e/'run.log').open('w') as log:
 result=subprocess.run(['timeout','110','gdb','--batch','-x',str(e/'scan.gdb'),os.environ.get('SLOT_SCAN_BINARY',str(p/'build/game/thething.host'))],cwd=p,env=env,stdout=log,stderr=subprocess.STDOUT)
text=(e/'run.log').read_text();print(text[-5000:]);
if os.environ.get('SLOT_SCAN_EXPECT_FAILURE')=='1':
 assert 'guest exceptions are not handled' in text and 'SLOT_SCAN_PASS' not in text
 print('BASELINE_FAILURE_CONFIRMED')
else:
 assert 'SLOT_SCAN_PASS' in text and 'guest exceptions are not handled' not in text
