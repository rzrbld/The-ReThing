#!/usr/bin/env python3
"""Generate a single dispatch index for the statically rebased game modules."""
from pathlib import Path
import json
import re
import hashlib
import pefile
import os
root=Path(__file__).resolve().parents[1]
reports=json.loads((root/'docs/evidence/translation-summary.json').read_text())
out=root/'build/game';out.mkdir(parents=True,exist_ok=True)
def write(path,source):
    if path.exists() and path.read_text()==source:return
    temporary=path.with_name(path.name+f'.{os.getpid()}.tmp')
    temporary.write_text(source)
    temporary.replace(path)
header='#include "win32hle.h"\n#include <strings.h>\n#include <string.h>\n'
entries=[]
export_arrays=[]
export_modules=[]
for report in reports:
    module=re.sub(r'[^a-z0-9]','_',Path(report['module']).stem.lower())
    header+=f'extern const recomp_dispatch_entry_t ds_{module}_dispatch[];\n'
    header+=f'extern const uint32_t ds_{module}_count;\n'
    catalog=json.loads((root/'build/translated'/module/'catalog.json').read_text())
    original=root/'build/active'/('bin/thething.exe' if report['module'].lower()=='thething.exe' else 'data/Levels/'+report['module'])
    if hashlib.sha256(original.read_bytes()).hexdigest()!=report['sha256']:
        raise SystemExit(f'Translation input hash does not match active installation: {original}')
    pe=pefile.PE(str(original))
    exports=[]
    for symbol in getattr(getattr(pe,'DIRECTORY_ENTRY_EXPORT',None),'symbols',[]):
        if symbol.forwarder:raise SystemExit('Forwarded game exports require explicit resolution')
        name=json.dumps(symbol.name.decode()) if symbol.name else 'NULL'
        exports.append(f'{{{name},{symbol.ordinal}u,{int(report["image_base"],16)+symbol.address:#x}u}}')
    export_arrays.append('static const GameExport exports_'+module+'[]={'+','.join(exports)+'};')
    export_modules.append('{'+json.dumps(report['module'])+',exports_'+module+',sizeof(exports_'+module+')/sizeof(GameExport),'+report['image_base']+'u}')
    if report['module'].lower()=='thething.exe':
        source='#include "win32hle.h"\n'
        source+='\n'.join(f'extern void {f["symbol"]}(void);' for f in catalog)
        source+='\nconst recomp_dispatch_entry_t recomp_dispatch_table[]={\n'
        source+='\n'.join(f'{{{f["va"]}u,{f["symbol"]}}},' for f in catalog)
        source+=f'\n}};\nconst uint32_t recomp_dispatch_count={len(catalog)};\n'
        write(out/'main_dispatch.c',source)
    else:
        entries.append(f'{{"{report["module"]}",{report["image_base"]}u,{report["entry"]}u,ds_{module}_dispatch,&ds_{module}_count,0}}')
source=header+'''
typedef struct { const char *name; uint32_t base,entry; const recomp_dispatch_entry_t *table; const uint32_t *count; int active; } Module;
static Module modules[]={
'''+',\n'.join(entries)+'''
};
static const char *basename_(const char *name) {
    const char *p=name; for(const char *s=name;*s;s++)if(*s=='/'||*s=='\\\\')p=s+1; return p;
}
uint32_t ds_module_preferred(const char *name) {
    if(!name)return 0;name=basename_(name);
    for(unsigned i=0;i<sizeof modules/sizeof *modules;i++)if(!strcasecmp(name,modules[i].name))return modules[i].base;
    return 0;
}
void ds_module_loaded(const char *name,uint32_t base) {
    extern void ds_dispatch_cache_invalidate(void) __attribute__((weak));
    if(ds_dispatch_cache_invalidate)ds_dispatch_cache_invalidate();
    for(unsigned i=0;i<sizeof modules/sizeof *modules;i++)if(modules[i].base==base){modules[i].active=1;return;}
    hle_fatal("unregistered translated DLL %s @ %#x",name,base);
}
void ds_module_unloaded(uint32_t base) {
    extern void ds_dispatch_cache_invalidate(void) __attribute__((weak));
    if(ds_dispatch_cache_invalidate)ds_dispatch_cache_invalidate();
    for(unsigned i=0;i<sizeof modules/sizeof *modules;i++)if(modules[i].base==base)modules[i].active=0;
}
recomp_func_t ds_module_lookup(uint32_t va) {
    if(va<0x10000000u || va>=0x25000000u)return NULL;
    for(unsigned i=0;i<sizeof modules/sizeof *modules;i++) {
        Module *m=&modules[i]; if(!m->active || va<m->base || va>=m->base+0x1000000)continue;
        uint32_t lo=0,hi=*m->count;
        while(lo<hi){uint32_t mid=lo+(hi-lo)/2;if(m->table[mid].address==va)return m->table[mid].func;if(m->table[mid].address<va)lo=mid+1;else hi=mid;}
    }
    return NULL;
}
'''
source+='''
typedef struct { const char *name; uint32_t ordinal,va; } GameExport;
'''+ '\n'.join(export_arrays)+'''
typedef struct { const char *name; const GameExport *symbols; unsigned count; uint32_t base; } GameExports;
static const GameExports game_exports[]={
'''+',\n'.join(export_modules)+'''
};
uint32_t thing_game_export(const char *module,const char *name,uint32_t ordinal) {
    if(!module)return 0;
    module=basename_(module);
    for(unsigned i=0;i<sizeof(game_exports)/sizeof(*game_exports);i++) {
        const GameExports *e=&game_exports[i];
        if(strcasecmp(e->name,module))continue;
        if(e->base!=0x400000u) {
            int active=0;
            for(unsigned j=0;j<sizeof(modules)/sizeof(*modules);j++)
                if(modules[j].base==e->base)active=modules[j].active;
            if(!active)return 0;
        }
        for(unsigned j=0;j<e->count;j++) {
            const GameExport *s=&e->symbols[j];
            if((name && s->name && !strcmp(s->name,name)) || (!name && s->ordinal==ordinal))return s->va;
        }
        return 0;
    }
    return 0;
}
'''
write(out/'modules.c',source)
print(f'Generated main dispatch and {len(entries)} DLL dispatch tables')
