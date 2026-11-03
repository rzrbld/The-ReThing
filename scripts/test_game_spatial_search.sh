#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
expected=085d1a90b2bd57967e8e911eb1f7c2232fd4b045b7f363c5c4a3056f720abb49
actual=$(sha256sum "$repo/build/active/bin/thething.exe")
test "${actual%% *}" = "$expected"
tmp=$(mktemp -d /tmp/game-spatial-search.XXXXXX)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -fno-fast-math \
  "$repo/probe/game_spatial/search_index_test.c" -o "$tmp/search-test"
"$tmp/search-test"
