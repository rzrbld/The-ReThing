/* Training only: the guest's _exit path bypasses ordinary gcov exit handlers. */
#include <stdio.h>
#include <stdlib.h>
typedef struct SDL_Window SDL_Window;
extern void __real_SDL_GL_SwapWindow(SDL_Window *);
extern void __gcov_dump(void);
static unsigned long frame, dump_at;
static int configured, dumped;
void __wrap_SDL_GL_SwapWindow(SDL_Window *window) {
    __real_SDL_GL_SwapWindow(window);
    if (!configured) {
        const char *value = getenv("THING_PGO_DUMP_AT_FRAME");
        char *end = NULL;
        dump_at = value ? strtoul(value, &end, 10) : 1800;
        if (!dump_at || (value && (*end || *value == '-'))) {
            fputs("[pgo] invalid THING_PGO_DUMP_AT_FRAME\n", stderr);
            abort();
        }
        configured = 1;
    }
    if (++frame >= dump_at && !dumped) {
        __gcov_dump();
        dumped = 1;
        fprintf(stderr, "[pgo] training counters dumped at swap=%lu\n", frame);
    }
}
