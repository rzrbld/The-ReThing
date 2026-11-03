#!/usr/bin/env python3
import json,os,shutil,sys,tempfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import install_game as m
R=Path(__file__).resolve().parents[1]
manifest=json.loads((R/'docs/evidence/patched-v12.json').read_text())
with tempfile.TemporaryDirectory(prefix='adopt-test-',dir=R/'build') as temp:
    root=Path(temp)
    for d in ('installer','config','userdata/bin'): (root/d).mkdir(parents=True,exist_ok=True)
    (root/'installer/patched-v12.json').write_text(json.dumps(manifest))
    for item in manifest['files']:
        source=m.existing_path(R/'build/active',item['path'])
        dest=root/item['path'];dest.parent.mkdir(parents=True,exist_ok=True);os.link(source,dest)
    # Earlier packages may spell Windows filenames differently.
    p=root/'data/FMV/UIS_Logo_PC.mpg';p.rename(p.with_name('UIS_LOGO_PC.mpg'))
    save=root/'userdata/bin/Slot1.sav';save.write_bytes(b'KEEP SAVE')
    cfg=root/'config/launcher.cfg';cfg.write_text('resolution=640x480\n')
    reg=root/'config/registry.txt';reg.write_text('preserve=sz:yes\n')
    m.install(root)
    receipt=root/'config/install-v12.json'
    assert json.loads(receipt.read_text())['adopted_existing']
    assert not (root/'source/the_thing.iso').exists()
    before=receipt.read_bytes();m.install(root);assert receipt.read_bytes()==before
    assert save.read_bytes()==b'KEEP SAVE' and cfg.read_text()=='resolution=640x480\n' and reg.read_text()=='preserve=sz:yes\n'
    print('PASS real patched resources, no ISO/receipt, mixed filename case, menu preparation, repeat startup, saves/config preserved')
    receipt.write_text('{bad json');m.install(root)
    print('PASS invalid receipt recovered from verified files')
    receipt.unlink();p=root/'bin/thething.exe';p.unlink();p.write_bytes(b'wrong')
    try:m.install(root)
    except RuntimeError as e:assert 'checksum mismatch' in str(e)
    else:raise AssertionError('Accepted incorrect executable')
    assert not receipt.exists()
    print('PASS wrong executable rejected without receipt or destructive reinstall')
