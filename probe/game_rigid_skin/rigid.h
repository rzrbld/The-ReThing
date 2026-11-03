#ifndef THING_GAME_RIGID_SKIN_H
#define THING_GAME_RIGID_SKIN_H
#include <stdint.h>
/* Returns count processed from descending indices n-1..1. Vertex0 remains
 * original so live guest register/flags/stack-slot effects are reconstructed.
 * Environment THING_NATIVE_RIGID_SKIN=1 enables; unset/0 is original baseline.
 */
extern int thing_game_rigid_skin_enabled;
uint32_t thing_game_rigid_skin(uint32_t source,uint32_t source_end,uint32_t output,uint32_t output_end,uint32_t palette,uint32_t palette_end,uint32_t n,uint32_t guest_cw,int guest_top);
#endif
