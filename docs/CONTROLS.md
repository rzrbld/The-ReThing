# Controller integration

SDL2's GameController API supplies the native controller. The default bridge
feeds the PC game's keyboard and relative-mouse interfaces, using bindings
observed in the original 1.2 executable's saved preferences. This path has
passed host virtual-controller/guest-ABI tests. Physical H700 input selected
New Game and Normal auto aiming in the first device run. Normal gameplay,
remaining menus and save-name entry are still unverified.

| Controller | Guest input / intended function |
|---|---|
| Left stick | W/S/A/D movement after configurable dead zone |
| Right stick | Relative mouse camera and menu pointer; elapsed-time velocity |
| A | E operate + Enter confirm |
| B | Space crouch |
| X | R reload |
| Y | I inventory |
| Left shoulder | Q NPC/squad manager |
| Right shoulder | F documents/keys |
| Right trigger | Left mouse button / primary action |
| Left trigger | Right mouse button / secondary action |
| Left stick click | Left Ctrl / walk toggle |
| Right stick click | Middle mouse button / quick equipment toggle |
| D-pad | Arrow keys; up/down also generate equipment mouse-wheel edges |
| Start | Escape pause/back |
| Back | Enter confirm |

The original preference evidence is
`docs/evidence/new-v12-game-preferences.reg.txt`. E/R/Space/I/Q/F and movement
are observed default assignments. Their physical controller layout above is
the port's proposed layout; it has not been confirmed in gameplay. Start is
the back/cancel button as well as pause. Mouse-trigger clicks and right-stick
pointer motion provide a path for mouse-operated menus. A complete keyboard
independence claim requires testing the actual menus and save-name entry.

## Configuration

Set `THING_CONTROLLER_DB` to an SDL controller mapping database path, including
the handheld firmware mapping if needed. Set `THING_PAD_CONFIG` to a controller
configuration file; the compatibility name `DS_PAD_CONFIG` is also accepted.
An invalid file leaves the defaults unchanged and logs the rejection.

```ini
deadzone=0.22
trigger_threshold=0.15
# Mouse/wheel names, none or SDL scancode names are accepted for key bindings.
key2=e,return
key3=r
key11=escape
# DirectInput slots: x,y,z,rx,ry,rz,slider0,slider1.
axis0=leftx
axis1=lefty
axis2=rightx
axis3=none
axis4=none
axis5=righty
axis6=lefttrigger
axis7=righttrigger
```

The inherited guest button numbering is: Y=0, B=1, A=2, X=3, LT=4, RT=5,
LB=6, RB=7, Back=8, L3=9, R3=10, Start=11, D-pad Up/Right/Down/Left=12/13/14/15.
Native button assignments can also be changed with lines such as `2=a` and
`4=lefttrigger`. Keyboard action bindings are indexed after that mapping.
Mouse bindings are `mouseleft`, `mouseright`, `mousemiddle`, `wheelup`, and
`wheeldown`. `lctrl`, `lshift`, and `lalt` are accepted modifier names.

`THING_CAMERA_SPEED` sets right-stick velocity in mouse units/second (default
650, accepted range 1–10000). Movement currently uses digital W/S/A/D in the
keyboard path; right-stick velocity retains analog magnitude. Native DirectInput
joystick mode (`THING_PAD_MODE=native`) exposes analog axes and trigger sliders
instead, but the game's own joystick data format and assignments still need
validation. The default keyboard mode hides the synthetic joystick to avoid
simultaneously feeding the game both paths.

Disconnect and focus loss release injected keys and mouse buttons. Reconnect
opens the first available SDL GameController again. Device handles are checked
for attachment rather than treating changing device indices as persistent IDs.

## Host checks

Run `bash probe/check_runtime.sh`. This exercises actual SDL virtual controllers
through guest DirectInput COM calls, including configurable axes, analog trigger
sliders, dead zones, press/release state, range conversion, hot-unplug/reconnect,
keyboard/mouse overlay, and focus-loss release. Buffered-input tests verify real
SDL key/button edges and signed mouse deltas, custom format offsets, 16/20-byte
guest event records, peek/count/flush/consume behavior, overflow reporting,
foreground focus loss/reacquisition, and timestamps aligned with GetTickCount.
The opt-in input command stream also checks that taps survive loading pumps
until sampled and remain identical across a peek followed by a consuming read.

For automated bring-up, set `THING_TEST_INPUT=1` and append commands to
`$THING_ROOT/input.commands` after startup:

```text
tap a
button rightshoulder 1
button rightshoulder 0
axis lefty -24000
axis lefty 0
reset
```

This affects sampled input only; it does not write game state. Legacy `DS_*`
diagnostic variables remain accepted. `THING_TRACE_INPUT=1` logs controller
state without reading game-specific addresses.

The actual 1.2 game requests a 256-entry buffer and 20-byte event records for
both keyboard and mouse. The implementation retains up to the documented
size-minus-one count (255 for this request), with a bound of 4,096 retained
events per device. Overflow returns `DI_BUFFEROVERFLOW` and requires the game
to resynchronize current state if transitions were lost.

Force feedback and complete object enumeration remain unsupported and report
failure. No test here executes original gameplay or establishes physical
device compatibility or playable frame rates.
