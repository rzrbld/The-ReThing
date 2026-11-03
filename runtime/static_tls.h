#ifndef THING_STATIC_TLS_H
#define THING_STATIC_TLS_H
#include <stdint.h>
/* Called under guest-machine ownership (or before starting guest threads). */
void thing_tls_image_loaded(uint32_t base);
void thing_tls_image_unloaded(uint32_t base);
void thing_tls_thread_attach(void);
#endif
