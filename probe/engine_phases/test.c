#define _POSIX_C_SOURCE 200809L
#define THING_ENGINE_PHASE_IMPLEMENTATION
#include "timing.h"
extern void test_server_phase(void);
int main(void){test_server_phase();uint64_t a=ep_begin(0,0x100);ep_end(0,a);thing_engine_phase_present(1);a=ep_begin(1,0x200);thing_engine_phase_present(2);ep_end(1,a);thing_engine_phase_present(3);a=ep_begin(2,0x300);ep_end(2,a);a=ep_begin(2,0x400);ep_end(2,a);thing_engine_phase_present(4);thing_engine_phase_finish();thing_engine_phase_finish();return 0;}
