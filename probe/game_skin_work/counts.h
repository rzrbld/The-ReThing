#ifndef THING_GAME_SKIN_WORK_COUNTS_H
#define THING_GAME_SKIN_WORK_COUNTS_H
#include <stdint.h>
#include <string.h>
#define THING_SKIN_BONE_LIMIT 2048u
typedef struct {
 uint64_t entry_calls,alternate_entries,palettes,bones_compared,bones_changed;
 uint64_t unchanged_palettes,changed_palettes,incomplete_palettes,overflow_palettes;
 uint64_t invalid_changed_index,sections_reused,sections_recomputed;
 uint64_t weighted_vertices,weighted_affected,weighted_unaffected,weighted_unknown;
 uint64_t rigid_vertices,rigid_affected,rigid_unaffected,rigid_unknown;
 uint64_t influences,affected_influences,unaffected_influences,unknown_influences;
 uint64_t invalid_vertex_index,zero_influence_vertices;
} ThingGameSkinWork;
extern ThingGameSkinWork thing_game_skin_work;
void thing_game_skin_work_frame(void);
typedef struct {
 uint64_t changed[THING_SKIN_BONE_LIMIT/64];
 uint32_t base,bones,tested,known,complete,vertex_affected,vertex_unknown,vertex_influences;
} ThingSkinWorkLocal;
static inline void thing_skin_work_begin(ThingSkinWorkLocal *s,uint32_t base,uint32_t end,uint32_t nodes_begin,uint32_t nodes_end){
 thing_game_skin_work.palettes++;s->base=base;s->tested=0;s->complete=0;
 s->known=base&&end>=base&&!( (end-base)&63u)&&nodes_end>=nodes_begin&&!((nodes_end-nodes_begin)&3u);
 s->bones=(end-base)/64u;
 if(s->bones>THING_SKIN_BONE_LIMIT){thing_game_skin_work.overflow_palettes++;s->known=0;}
 if(s->bones!=(nodes_end-nodes_begin)/4u)s->known=0;
 memset(s->changed,0,sizeof s->changed);
}
/* Actual destination byteaddress from7AE910, NOT an assumed node ordinal. */
static inline void thing_skin_work_changed(ThingSkinWorkLocal *s,uint32_t destination){
 thing_game_skin_work.bones_changed++;
 if(!s->known||destination<s->base||((destination-s->base)&63u)||((destination-s->base)/64u)>=s->bones){s->known=0;thing_game_skin_work.invalid_changed_index++;return;}
 unsigned i=(destination-s->base)/64u;s->changed[i>>6]|=UINT64_C(1)<<(i&63u);
}
static inline void thing_skin_work_compared(ThingSkinWorkLocal *s){s->tested++;thing_game_skin_work.bones_compared++;}
static inline void thing_skin_work_palette_done(ThingSkinWorkLocal *s,unsigned unchanged){
 s->complete=s->known&&s->tested==s->bones;
 if(!s->complete)thing_game_skin_work.incomplete_palettes++;
 if(unchanged)thing_game_skin_work.unchanged_palettes++;else thing_game_skin_work.changed_palettes++;
}
static inline unsigned thing_skin_work_dependency(ThingSkinWorkLocal *s,int32_t bone){
 if(!s->complete)return 2;
 if(bone<0||(uint32_t)bone>=s->bones){thing_game_skin_work.invalid_vertex_index++;return 2;}
 return (unsigned)((s->changed[(unsigned)bone>>6]>>((unsigned)bone&63u))&1u);
}
static inline void thing_skin_work_weight_begin(ThingSkinWorkLocal *s){s->vertex_affected=0;s->vertex_unknown=!s->complete;s->vertex_influences=0;}
static inline void thing_skin_work_influence(ThingSkinWorkLocal *s,int32_t bone){
 unsigned d=thing_skin_work_dependency(s,bone);s->vertex_influences++;thing_game_skin_work.influences++;
 if(d==2){s->vertex_unknown=1;thing_game_skin_work.unknown_influences++;}
 else if(d){s->vertex_affected=1;thing_game_skin_work.affected_influences++;}
 else thing_game_skin_work.unaffected_influences++;
}
static inline void thing_skin_work_weight_end(ThingSkinWorkLocal *s){
 thing_game_skin_work.weighted_vertices++;if(!s->vertex_influences)thing_game_skin_work.zero_influence_vertices++;
 if(s->vertex_unknown)thing_game_skin_work.weighted_unknown++;
 else if(s->vertex_affected)thing_game_skin_work.weighted_affected++;
 else thing_game_skin_work.weighted_unaffected++;
}
static inline void thing_skin_work_rigid(ThingSkinWorkLocal *s,int32_t bone){
 unsigned d=thing_skin_work_dependency(s,bone);thing_game_skin_work.rigid_vertices++;
 if(d==2)thing_game_skin_work.rigid_unknown++;
 else if(d)thing_game_skin_work.rigid_affected++;else thing_game_skin_work.rigid_unaffected++;
}
#endif
