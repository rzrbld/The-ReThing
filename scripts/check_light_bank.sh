#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/light-bank-check
cc -std=c11 -O2 -Wall -Wextra -Iruntime runtime/d3d8_lights.c runtime/d3d8_light_bank_selftest.c -o build/light-bank-check/check
build/light-bank-check/check
