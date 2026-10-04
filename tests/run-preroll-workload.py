#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Compile identical workload against a configured worktree's matching headers/lib.
Usage: python3 tests/run-preroll-workload.py /path/to/worktree /tmp/workload-name
The worktree must have built openshot-VideoCacheThread-test in build/.
"""
import pathlib
import shlex
import subprocess
import sys

worktree = pathlib.Path(sys.argv[1]).resolve()
output = pathlib.Path(sys.argv[2]).resolve()
unit = worktree / 'build/tests/CMakeFiles/openshot-VideoCacheThread-test.dir'
flags = {}
for line in (unit / 'flags.make').read_text().splitlines():
    if line.startswith('CXX_') and ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
link = shlex.split((unit / 'link.txt').read_text())
# Preserve the configured matching dependency flags; replace only test objects.
args = [link[0]] + flags['CXX_FLAGS'] + flags['CXX_DEFINES'] + flags['CXX_INCLUDES']
args += [str(pathlib.Path(__file__).with_name('PrerollWorkload.cpp').resolve())]
args += ['-o', str(output)]
skip = False
for item in link[1:]:
    if skip:
        skip = False
        continue
    if item == '-o':
        skip = True
        continue
    if item.endswith('.o') or item in flags['CXX_FLAGS']:
        continue
    args.append(item)
print(shlex.join(args), flush=True)
subprocess.run(args, cwd=worktree / 'build/tests', check=True)
subprocess.run([str(output)], check=True)
