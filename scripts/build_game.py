#!/usr/bin/env python3
"""Build translated modules and native host, with per-unit logs and caching.

Use via build_game.sh for the pinned AArch64 container or directly for host.
Unimplemented lifted instructions remain fatal traps in bring-up builds.
"""
import argparse
import concurrent.futures
import fcntl
import hashlib
import json
import os
import re
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def run(cmd, log):
    with log.open('w') as f:
        result = subprocess.run([str(x) for x in cmd], stdout=f, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f'{log}:\n{log.read_text()[-5000:]}')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--target', choices=['host', 'aarch64'], default='host')
    p.add_argument('--jobs', type=int, default=4)
    p.add_argument('--output-root', type=Path, default=ROOT/'build/game',
                   help='isolated object/executable destination; generated dispatch inputs remain in build/game')
    p.add_argument('--translated-source-root', type=Path,
                   help='explicit staged translation tree for isolated experiments')
    p.add_argument('--extra-object', type=Path, action='append', default=[],
                   help='additional precompiled object for an isolated build experiment')
    p.add_argument('--allow-traps', action='store_true')
    p.add_argument('--local-regs', action='store_true',
                   help='compile generated code with RECOMP_LOCAL_REGS (registers in host locals)')
    p.add_argument('--translated-opt', default='-O1',
                   help='optimisation level for translated guest code (default -O1)')
    p.add_argument('--icall-trace', action='store_true',
                   help='keep the always-on indirect-call ring-buffer trace (diagnostics)')
    p.add_argument('--render-thread', action='store_true',
                   help='build the experimental render-worker variant: stage the runtime hooks, '
                        'compile probe/rt/render_thread_proxy.c and link with --wrap for every GL call')
    p.add_argument('--lto', action='store_true',
                   help='build translated guest code with -flto (slim LTO objects, gcc-ar; '
                        'link-time optimisation of the whole guest program; experiment)')
    p.add_argument('--pgo', choices=['off', 'generate', 'use'], default='off',
                   help='isolated translated-code profile-guided experiment')
    p.add_argument('--profile-dir', type=Path,
                   help='same profile directory for training and optimized compilation')
    p.add_argument('--mem-model', choices=['volatile','nonvolatile'], default='volatile',
                   help='guest load/store qualifier model (nonvolatile is A/B only)')
    p.add_argument('--fixed-guest-base', default='0',
                   help='map the guest 4 GiB at this host address and fold it into every '
                        'guest memory access (e.g. 0x100000000; 0 keeps the runtime base)')
    args = p.parse_args()
    profile_stamp = 0
    if args.pgo != 'off':
        if not args.profile_dir or args.output_root.resolve() == (ROOT/'build/game').resolve():
            p.error('--pgo requires --profile-dir and an isolated --output-root')
        args.profile_dir = args.profile_dir.resolve()
        if args.pgo == 'generate':
            args.profile_dir.mkdir(parents=True, exist_ok=True)
        else:
            profiles = list(args.profile_dir.rglob('*.gcda'))
            if not profiles:
                p.error('--pgo=use requires completed .gcda training data')
            profile_stamp = max(x.stat().st_mtime for x in profiles)
    target = args.target
    cc = 'aarch64-linux-gnu-gcc' if target == 'aarch64' else 'cc'
    ar = ('aarch64-linux-gnu-gcc-ar' if target == 'aarch64' else 'gcc-ar') if args.lto else ('aarch64-linux-gnu-ar' if target == 'aarch64' else 'ar')
    output_root = args.output_root.resolve()
    out = output_root/target
    out.mkdir(parents=True, exist_ok=True)
    lock = (out/'.build.lock').open('w')
    fcntl.flock(lock, fcntl.LOCK_EX)
    reports = json.loads((ROOT/'docs/evidence/translation-summary.json').read_text())
    if any(m['unsupported'] or m['lift_exceptions'] for m in reports) and not args.allow_traps:
        sys.exit('Translation failures present. --allow-traps permits diagnostic build with fatal traps.')
    runtime_dir = (ROOT/'build/render-thread-src/runtime') if args.render_thread else ROOT/'runtime'
    include = [runtime_dir, ROOT/'runtime', ROOT/'probe', ROOT/'third_party/pcrecomp/runtime/recomp32',
               ROOT/'third_party/pcrecomp/runtime/win32hle']
    if args.render_thread:
        include[:0] = [ROOT/'probe/rt', ROOT/'build/render-thread-stage']
    flags = ['-std=gnu11', '-O1', '-g', '-fno-strict-aliasing', '-fwrapv',
             '-Werror=implicit-function-declaration', '-DDS_HEAP_START=0x40000000u',
             '-DRECOMP_FP_STACK_EXACT=1']
    link = ['-lm', '-lpthread', '-ldl', '-lrt']
    if target == 'host':
        flags += subprocess.check_output(['pkg-config', '--cflags', 'sdl2', 'glesv2'], text=True).split()
        include += [ROOT/'build/host-sdk/usr/include/SDL2', ROOT/'build/host-sdk/usr/include']
        lib = ROOT/'build/host-sdk/usr/lib/x86_64-linux-gnu'
        link += [f'-L{lib}', f'-Wl,-rpath,{lib}', '-lSDL2_ttf']
        link += subprocess.check_output(['pkg-config', '--libs', 'sdl2', 'glesv2'], text=True).split()
    else:
        include += [ROOT/'build/sdk-include', ROOT/'build/sdk-include/SDL2', Path('/usr/include/SDL2')]
        flags += ['-mcpu=cortex-a53']
        link += [f'-L{ROOT}/build/mali-link', f'-L{ROOT}/build/target-libs', '-Wl,--allow-shlib-undefined', '-lSDL2', '-lSDL2_ttf', '-lGLESv2']
    flags += [f'-I{x}' for x in include]
    # A fixed guest mapping (runtime/win32hle/guest_mem.c) lets the lifter fold
    # the guest base into every memory access as a constant instead of reloading
    # g_mem_base after stores. It must be visible to the native runtime too, so
    # it goes into the shared flags rather than the translated-only list.
    if args.fixed_guest_base not in ('', '0', '0x0'):
        flags.append(f'-DRECOMP_MEM_BASE={args.fixed_guest_base}ULL')
    if args.mem_model == 'nonvolatile':
        flags.append('-DRECOMP_MEM_NONVOLATILE')
    # Generated guest code may use a different optimisation level and the
    # RECOMP_LOCAL_REGS register model. Both change the emitted object code,
    # never the guest semantics; they are part of the compile signature so a
    # flag change forces a full rebuild rather than mixing stale objects.
    translated_flags = [args.translated_opt if f == '-O1' else f for f in flags]
    if args.local_regs:
        translated_flags.append('-DRECOMP_LOCAL_REGS')
    if args.icall_trace:
        translated_flags.append('-DRECOMP_ICALL_TRACE')
    if args.lto:
        translated_flags.append('-flto=auto')
        link.append('-flto=auto')
    if args.pgo == 'generate':
        translated_flags += [f'-fprofile-generate={args.profile_dir}', '-fprofile-update=atomic']
        link += [f'-fprofile-generate={args.profile_dir}', '-Wl,--wrap=SDL_GL_SwapWindow']
    elif args.pgo == 'use':
        translated_flags += [f'-fprofile-use={args.profile_dir}', '-fprofile-partial-training',
                             '-Werror=coverage-mismatch', '-Werror=missing-profile']
    # Headers are a cache dependency; include transitive headers from shared runtime.
    header_stamp = max(p.stat().st_mtime for d in include if d.is_dir() for p in d.rglob('*.h'))
    tasks, archives, host_objects = [], [], []
    source_root = ROOT/'build/translated-local-regs' if args.local_regs else ROOT/'build/translated-x87'
    if args.translated_source_root:
        source_root = args.translated_source_root.resolve()
    for report in reports:
        module = Path(report['module']).stem.lower()
        srcdir = source_root/module
        if not srcdir.is_dir():
            sys.exit(f'Missing translated module source directory: {srcdir}')
        objdir = out/module
        objdir.mkdir(exist_ok=True)
        objects = []
        for src in sorted(srcdir.glob('*.c')):
            obj = objdir/(src.stem+'.o')
            objects.append(obj)
            tasks.append((src, obj, translated_flags))
        archives.append((objdir/'lifted.a', objects))
    sources = [p for p in (ROOT/'third_party/pcrecomp/runtime/win32hle').glob('*.c') if not p.name.endswith('_selftest.c')]
    sources += [p for p in runtime_dir.glob('*.c') if not p.name.endswith('_selftest.c')]
    sources += [p for p in (ROOT/'probe').glob('*.c') if not p.name.endswith('_selftest.c')]
    sources += [ROOT/'build/game/main_dispatch.c', ROOT/'build/game/modules.c']
    sources += list((ROOT/'build/game').glob('recovered*.c'))
    for src in sorted(sources):
        name = ('hle_' if 'win32hle' in str(src) else 'native_')+src.stem
        obj = out/(name+'.o')
        host_objects.append(obj)
        local_flags = flags + (['-O2'] if src.stem in ['d3d8', 'd3d8_renderer', 'fp_stack_exact'] else [])
        tasks.append((src, obj, local_flags))
    if args.render_thread:
        proxy_src = ROOT/'probe/rt/render_thread_proxy.c'
        proxy_obj = out/'rt_render_proxy.o'
        host_objects.append(proxy_obj)
        tasks.append((proxy_src, proxy_obj, flags + ['-O2']))
    if args.pgo == 'generate':
        flush_obj = out/'pgo_flush.o'
        host_objects.append(flush_obj)
        tasks.append((ROOT/'probe/pgo/flush.c', flush_obj, flags + ['-O2']))

    def compile_one(task):
        src, obj, cflags = task
        cmd = [cc, *cflags, '-c', src, '-o', obj]
        signature = hashlib.sha256('\0'.join(map(str, cmd)).encode()).hexdigest()
        stamp = obj.with_suffix('.flags')
        required_header_stamp = header_stamp
        if src.is_relative_to(source_root):
            headers = list((ROOT/'third_party/pcrecomp/runtime/recomp32').glob('*.h'))
            headers += [ROOT/'runtime/cpu_features.h', ROOT/'runtime/fp_stack_exact.h', src.parent/'functions.h']
            with src.open() as input:
                prefix=input.read(2048)
            for name in re.findall(r'^#include "([^"]+)"',prefix,re.M):
                for directory in [src.parent,*include]:
                    if (directory/name).is_file():
                        headers.append(directory/name)
                        break
            required_header_stamp = max(profile_stamp, max(h.stat().st_mtime for h in headers if h.exists()))
        elif src.stem=='movie_decoder':
            required_header_stamp=max(required_header_stamp,(ROOT/'third_party/pl_mpeg/pl_mpeg.h').stat().st_mtime)
        if obj.exists() and stamp.exists() and stamp.read_text() == signature and obj.stat().st_mtime > max(src.stat().st_mtime, required_header_stamp):
            return
        run(cmd, obj.with_suffix('.log'))
        stamp.write_text(signature)

    failures = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = [pool.submit(compile_one, t) for t in tasks]
        for n, f in enumerate(concurrent.futures.as_completed(futures), 1):
            try:
                f.result()
            except Exception as e:
                failures.append(str(e))
                print(e, file=sys.stderr, flush=True)
            if n % 20 == 0:
                print(f'{n}/{len(tasks)} units checked', flush=True)
    if failures:
        sys.exit(f'{len(failures)} compilation failures; no executable linked')
    for archive, objects in archives:
        temporary=archive.with_name(f'.lifted-{os.getpid()}.a')
        if temporary.exists():
            temporary.unlink()
        run([ar, 'rcs', temporary, *objects], archive.with_suffix('.log'))
        temporary.replace(archive)
    binary = output_root/f'thething.{target}'
    temporary_binary=binary.with_name(f'.thething-{target}-{os.getpid()}')
    wrap_flags = []
    if args.render_thread:
        wrap_flags = (ROOT/'build/render-thread-stage/wrap-flags.txt').read_text().split()
    extra_objects = [x.resolve(strict=True) for x in args.extra_object]
    cmd = [cc, '-g', '-rdynamic', '-o', temporary_binary, *host_objects, *extra_objects, '-Wl,--start-group',
           *[x[0] for x in archives], '-Wl,--end-group', *link, *wrap_flags]
    run(cmd, out/'link.log')
    temporary_binary.replace(binary)
    digest = hashlib.sha256(binary.read_bytes()).hexdigest()
    binary.with_suffix(binary.suffix+'.sha256').write_text(f'{digest}  {binary.name}\n')
    readelf = 'aarch64-linux-gnu-readelf' if target == 'aarch64' else 'readelf'
    run([readelf, '-h', binary], out/'elf-header.txt')
    run([readelf, '-d', binary], out/'dynamic.txt')
    print(f'Linked {binary}; SHA-256 {digest}', flush=True)


if __name__ == '__main__':
    main()
