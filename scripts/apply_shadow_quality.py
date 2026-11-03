#!/usr/bin/env python3
"""Apply only the selected original-game shadow threshold before startup."""
import os
import sys
from pathlib import Path
from configure_graphics import configure

def apply(root, choice=None):
    root = Path(root)
    if choice is None:
        choice = '0'
        cfg = root / 'config/launcher.cfg'
        if cfg.exists():
            for line in cfg.read_text().splitlines():
                if line.startswith('shadow_quality='):
                    choice = line.partition('=')[2]
        choice = os.environ.get('THING_SHADOW_QUALITY', choice)
    if choice not in ('0', '1'):
        raise ValueError('shadow_quality must be 0 (simple) or 1 (original)')
    path = root / 'config/registry.txt'
    old = path.read_text() if path.exists() else ''
    new, _, _ = configure(old, {'CircleShadowLOD': '1' if choice == '0' else '0.2'})
    if new != old:
        path.parent.mkdir(parents=True, exist_ok=True)
        tmp = path.with_suffix('.shadow-tmp')
        tmp.write_text(new)
        tmp.replace(path)
    print('[shadows] ' + ('SIMPLE CircleShadowLOD=1' if choice == '0' else 'ORIGINAL CircleShadowLOD=0.2'))

if __name__ == '__main__':
    apply(sys.argv[1])
