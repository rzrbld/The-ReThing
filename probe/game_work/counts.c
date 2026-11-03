#define _POSIX_C_SOURCE 200809L
#include "counts.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
ThingGameWork thing_game_work;
extern uint32_t SDL_GetTicks(void);
void thing_game_work_frame(void){
 static FILE *f;static uint64_t frame;
 if(!f){const char *root=getenv("THING_ROOT");if(!root)abort();char path[PATH_MAX];if(snprintf(path,sizeof path,"%s/original-game-work.csv",root)>=(int)sizeof path)abort();f=fopen(path,"w");if(!f)abort();setvbuf(f,NULL,_IOLBF,4096);fputs("client_frame,sdl_ticks,color_754460,color_75cef0,color_75cfc3,nonzero_color,spatial_queries,spatial_n_sum,spatial_n_max,spatial_tail,spatial_filter,spatial_index_sum,recursive_effect,server_ticks,entity_pass1,entity_pass2\n",f);}
 ThingGameWork *s=&thing_game_work;
 fprintf(f,"%llu,%u,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",(unsigned long long)frame++,SDL_GetTicks(),(unsigned long long)s->color[0],(unsigned long long)s->color[1],(unsigned long long)s->color[2],(unsigned long long)s->nonzero_color,(unsigned long long)s->spatial_queries,(unsigned long long)s->spatial_n_sum,(unsigned long long)s->spatial_n_max,(unsigned long long)s->spatial_tail,(unsigned long long)s->spatial_filter,(unsigned long long)s->spatial_index_sum,(unsigned long long)s->recursive_effect,(unsigned long long)s->server_ticks,(unsigned long long)s->entity_pass1,(unsigned long long)s->entity_pass2);
 memset(s,0,sizeof *s);
}
