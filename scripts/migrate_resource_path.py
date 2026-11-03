#!/usr/bin/env python3
r"""Correct only the port's old incomplete ResourceDir in a copied registry.

The original installer specifies <TARGETDIR>\data;<TARGETDIR>\data\mainui.pak.
Custom resource paths and all unrelated preferences are retained.
"""
import argparse
import json
from pathlib import Path


def migrate(source):
    output, changed = [], 0
    suffix = r'\software\computer artworks\the thing\1.0\resourcedir'
    for line in source.splitlines(keepends=True):
        content = line.rstrip('\r\n')
        key, separator, value = content.partition('=sz:')
        root = key.split('\\', 1)[0].casefold()
        if (separator and root in ('hkey_current_user', 'hkey_local_machine')
                and key.casefold().endswith(suffix)
                and value.replace('/', '\\').rstrip('\\').casefold() == r'c:\data'):
            output.append(key + r'=sz:C:\data;C:\data\mainui.pak' + line[len(content):])
            changed += 1
        else:
            output.append(line)
    return ''.join(output), changed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.input.resolve() == args.output.resolve():
        parser.error('Use a separate output file; keep the original for rollback')
    with args.input.open(newline='') as handle:
        output, changed = migrate(handle.read())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('w', newline='') as handle:
        handle.write(output)
    print(json.dumps({'input': str(args.input), 'output': str(args.output),
                      'corrected_values': changed,
                      'resource_path': r'C:\data;C:\data\mainui.pak',
                      'scope': 'Exact old port default only; custom paths and graphics unchanged'}))


if __name__ == '__main__':
    main()
