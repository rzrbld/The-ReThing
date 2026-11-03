#!/usr/bin/env python3
"""Validate real original staging plus hostile/colliding ZIP paths; no tar build."""
import hashlib,io,json,stat,tarfile,tempfile,zipfile
from pathlib import Path
from stage_mainui import stage_mainui,merge_inputs
root=Path(__file__).resolve().parents[1]
staging=root/'build/package/mainui-loose'
archive=root/'build/active/data/mainui.pak'
before=hashlib.sha256(archive.read_bytes()).hexdigest()
inputs=stage_mainui(archive,root/'build/active/data',staging)
manifest=json.loads((staging/'original-mainui-assets.json').read_text())
assert len(manifest['assets'])==103 and sum(a['size'] for a in manifest['assets'])==17693202
with zipfile.ZipFile(archive) as z:
 for row in manifest['assets']:
  file=staging/row['path'].removeprefix('the-thing/')
  data=file.read_bytes();assert data==z.read(row['archive_member'])
  assert hashlib.sha256(data).hexdigest()==row['sha256']
assert len({name.casefold() for _,name in inputs})==len(inputs)
assert all(name.startswith(('the-thing/data/Textures/','the-thing/data/Levels/','the-thing/data/Sound/','the-thing/licenses/')) or name=='the-thing/original-mainui-assets.json' for _,name in inputs)
# Construct a small in-memory staging-only archive to verify safe members and
# provenance inclusion. The actual final candidate archive is root-owned.
buf=io.BytesIO()
with tarfile.open(fileobj=buf,mode='w') as tar:
 for path,name in inputs:tar.add(path,arcname=name,recursive=False)
buf.seek(0)
with tarfile.open(fileobj=buf,mode='r') as tar:
 names=tar.getnames();assert 'the-thing/licenses/original-mainui-assets.txt' in names
 assert len(names)==105 and all('saves' not in n and 'config/' not in n and 'userdata/' not in n for n in names)
 for row in manifest['assets']:assert hashlib.sha256(tar.extractfile(row['path']).read()).hexdigest()==row['sha256']
with tempfile.TemporaryDirectory() as t:
 t=Path(t);data=t/'game-data';data.mkdir();out=t/'stage';bad=t/'bad.zip'
 def rejected(entries,prepare=None):
  with zipfile.ZipFile(bad,'w') as z:
   for key,value in entries:z.writestr(key,value)
  try:stage_mainui(bad,data,out)
  except ValueError:return
  raise AssertionError('Accepted invalid/colliding archive')
 rejected([('../saves/player.sav',b'bad')]);rejected([('/textures/a.png',b'bad')]);rejected([('textures/../../saves/player.sav',b'bad')]);rejected([('textures\\a.png',b'bad')])
 rejected([('textures/A.png',b'a'),('textures/a.png',b'b')])
 link=zipfile.ZipInfo('textures/link.png');link.create_system=3;link.external_attr=(stat.S_IFLNK|0o777)<<16
 rejected([(link,b'../../saves/player.sav')])
 (data/'Textures').mkdir();(data/'Textures/a.png').write_bytes(b'original')
 rejected([('textures/A.png',b'different')]);assert (data/'Textures/a.png').read_bytes()==b'original'
 # Identical existing files are accepted; merging uses a single package path.
 with zipfile.ZipFile(bad,'w') as z:z.writestr('textures/a.png',b'original')
 with zipfile.ZipFile(data/'resources.pak','w') as z:z.writestr('textures/A.png',b'different')
 try:stage_mainui(bad,data,out)
 except ValueError:pass
 else:raise AssertionError('Accepted differing other-archive collision')
 with zipfile.ZipFile(data/'resources.pak','w') as z:z.writestr('textures/A.png',b'original')
 staged=stage_mainui(bad,data,out)
 accepted=json.loads((out/'original-mainui-assets.json').read_text())
 assert accepted['identical_other_archive'][0]['archive']=='resources.pak'
 merged=merge_inputs([(data/'Textures/a.png','the-thing/data/Textures/a.png')],staged)
 assert len(merged)==3
 try:stage_mainui(bad,data,data/'bad-stage')
 except ValueError:pass
 else:raise AssertionError('Accepted staging inside active data')
 assert not (data/'saves').exists()
assert hashlib.sha256(archive.read_bytes()).hexdigest()==before
report={'status':'PASS','original_members':103,'original_bytes':17693202,'staged_files_including_manifest_notice':len(inputs),'archive_sha256':before,'checks':['all original bytes and SHA256','canonical Textures/Sound/Levels','casefold duplicates','ZIP traversal/absolute/backslash/symlink rejection','different loose-file collision rejection','identical package path deduplication','different other-archive collision rejection/identical bytes accepted','staging isolated from original data','105-member in-memory tar includes provenance and excludes saves/config/userdata']}
p=root/'docs/evidence/loose-ui-complete/staging-check.json';p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
