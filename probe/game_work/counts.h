#ifndef THING_GAME_WORK_COUNTS_H
#define THING_GAME_WORK_COUNTS_H
#include <stdint.h>
typedef struct {
 uint64_t color[3],nonzero_color,spatial_queries,spatial_n_sum,spatial_n_max;
 uint64_t spatial_tail,spatial_filter,spatial_index_sum,recursive_effect,server_ticks,entity_pass1,entity_pass2;
} ThingGameWork;
extern ThingGameWork thing_game_work;
void thing_game_work_frame(void);
static inline void thing_game_work_color(unsigned entry,uint32_t color){thing_game_work.color[entry]++;thing_game_work.nonzero_color+=!!(color&0xffffffu);}
static inline void thing_game_work_spatial(uint32_t n){thing_game_work.spatial_queries++;thing_game_work.spatial_n_sum+=n;if(n>thing_game_work.spatial_n_max)thing_game_work.spatial_n_max=n;}
#endif
