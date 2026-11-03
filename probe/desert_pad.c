/* Derived from ConflictDesertStormPatch, Copyright (c) 2026 eSKylezZ (MIT).
 * Retains its PS2 button numbering, +/-128 axes, deadzone rescaling, and
 * neutral POV sentinels. Replaces XInput/Win32 HID reads with SDL2.
 * No game-memory writes or x86 detours are installed by this adapter. */
#include "desert_pad.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

_Static_assert(sizeof(DS_PadState) == 176, "guest joystick prefix ABI");
_Static_assert(offsetof(DS_PadState, buttons) == 48, "guest button offset");
enum { TRIGGER_LEFT = 100, TRIGGER_RIGHT = 101 };

void ds_pad_defaults(DS_PadConfig *c) {
    *c = (DS_PadConfig){.deadzone = 0.22f, .trigger_threshold = 0.15f,
        .button = {SDL_CONTROLLER_BUTTON_Y, SDL_CONTROLLER_BUTTON_B,
                   SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_X,
                   TRIGGER_LEFT, TRIGGER_RIGHT,
                   SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
                   SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_LEFTSTICK,
                   SDL_CONTROLLER_BUTTON_RIGHTSTICK, SDL_CONTROLLER_BUTTON_START,
                   SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
                   SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_CONTROLLER_BUTTON_DPAD_LEFT},
        .axis = {SDL_CONTROLLER_AXIS_LEFTX, SDL_CONTROLLER_AXIS_LEFTY,
                 SDL_CONTROLLER_AXIS_RIGHTX, -1, -1, SDL_CONTROLLER_AXIS_RIGHTY,
                 SDL_CONTROLLER_AXIS_TRIGGERLEFT, SDL_CONTROLLER_AXIS_TRIGGERRIGHT},
        .key = {{SDL_SCANCODE_I,0},{SDL_SCANCODE_SPACE,0},{SDL_SCANCODE_E,SDL_SCANCODE_RETURN},{SDL_SCANCODE_R,0},
                {-2,0},{-1,0},{SDL_SCANCODE_Q,0},{SDL_SCANCODE_F,0},
                {SDL_SCANCODE_RETURN,0},{SDL_SCANCODE_LCTRL,0},{-5,0},{SDL_SCANCODE_ESCAPE,0},
                {SDL_SCANCODE_UP,-3},{SDL_SCANCODE_RIGHT,0},{SDL_SCANCODE_DOWN,-4},{SDL_SCANCODE_LEFT,0}}};
}

static int key_binding(const char *name){
    if(!strcmp(name,"none"))return 0;
    if(!strcmp(name,"mouseleft"))return -1;if(!strcmp(name,"mouseright"))return -2;
    if(!strcmp(name,"wheelup"))return -3;if(!strcmp(name,"wheeldown"))return -4;
    if(!strcmp(name,"mousemiddle"))return -5;
    if(!strcmp(name,"lctrl"))return SDL_SCANCODE_LCTRL;if(!strcmp(name,"lshift"))return SDL_SCANCODE_LSHIFT;
    if(!strcmp(name,"lalt"))return SDL_SCANCODE_LALT;
    int s=SDL_GetScancodeFromName(name);return s?s:-100;
}

int ds_pad_load(DS_PadConfig *c, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    DS_PadConfig candidate = *c;
    char line[160], name[80]; int index, ok = 1;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        float v;
        if (sscanf(line, "deadzone=%f", &v) == 1) {
            if (!isfinite(v) || v < 0 || v >= 1) { ok = 0; break; }
            candidate.deadzone = v; continue;
        }
        if (sscanf(line, "trigger_threshold=%f", &v) == 1) {
            if (!isfinite(v) || v < 0 || v >= 1) { ok = 0; break; }
            candidate.trigger_threshold = v; continue;
        }
        if (sscanf(line, "key%d=%79s", &index, name) == 2) {
            if(index<0||index>=16){ok=0;break;}
            char *second=strchr(name,',');if(second)*second++=0;
            int a=key_binding(name),b=second?key_binding(second):0;
            if(a==-100||b==-100){ok=0;break;}
            candidate.key[index][0]=a;candidate.key[index][1]=b;continue;
        }
        if (sscanf(line, "axis%d=%79s", &index, name) == 2) {
            int axis = !strcmp(name, "none") ? -1 : (int)SDL_GameControllerGetAxisFromString(name);
            if(index < 0 || index >= 8 || (axis == SDL_CONTROLLER_AXIS_INVALID && strcmp(name,"none"))) {ok=0;break;}
            candidate.axis[index] = axis; continue;
        }
        if (sscanf(line, "%d=%79s", &index, name) != 2 || index < 0 || index >= 16) { ok = 0; break; }
        int button = !strcmp(name, "lefttrigger") ? TRIGGER_LEFT :
                     !strcmp(name, "righttrigger") ? TRIGGER_RIGHT :
                     (int)SDL_GameControllerGetButtonFromString(name);
        if (button == SDL_CONTROLLER_BUTTON_INVALID) { ok = 0; break; }
        candidate.button[index] = button;
    }
    if (ferror(f)) ok = 0;
    fclose(f);
    if (!ok) return -1;
    *c = candidate;
    return 0;
}

