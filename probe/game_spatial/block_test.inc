static unsigned char memory[0x40000],saved_memory[0x40000];
struct snapshot { uint32_t regs[13],local[6]; double st[8];int64_t si[8];int top,exceptions;uint16_t cw; };
static void snapshot(struct snapshot *s) {
    memset(s,0,sizeof *s);
    uint32_t r[]={g_eax,g_ecx,g_edx,g_ebx,g_esp,g_esi,g_edi,g_ebp,g_cur_func,g_flag_k,g_flag_a,g_flag_b,g_flag_cf};
    memcpy(s->regs,r,sizeof r); memcpy(s->local,end_state,sizeof end_state);
    memcpy(s->st,g_st,sizeof g_st);memcpy(s->si,g_st_i64,sizeof g_st_i64);
    s->top=g_fp_top;s->cw=g_fpu_cw;s->exceptions=fetestexcept(FE_ALL_EXCEPT);
}
static uint32_t rng=0x70c1a0;
static uint32_t rnd(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static void setup(unsigned n,unsigned trial,float t,int valid) {
    g_eax=0x3000;g_ecx=0x2000;g_esp=0x1000;g_ebp=n;
    g_edx=0x77777777;g_ebx=0x88888888;g_esi=0x99999999;g_edi=0xaaaaaaaa;
    g_cur_func=0x70c1a0;g_flag_a=123;g_flag_b=456;g_flag_k=FK_CMP;g_flag_cf=1;
    for(unsigned j=0;j<8;j++){g_st[j]=j*.125+trial;g_st_i64[j]=(int64_t)trial+j;}
    g_fp_top=trial%8;g_fpu_cw=0x7f|((trial%3==0?0:trial%3==1?2:3)<<8)|((trial%4)<<10);
    memcpy(memory+0x1018,&t,4);MEM32(0x2018)=0;
    for(unsigned j=0;j<n;j++) {
        uint32_t p=0x8000+j*0x34; MEM32(0x3000+j*4)=p;
        float f=trial%4==0?0:(float)((int)(j/3)-(int)(n/6));
        memcpy(memory+p+0x14,&f,4);
        if(!valid && j%11==0) MEM32(p+0x14)=0x7fa12345;
    }
    game_spatial_generation_valid=valid;
    feclearexcept(FE_ALL_EXCEPT);if(trial&1)feraiseexcept(FE_DIVBYZERO);
}
int main(void) {
    g_mem_base=(ptrdiff_t)memory;
    unsigned cases=0;int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
    uint32_t edge_bits[]={0,0x80000000,1,0x80000001,0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,0x7fc12345,0x7fa12345};
    for(unsigned h=0;h<4;h++) {
        fesetround(modes[h]);
        for(unsigned trial=0;trial<12000;trial++) {
            unsigned n=1+rnd()%1024;float t;
            if(trial%3==0){uint32_t b=edge_bits[(trial/3)%10];memcpy(&t,&b,4);}
            else t=(float)((int)(rnd()%2048)-1024)*.125f;
            int valid=trial%5!=0;
            struct snapshot a,b;
            setup(n,trial,t,valid);original();snapshot(&a);memcpy(saved_memory,memory,sizeof memory);
            setup(n,trial,t,valid);candidate();snapshot(&b);
            if(memcmp(&a,&b,sizeof a)||memcmp(memory,saved_memory,sizeof memory)) {
                printf("FAIL mode=%u trial=%u n=%u valid=%d state=%d memory=%d FE=%x/%x\n",h,trial,n,valid,memcmp(&a,&b,sizeof a),memcmp(memory,saved_memory,sizeof memory),a.exceptions,b.exceptions);return 1;
            }
            cases++;
        }
    }
    printf("PASS %u extracted-block cases: all GPRs, local/global lazy flags, fpu comparison, all8 FP/shadow slots, top/CW, entire guest memory, host exception flags.\n",cases);
    puts("Finite sorted validated state only accelerated; unvalidated arrays and nonfinite threshold retain original path. No live lifecycle cache enabled.");
}
