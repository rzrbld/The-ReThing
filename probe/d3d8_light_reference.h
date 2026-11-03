/* Preserved pre-optimization CPU lighting arithmetic, 2026-10-29.
 * Comparison oracle only; no GL or game behavior. */
static void light_vertex_reference(const D8State *s,const float p[4],const float normal[3],float color[4],float specular[4]){
    float ambient[4],inverse[9],n[3]={normal[0],normal[1],normal[2]};argb(s->rs[139],ambient);
    if(!inverse3(s->transform[256],inverse))for(int j=0;j<3;j++)n[j]=normal[0]*inverse[j*3]+normal[1]*inverse[j*3+1]+normal[2]*inverse[j*3+2];
    if(s->rs[143])normalize(n);
    const float *diffuse=material_source(s->rs[141]?s->rs[145]:0,s->material,color,specular,s->fvf);
    const float *ma=material_source(s->rs[141]?s->rs[147]:0,s->material+4,color,specular,s->fvf);
    const float *ms=material_source(s->rs[141]?s->rs[146]:0,s->material+8,color,specular,s->fvf);
    const float *me=material_source(s->rs[141]?s->rs[148]:0,s->material+12,color,specular,s->fvf);
    float c[4],highlight[4]={0,0,0,specular[3]},eye[3]={-s->transform[2][2],-s->transform[2][6],-s->transform[2][10]};
    if(s->rs[142]&&!inverse3(s->transform[2],inverse))for(int j=0;j<3;j++)eye[j]=-(s->transform[2][12]*inverse[j]+s->transform[2][13]*inverse[3+j]+s->transform[2][14]*inverse[6+j])-p[j];
    normalize(eye);
    for(int k=0;k<3;k++)c[k]=me[k]+ambient[k]*ma[k];c[3]=diffuse[3];
    for(int i=0;i<8;i++)if(s->light_on[i]){
        const float *l=s->light[i];uint32_t type;memcpy(&type,l,4);float dir[3],att=1;
        if(type==3){dir[0]=-l[16];dir[1]=-l[17];dir[2]=-l[18];}
        else if(type==1||type==2){dir[0]=l[13]-p[0];dir[1]=l[14]-p[1];dir[2]=l[15]-p[2];float distance=sqrtf(dir[0]*dir[0]+dir[1]*dir[1]+dir[2]*dir[2]);if(distance>l[19])continue;att=1/fmaxf(.000001f,l[21]+l[22]*distance+l[23]*distance*distance);}
        else continue;
        normalize(dir);
        if(type==2){float spot[3]={l[16],l[17],l[18]};normalize(spot);float cosine=-(dir[0]*spot[0]+dir[1]*spot[1]+dir[2]*spot[2]),outer=cosf(l[25]*.5f),inner=cosf(l[24]*.5f);if(cosine<=outer)continue;if(cosine<inner)att*=powf((cosine-outer)/fmaxf(1e-8f,inner-outer),l[20]);}
        float dot=fmaxf(0,n[0]*dir[0]+n[1]*dir[1]+n[2]*dir[2]);
        for(int k=0;k<3;k++)c[k]+=att*(l[1+k]*diffuse[k]*dot+l[9+k]*ma[k]);
        if(s->rs[29]&&dot>0){float half[3]={dir[0]+eye[0],dir[1]+eye[1],dir[2]+eye[2]};normalize(half);float power=powf(fmaxf(0,n[0]*half[0]+n[1]*half[1]+n[2]*half[2]),s->material[16]);for(int k=0;k<3;k++)highlight[k]+=att*l[5+k]*ms[k]*power;}
    }
    for(int k=0;k<4;k++){color[k]=fminf(1,fmaxf(0,c[k]));specular[k]=fminf(1,fmaxf(0,highlight[k]));}
}