static int32_t scale(Sint16 value, float dz) {
    float v = value / 32767.0f, mag = fabsf(v);
    if (mag <= dz) return 0;
    v = (v < 0 ? -1 : 1) * fminf(1, (mag - dz) / (1 - dz));
    return (int32_t)(v * 128);
}

void ds_pad_encode(const DS_PadRaw *raw, const DS_PadConfig *c, DS_PadState *out) {
    memset(out, 0, sizeof(*out));
    for (int i = 0; i < 4; ++i) out->pov[i] = UINT32_MAX;
    if (!raw) return;
    int32_t axes[8]={0};
    for(int i=0;i<8;i++){
        int axis=c->axis[i];
        if(axis<0 || axis>=SDL_CONTROLLER_AXIS_MAX)continue;
        if(axis==SDL_CONTROLLER_AXIS_TRIGGERLEFT || axis==SDL_CONTROLLER_AXIS_TRIGGERRIGHT){
            int v=raw->axes[axis];if(v<0)v=0;
            axes[i]=(int32_t)(v*256.0f/32767.0f)-128;
        }else axes[i]=scale(raw->axes[axis],c->deadzone);
    }
    memcpy(&out->x,axes,sizeof axes);
    for (int i = 0; i < 16; ++i) {
        int button = c->button[i];
        int down = button == TRIGGER_LEFT ? raw->axes[SDL_CONTROLLER_AXIS_TRIGGERLEFT] > 32767 * c->trigger_threshold :
                   button == TRIGGER_RIGHT ? raw->axes[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] > 32767 * c->trigger_threshold :
                   (raw->buttons & (1u << button)) != 0;
        out->buttons[i] = down ? 0x80 : 0;
    }
}

void ds_pad_sample(SDL_GameController *pad, const DS_PadConfig *c, DS_PadState *out) {
    DS_PadRaw raw = {{0}, 0};
    if (!pad || !SDL_GameControllerGetAttached(pad)) { ds_pad_encode(NULL, c, out); return; }
    for (int a = 0; a < SDL_CONTROLLER_AXIS_MAX; ++a)
        raw.axes[a] = SDL_GameControllerGetAxis(pad, (SDL_GameControllerAxis)a);
    for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; ++b)
        if (SDL_GameControllerGetButton(pad, (SDL_GameControllerButton)b)) raw.buttons |= 1u << b;
    ds_pad_encode(&raw, c, out);
}

int ds_pad_selftest(void) {
    DS_PadConfig c; ds_pad_defaults(&c);
    DS_PadRaw raw = {{0}, 0}; DS_PadState out;
    ds_pad_encode(NULL, &c, &out);
    if (out.x || out.y || out.z || out.rz || out.pov[0] != UINT32_MAX) return 0;
    raw.axes[SDL_CONTROLLER_AXIS_LEFTX] = -32768;
    raw.axes[SDL_CONTROLLER_AXIS_LEFTY] = 32767;
    raw.axes[SDL_CONTROLLER_AXIS_RIGHTX] = 100;
    raw.axes[SDL_CONTROLLER_AXIS_RIGHTY] = 32767;
    raw.axes[SDL_CONTROLLER_AXIS_TRIGGERLEFT] = 32767;
    raw.buttons = (1u << SDL_CONTROLLER_BUTTON_A) | (1u << SDL_CONTROLLER_BUTTON_START) |
                  (1u << SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    ds_pad_encode(&raw, &c, &out);
    if (out.x != -128 || out.y != 128 || out.z != 0 || out.rz != 128 ||
        out.buttons[2] != 0x80 || out.buttons[4] != 0x80 || out.buttons[5] ||
        out.buttons[11] != 0x80 || out.buttons[15] != 0x80 || out.buttons[0]) return 0;
    raw.axes[SDL_CONTROLLER_AXIS_LEFTX] = 16384;
    ds_pad_encode(&raw, &c, &out);
    return out.x >= 45 && out.x <= 46;
}
