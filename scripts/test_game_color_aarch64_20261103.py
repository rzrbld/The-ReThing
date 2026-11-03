#!/usr/bin/env python3
"""Isolated, offline static AArch64 test; requires existing Docker socket access."""
import hashlib,json,os,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1];probe=r/'probe/game_color';e=r/'docs/evidence/original-queue-audit-20261103';image='sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49'
subprocess.run(['docker','run','--rm','--network','none','--read-only','--tmpfs','/tmp','--user',f'{os.getuid()}:{os.getgid()}','-v',f'{r}:{r}:ro','-v',f'{probe}:{probe}:rw','--workdir',str(r),image,'aarch64-linux-gnu-gcc','-O2','-static','-mcpu=cortex-a53','-frounding-math','-fno-strict-aliasing','-o','probe/game_color/block_test.aarch64','probe/game_color/block_test.c','-lm'],check=True)
p=subprocess.run([str(r/'build/tools/qemu-user/usr/bin/qemu-aarch64'),str(probe/'block_test.aarch64')],capture_output=True,text=True,check=True)
(e/'color-full-block-aarch64-qemu.txt').write_text('STATIC AArch64 cortex-a53 under QEMU; synthetic timing is not hardware timing.\n'+p.stdout)
print(p.stdout)
