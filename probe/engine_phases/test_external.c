#include "timing.h"
/* Exercise a second translated unit using the one shared collector. */
void test_server_phase(void){
 uint64_t pump=ep_begin(17,0x6a7450);
 for(unsigned i=0;i<3;i++){
  uint64_t tick=ep_begin(18,0x6a0460);
  uint64_t entities=ep_begin(19,0x6a0850);
  ep_end(19,entities);ep_end(18,tick);
 }
 ep_end(17,pump);
}
