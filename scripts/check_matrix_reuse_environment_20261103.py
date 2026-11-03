#!/usr/bin/env python3
"""Target repeated-input reuse across native FP control changes, separate evidence."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1];base=root/'build/matrix-last-result-20261103';out=root/'build/matrix-reuse-environment-20261103';out.mkdir(exist_ok=True)
for name in ('reference.c','candidate.c'):(out/name).write_bytes((base/name).read_bytes())
s=(base/'driver.c').read_text()
helper=r'''
static void alter_native_control(void){
#if defined(__aarch64__)
 uint64_t c;__asm__ volatile("mrs %0, fpcr":"=r"(c)::"memory");
 c^=(1ull<<24)|(1ull<<25);__asm__ volatile("msr fpcr, %0"::"r"(c):"memory");
#elif defined(__x86_64__)
 uint32_t c;__asm__ volatile("stmxcsr %0":"=m"(c)::"memory");
 c^=0x8040u;__asm__ volatile("ldmxcsr %0"::"m"(c):"memory");
#endif
}
'''
s=s.replace('int main(int argc,char **argv){',helper+'\nint main(int argc,char **argv){')
s=s.replace('t<50000','t<2000').replace('repeat<2','repeat<4')
s=s.replace('}fegetenv(&startenv);','}if(repeat==0)fesetround(modes[m]);if(repeat==2){fesetround(modes[(m+1)%4]);alter_native_control();}fegetenv(&startenv);')
assert 'alter_native_control();}fegetenv' in s
(out/'driver.c').write_text(s)
runner=(root/'scripts/check_matrix_last_result_20261103.py').read_text().replace('matrix-last-result-20261103','matrix-reuse-environment-20261103')
# Runner lives under scripts so root resolution remains correct.
p=root/'scripts/_run_matrix_reuse_environment_20261103.py';p.write_text(runner)
try:subprocess.run(['python3',str(p)],cwd=root,check=True)
finally:p.unlink()
