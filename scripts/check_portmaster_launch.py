#!/usr/bin/env python3
"""Check wrapper lifecycle with a fake PortMaster; never touches real services."""
import os, pathlib, shutil, subprocess, tempfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='thing-pm-',dir=ROOT/'build') as tmp:
 base=pathlib.Path(tmp);ports=base/'SD card/ports';game=ports/'the-thing';pm=base/'PortMaster'
 game.mkdir(parents=True);pm.mkdir()
 (pm/'control.txt').write_text('''CFW_NAME=Test
get_controls() { echo controls >> "$PM_TEST_LOG"; sdl_controllerconfig=testmapping; }
pm_platform_helper() { echo "helper:$(basename "$1")" >> "$PM_TEST_LOG"; }
pm_finish() { echo finish >> "$PM_TEST_LOG"; }
''')
 for name in ('The ReThing.sh','run-game.sh'):shutil.copy2(ROOT/'packaging'/name,ports/name)
 shutil.copy2(ROOT/'packaging/run-game.sh',game/'run-game.sh')
 shutil.copy2(ROOT/'packaging/the-thing-env.sh',game/'the-thing-env.sh')
 (game/'muos-session.sh').write_text('thing_muos_suspend() { echo suspend >> "$PM_TEST_LOG"; }; thing_muos_resume() { echo resume >> "$PM_TEST_LOG"; }\n')
 (game/'cpu-session.sh').write_text('thing_cpu_begin() { echo cpu_begin >> "$PM_TEST_LOG"; }; thing_cpu_restore() { echo cpu_restore >> "$PM_TEST_LOG"; }\n')
 (game/'thething.aarch64').write_text('#!/bin/bash\necho "game:$THING_ROOT:$SDL_GAMECONTROLLERCONFIG" >> "$PM_TEST_LOG"\nexit 7\n');(game/'thething.aarch64').chmod(0o755)
 launcher=game/'thething-launcher';launcher.write_text('''#!/bin/bash
if [[ "$1" = --apply-shadows ]]; then exec "$PM_TEST_NATIVE" "$@"; fi
[[ "$THING_ROOT" = "$1" && "$TMPDIR" = "$1/tmp" ]] || exit 90
if [[ "$PM_TEST_MODE" = game ]]; then exec bash "$2"; fi
exit 0
''');launcher.chmod(0o755)
 shortcuts=base/'SD card/Roms/PORTS';shortcuts.mkdir(parents=True)
 for name in ('The ReThing.sh','run-game.sh'):shutil.copy2(ROOT/'packaging'/name,shortcuts/name)
 cases=[(location,mode,script,expected) for location in (ports,shortcuts) for mode,script,expected in [('cancel','The ReThing.sh',0),('game','The ReThing.sh',7),('direct','run-game.sh',7)]]
 for location,mode,script,expected in cases:
  log=base/(location.name+'-'+mode+'.log')
  env={k:v for k,v in os.environ.items() if not k.startswith(('THING_','DS_','PM_'))}
  env.update(controlfolder=str(pm),PM_TEST_LOG=str(log),PM_TEST_MODE=mode,PM_TEST_NATIVE=str(ROOT/'build/launcher/thething-launcher.host'))
  result=subprocess.run(['bash',str(location/script)],env=env,cwd='/',capture_output=True,text=True)
  assert result.returncode==expected,(mode,result.returncode,result.stderr)
  lines=log.read_text().splitlines();assert lines.count('finish')==1,(mode,lines)
  # The menu path leaves frontend handover to PortMaster; only a direct
  # run-game.sh (SSH test harness) suspends the muOS frontend itself.
  want=1 if mode=='direct' else 0
  assert lines.count('suspend')==want and lines.count('resume')==want,(mode,lines)
  if mode!='direct':assert 'helper:thething-launcher' in lines
  if mode!='cancel':assert 'helper:thething.aarch64' in lines and lines.count('cpu_restore')==1,(mode,lines)
  assert not (shortcuts/'the-thing').exists()
  print('PASS',location.name,mode,': correct paths with spaces, lifecycle, one cleanup, exit status',expected)
