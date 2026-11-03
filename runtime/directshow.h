#ifndef THING_DIRECTSHOW_H
#define THING_DIRECTSHOW_H
/* Graph identity/QI/lifetime and native MPEG playback. Media completion comes
 * only from real decoded EOF plus drained audio, never from a duration timer. */
void thing_directshow_register(void);
#endif
