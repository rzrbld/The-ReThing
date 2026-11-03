#ifndef THING_D3D8_VIDEO_H
#define THING_D3D8_VIDEO_H
#include <stdint.h>
#include <SDL.h>

/* Main-thread movie output on the already-created game's SDL/GLES window.
 * Input is tightly packed RGBA, top row first. Draws and swaps immediately:
 * ordinary D3D Present need not run while the movie is active.
 */
int thing_video_present_rgba(const uint8_t *rgba,int width,int height);

/* Shared implementation also used by the isolated host pixel/state test.
 * present=0 draws to the backbuffer without swapping for deterministic reads.
 */
int d8_video_draw_rgba(SDL_Window *window,const uint8_t *rgba,int width,int height,int present);
/* Must be called with its owning GL context current, before context teardown. */
void d8_video_close(void);
#endif
