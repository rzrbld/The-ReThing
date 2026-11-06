# Render options

Open **RENDER OPTIONS** in the launcher. Use the D-pad or left stick to select
an option, left/right to change it, and B to return. Save and start applies the
settings. The main page keeps the FPS cap and counter controls.

Recommended on H700: **640x480, SINGLE, BUFFER CACHE OFF, SHADOWS SIMPLE**.
Existing saved settings are preserved when upgrading.

| Option | Behavior |
| --- | --- |
| RENDERER | SINGLE is recommended. THREADED is optional and has been slower in device tests. |
| RESOLUTION | 640x480 is recommended. 512x384 scales to the screen and may help GPU-limited scenes; it does not fix CPU bottlenecks. |
| SHADOWS | SIMPLE uses the game's lower shadow setting; ORIGINAL keeps the original setting. |
| BUFFER CACHE | Optional buffer reuse; OFF is recommended from H700 testing. |
| ASYNC PRESENT | Threaded presentation option; blocked in SINGLE. Other synchronization can limit overlap. |
| ONE UPLOAD | Combines transient buffer uploads. Optional, OFF by default. |
| RANGED DRAWS | Supplies index bounds to the driver. ON by default; blocked in THREADED. |
| ALPHA ELISION | Uses a shader without alpha-test discard when the game disables alpha testing. ON by default. |
| DEPTH INVALIDATE | Discards completed-frame depth/stencil data. ON by default; blocked in THREADED. |

Blocked options cannot be changed in the current renderer mode. Their saved
preferences return when switching to a compatible mode.

The renderer uses smaller shader outputs for eligible draws and prebuilds common
shader variants. Compiled shaders are kept in `userdata/shader-cache`: a shader
first needed during play (turning round, looking into another room) is compiled
once, about 0.2 s on H700, and is loaded in under a millisecond at every later
start, so areas already visited do not hitch. Deleting the folder is safe; it
is rebuilt as you play. `THING_SHADER_CACHE=0` disables it. The 512x384 path uses the correct framebuffer attachment enums.
The material-state optimization bypasses only already-satisfied render-state
setup; textures, resources and geometry submission still execute normally.

On H700 at 640x480, SINGLE, cache OFF, two stationary save-slot-4 comparisons
measured approximately **0.55 ms/frame (1.5%)** and **1.11 ms/frame (2.4%)** saved
by material-state reuse. Each comparison held its camera fixed and switched
between enabled and disabled phases. The two runs had different camera views,
so their absolute FPS must not be compared against one another. These are
limited measurements, not a promise of stable 30 FPS or gains in every scene.

The device was warm during those tests and CPU frequency was not logged; the
small measured gains should not be treated as a precise thermal-controlled result.
