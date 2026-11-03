"""Read-only host scheduling/frequency observations, not cycle measurements."""
from pathlib import Path
FIELDS=('elapsed_seconds','processor','allowed_cpus','governor','scaling_cur_khz','user_ticks','system_ticks')
def parse_stat(text):
    # comm may contain spaces and parentheses. Fields after its final ')' begin
    # with field3 (state); processor is field39, utime/stime fields14/15.
    fields=text[text.rindex(')')+1:].split()
    return int(fields[36]),int(fields[11]),int(fields[12])
def sample(pid,elapsed):
    proc=Path('/proc')/str(pid)
    cpu,user,system=parse_stat((proc/'stat').read_text())
    allowed=next((s.split(':',1)[1].strip() for s in (proc/'status').read_text().splitlines() if s.startswith('Cpus_allowed_list:')),'')
    cpufreq=Path('/sys/devices/system/cpu')/f'cpu{cpu}'/'cpufreq'
    def optional(name):
        try:return (cpufreq/name).read_text().strip()
        except OSError:return ''
    return dict(zip(FIELDS,(round(elapsed,3),cpu,allowed,optional('scaling_governor'),optional('scaling_cur_freq'),user,system)))
