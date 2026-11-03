#ifndef THING_FONT_DIAGNOSTIC_H
#define THING_FONT_DIAGNOSTIC_H
#include <stdint.h>
/* Read-only diagnostic at two byte-checked branches of font creation768610. */
void thing_font_diagnostic(uint32_t site,uint32_t object,uint32_t requested,uint32_t resolved,uint32_t result);
#endif
