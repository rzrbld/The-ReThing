#!/usr/bin/env python3
"""Fetch checksum-locked Debian ARM tools and stage only their ELF dependencies.
Requires host Python3, dpkg-deb, readelf. Cached .deb files allow offline rebuilds.
"""
import hashlib,json,re,shutil,subprocess,urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
work=ROOT/'build/installer-tools';debs=work/'debs';src=work/'deb-root';out=work/'bundle'
debs.mkdir(parents=True,exist_ok=True)
for item in json.loads((ROOT/'config/installer-packages.lock.json').read_text()):
    p=debs/item['file']
    if not p.is_file():
        partial=p.with_suffix('.download')
        with urllib.request.urlopen(item['url']) as response,partial.open('wb') as f:shutil.copyfileobj(response,f)
        if hashlib.sha256(partial.read_bytes()).hexdigest()!=item['sha256']:raise RuntimeError('Download checksum mismatch: '+item['file'])
        partial.replace(p)
    if hashlib.sha256(p.read_bytes()).hexdigest()!=item['sha256']:raise RuntimeError('Cached package checksum mismatch: '+item['file'])
    subprocess.run(['dpkg-deb','-x',str(p),str(src)],check=True)
for d in ('bin','lib','p7zip','licenses'):(out/d).mkdir(parents=True,exist_ok=True)
queue=[]
for folder,names in [('bin',('unshield','xdelta')),('lib/p7zip',('7z','7z.so'))]:
    for name in names:
        p=src/'usr'/folder/name;shutil.copy2(p,out/('bin' if folder=='bin' else 'p7zip')/name);queue.append(p)
seen=set()
while queue:
    p=queue.pop()
    for name in re.findall(r'Shared library: \[(.*?)\]',subprocess.check_output(['readelf','-d',str(p)],text=True)):
        if name in seen:continue
        seen.add(name)
        candidates=[src/'usr/lib/aarch64-linux-gnu'/name,src/'lib/aarch64-linux-gnu'/name]
        lib=next((p for p in candidates if p.exists()),None)
        if lib is None:raise RuntimeError('Missing library '+name)
        shutil.copy2(lib,out/'lib'/name);queue.append(lib)
shutil.copytree(src/'usr/share/doc',out/'licenses',dirs_exist_ok=True)
# Include the actual shared license texts referenced by Debian copyright files.
commons=Path('/usr/share/common-licenses')
if commons.exists():shutil.copytree(commons,out/'licenses/common-licenses',dirs_exist_ok=True)
shutil.copy2(ROOT/'config/installer-packages.lock.json',out/'packages.lock.json')
(out/'SHA256SUMS').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+str(p.relative_to(out))+'\n' for p in sorted(out.rglob('*')) if p.is_file() and p.name!='SHA256SUMS'))
print('Staged ARM tools and %d dependency libraries: %s'%(len(seen),out))
