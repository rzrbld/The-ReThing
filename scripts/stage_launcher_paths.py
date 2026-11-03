#!/usr/bin/env python3
"""Embed the same bootstrap resolver in shortcuts: no adjacent helper required."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
block='''# BEGIN PORT ROOT RESOLUTION
launcher_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
if [[ -n ${THING_ROOT:-} ]]; then
    gamedir=$THING_ROOT
    if [[ ! -f "$gamedir/the-thing-env.sh" ]]; then
        printf 'The Thing: invalid THING_ROOT: %s\\n' "$gamedir" >&2
        exit 1
    fi
else
    gamedir=
    # muOS shortcuts can live in Roms/PORTS while payload lives in card/ports.
    # Test the payload marker, not bin/: first-run installation has no bin/ yet.
    for candidate in "$launcher_dir" "$launcher_dir/the-thing" \\
        "$launcher_dir/../../ports/the-thing" \\
        "${directory:+/${directory#/}/ports/the-thing}" \\
        /mnt/sdcard/ports/the-thing /mnt/mmc/ports/the-thing \\
        /roms/ports/the-thing /roms2/ports/the-thing; do
        [[ -n "$candidate" && -f "$candidate/the-thing-env.sh" ]] || continue
        gamedir=$candidate
        break
    done
    if [[ -z "$gamedir" ]]; then
        printf 'The Thing: cannot find ports/the-thing/the-thing-env.sh. Install the payload in the card ports folder or set THING_ROOT.\\n' >&2
        exit 1
    fi
fi
gamedir=$(cd -- "$gamedir" && pwd) || exit 1
export THING_ROOT="$gamedir"
# END PORT ROOT RESOLUTION
'''
for name in ('The ReThing.sh','The Thing.sh','The Thing Measure.sh'):
 p=root/'packaging'/name;s=p.read_text()
 if '# BEGIN PORT ROOT RESOLUTION' in s:
  a=s.index('# BEGIN PORT ROOT RESOLUTION');b=s.index('# END PORT ROOT RESOLUTION')+len('# END PORT ROOT RESOLUTION\n')
 else:
  a=s.index('launcher_dir=');b=s.index('export THING_ROOT="$gamedir"')+len('export THING_ROOT="$gamedir"\n')
 s=s[:a]+block+s[b:]
 if name=='The Thing Measure.sh' and 'Install the game through The ReThing first.' not in s:s=s.replace('# END PORT ROOT RESOLUTION\n','# END PORT ROOT RESOLUTION\n[[ -d "$gamedir/bin" ]] || { echo "Install the game through The ReThing first." >&2; exit 1; }\n')
 p.write_text(s)
