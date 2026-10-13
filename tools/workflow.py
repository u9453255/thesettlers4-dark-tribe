"""Run the complete offline build and verification workflow (HANDOFF.md, "Primary offline workflow").

  python tools/workflow.py

Builds the compatibility XML, the scenario map and the native DLL, stages the release and
runs every offline check. It never launches, inspects or changes the game; installation is
left to Launch Dark Tribe.cmd. Node.js comes from DARK_TRIBE_NODE, local-config.json or PATH.
"""
import os
import subprocess
import sys
from pathlib import Path

from project_paths import ROOT, game_dir, tool

STEPS = ['test-game-build.py', 'inspect-united.py', 'inspect-hd-rendering.py', 'build-compat.py',
         'build-scenario.cjs', 'test-scenario.cjs', 'test-placement.cjs', 'inspect-placement.py',
         'inspect-placement-hints.py', 'inspect-construction.py', 'inspect-interactions.py',
         'inspect-recruitment.py', 'test-scenario.py', 'inspect-startup-sectors.py', 'verify-maps.cjs',
         'build-native.py', 'stage-release.py', 'test-startup.py', 'verify-offline.py', 'load-native.py']


def main():
    node = tool('node')
    if not node:
        sys.exit('Node.js not found: put node on PATH, set DARK_TRIBE_NODE, or run python tools/setup.py --node <node.exe>')
    try:
        game = game_dir()
    except RuntimeError as error:
        sys.exit(str(error))
    env = dict(os.environ, DARK_TRIBE_GAME_DIR=str(game), DARK_TRIBE_PYTHON=sys.executable, DARK_TRIBE_NODE=node)
    env['PATH'] = os.pathsep.join([str(Path(node).parent), str(Path(sys.executable).parent), env.get('PATH', '')])
    for step in STEPS:
        command = [node if step.endswith('.cjs') else sys.executable, str(ROOT / 'tools' / step)]
        result = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True, encoding='utf-8', errors='replace')
        output = (result.stdout + result.stderr).strip().splitlines()
        print(f'[{result.returncode}] {step} :: {output[-1][:140] if output else ""}', flush=True)
        if result.returncode:
            print('\n'.join(output[-20:]))
            sys.exit(f'Stopped: {step} failed.')
    print('All offline steps passed. Gameplay is not verified by these checks; follow TESTING.md in game.')


if __name__ == '__main__':
    main()
