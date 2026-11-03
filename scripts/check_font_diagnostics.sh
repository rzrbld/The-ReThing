#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/font-diagnostic-check
cc -std=gnu11 -O1 -g -Wall -Wextra -Werror -Iruntime \
 -Ithird_party/pcrecomp/runtime/recomp32 -Ithird_party/pcrecomp/runtime/win32hle \
 runtime/font_diagnostic.c runtime/font_diagnostic_selftest.c -o build/font-diagnostic-check/check
build/font-diagnostic-check/check off
build/font-diagnostic-check/check on
python3 - <<'PY'
import importlib.util
spec=importlib.util.spec_from_file_location('font_patch','scripts/patch_font_diagnostics.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
s=m.MISSING+m.CALL+'#include "functions.h"\n';p=m.patch(s);assert m.patch(p)==p
for bad in ('',s+s):
 try:m.patch(bad)
 except ValueError:pass
 else:raise AssertionError('Changed site counts not rejected')
print('PASS font patch idempotence and missing/duplicate source-site rejection')
PY
python3 scripts/patch_font_diagnostics.py
