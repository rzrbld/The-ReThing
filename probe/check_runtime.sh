#!/usr/bin/env bash
# Host-only bridge checks; does not execute translated game code or H700 tests.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/runtime-check
cflags=(-std=gnu11 -O1 -Werror=implicit-function-declaration -Iprobe -Iruntime
  -Ithird_party/pcrecomp/runtime/recomp32 -Ithird_party/pcrecomp/runtime/win32hle)
read -r -a sdl_cflags <<< "$(pkg-config --cflags sdl2)"
read -r -a sdl_libs <<< "$(pkg-config --libs sdl2)"
read -r -a gles_flags <<< "$(pkg-config --cflags --libs glesv2)"
cc "${cflags[@]}" "${sdl_cflags[@]}" probe/pad_config_selftest.c probe/desert_pad.c \
  "${sdl_libs[@]}" -lm -o build/runtime-check/pad-config-selftest
build/runtime-check/pad-config-selftest
cc "${cflags[@]}" "${sdl_cflags[@]}" probe/dinput8_selftest.c runtime/dinput8.c \
  runtime/test_input.c probe/desert_pad.c "${sdl_libs[@]}" -lm -o build/runtime-check/input-selftest
THING_PAD_MODE=native build/runtime-check/input-selftest
THING_PAD_MODE=native build/runtime-check/input-selftest --commands
cc "${cflags[@]}" "${sdl_cflags[@]}" probe/controller_keyboard_selftest.c runtime/dinput8.c \
  runtime/test_input.c probe/desert_pad.c "${sdl_libs[@]}" -lm -o build/runtime-check/controller-keyboard-selftest
build/runtime-check/controller-keyboard-selftest
cc "${cflags[@]}" "${sdl_cflags[@]}" probe/dinput8_buffer_selftest.c runtime/dinput8.c \
  runtime/test_input.c probe/desert_pad.c "${sdl_libs[@]}" -lm -o build/runtime-check/input-buffer-selftest
build/runtime-check/input-buffer-selftest
build/runtime-check/input-buffer-selftest --commands
cc "${cflags[@]}" "${sdl_cflags[@]}" probe/dsound8_selftest.c runtime/dsound8.c \
  third_party/pcrecomp/runtime/win32hle/dsound.c "${sdl_libs[@]}" -lm -o build/runtime-check/sound-selftest
build/runtime-check/sound-selftest
cc "${cflags[@]}" "${sdl_cflags[@]}" probe/dsound_notify_selftest.c runtime/dsound8.c \
  third_party/pcrecomp/runtime/win32hle/dsound.c "${sdl_libs[@]}" -lm -o build/runtime-check/sound-notify-selftest
build/runtime-check/sound-notify-selftest
cc "${cflags[@]}" "${sdl_cflags[@]}" probe/dsound_pool_selftest.c runtime/dsound8.c \
  third_party/pcrecomp/runtime/win32hle/dsound.c "${sdl_libs[@]}" -lm -o build/runtime-check/sound-pool-selftest
build/runtime-check/sound-pool-selftest
cc "${cflags[@]}" "${sdl_cflags[@]}" -Ithird_party/pl_mpeg probe/audio_stream_selftest.c \
  third_party/pcrecomp/runtime/win32hle/dsound.c "${sdl_libs[@]}" -lm -o build/runtime-check/audio-stream-selftest
sample=build/active/data/FMV/UIS_Logo_PC.mpg
[[ -f "$sample" ]] || sample=build/original/data/FMV/UIS_Logo_PC.mpg
build/runtime-check/audio-stream-selftest "$sample"
cc "${cflags[@]}" "${sdl_cflags[@]}" probe/d3d8_raster_selftest.c runtime/d3d8_renderer.c \
  "${sdl_libs[@]}" "${gles_flags[@]}" -lm -o build/runtime-check/renderer-selftest
SDL_VIDEODRIVER=offscreen build/runtime-check/renderer-selftest
cc "${cflags[@]}" "${sdl_cflags[@]}" probe/d3d8_video_selftest.c runtime/d3d8_video.c \
  "${sdl_libs[@]}" "${gles_flags[@]}" -lm -o build/runtime-check/video-selftest
SDL_VIDEODRIVER=offscreen build/runtime-check/video-selftest
