#define _POSIX_C_SOURCE 200809L
#include "counts.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
ThingGameSkinWork thing_game_skin_work;
extern uint32_t SDL_GetTicks(void);
void thing_game_skin_work_frame(void){
 static FILE *f;static uint64_t frame;
 if(!f){const char *root=getenv("THING_ROOT");if(!root)abort();char path[PATH_MAX];if(snprintf(path,sizeof path,"%s/original-game-skin-work.csv",root)>=(int)sizeof path)abort();f=fopen(path,"w");if(!f)abort();setvbuf(f,NULL,_IOLBF,4096);
 fputs("client_frame,sdl_ticks,entry_calls,alternate_entries,palettes,bones_compared,bones_changed,unchanged_palettes,changed_palettes,incomplete_palettes,overflow_palettes,invalid_changed_index,sections_reused,sections_recomputed,weighted_vertices,weighted_affected,weighted_unaffected,weighted_unknown,rigid_vertices,rigid_affected,rigid_unaffected,rigid_unknown,influences,affected_influences,unaffected_influences,unknown_influences,invalid_vertex_index,zero_influence_vertices\n",f);}
 fprintf(f,"%llu,%u",(unsigned long long)frame++,SDL_GetTicks());
#define EMIT(field) fprintf(f,",%llu",(unsigned long long)thing_game_skin_work.field)
 EMIT(entry_calls);EMIT(alternate_entries);EMIT(palettes);EMIT(bones_compared);EMIT(bones_changed);
 EMIT(unchanged_palettes);EMIT(changed_palettes);EMIT(incomplete_palettes);EMIT(overflow_palettes);
 EMIT(invalid_changed_index);EMIT(sections_reused);EMIT(sections_recomputed);
 EMIT(weighted_vertices);EMIT(weighted_affected);EMIT(weighted_unaffected);EMIT(weighted_unknown);
 EMIT(rigid_vertices);EMIT(rigid_affected);EMIT(rigid_unaffected);EMIT(rigid_unknown);
 EMIT(influences);EMIT(affected_influences);EMIT(unaffected_influences);EMIT(unknown_influences);
 EMIT(invalid_vertex_index);EMIT(zero_influence_vertices);
#undef EMIT
 fputc('\n',f);memset(&thing_game_skin_work,0,sizeof thing_game_skin_work);
}
