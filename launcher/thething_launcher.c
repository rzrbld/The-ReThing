/* The Thing settings launcher: retro blue menu, controller/keyboard driven.
 * Writes config/launcher.cfg and execs the Ports launcher with settings as env. */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

extern int thing_native_setup(const char *root, int shadow_only);

#define W 640
#define H 480
#define KEY_COUNT 2            /* page-0 setting rows before RENDER OPTIONS */
#define OPTIONS_ITEM KEY_COUNT /* page-0 "RENDER OPTIONS" row */
#define MAX_ITEMS 12
#define PAGE0_SAVE (KEY_COUNT+1)
#define PAGE0_CANCEL (KEY_COUNT+2)
#define PAGE0_COUNT (KEY_COUNT+3)
#define PAGE1_ITEMS 9          /* All rendering settings */
#define PAGE1_BACK PAGE1_ITEMS
#define PAGE1_COUNT (PAGE1_ITEMS+1)
#define FONT_W 5
#define FONT_H 7

typedef struct { const char *label; const char **values; int count, index; const char *env; const char *cfg; } Item;

static const char *fps_values[]={"UNCAPPED","60 FPS","30 FPS"};
static const char *counter_values[]={"OFF","ON"};
static const char *res_values[]={"NATIVE","640X480","512X384 (FAST)"};
static const char *shadow_values[]={"SIMPLE","ORIGINAL"};
static const char *thread_values[]={"THREADED","SINGLE"};
static const char *nav_values[]={"OPEN"};

static Item items[MAX_ITEMS]={
    {"FPS CAP",fps_values,3,0,"THING_FPS_LIMIT","fps_limit"},
    {"FPS COUNTER",counter_values,2,0,"THING_FPS_OVERLAY","fps_counter"},
    {"RESOLUTION",res_values,3,1,"","resolution"},
    {"RENDERER",thread_values,2,1,"DS_RENDER_THREAD","render_thread"},
    {"BUFFER CACHE",counter_values,2,0,"THING_GPU_BUFFER_CACHE","buffer_cache"},
    {"SHADOWS",shadow_values,2,0,"THING_SHADOW_QUALITY","shadow_quality"},
    {"RENDER OPTIONS",nav_values,1,0,"",""},
    {"ASYNC PRESENT",counter_values,2,1,"DS_RT_ASYNC_SWAP","async_present"},
    {"ONE UPLOAD",counter_values,2,0,"DS_ONE_UPLOAD","one_upload"},
    {"RANGED DRAWS",counter_values,2,1,"DS_RANGE_DRAW","ranged_draws"},
    {"ALPHA ELISION",counter_values,2,1,"DS_ALPHA_ELIDE","alpha_elide"},
    {"DEPTH INVALIDATE",counter_values,2,1,"DS_INVALIDATE_DEPTH","invalidate_depth"},
};
static const int main_rows[]={0,1,6};
static const int render_rows[]={3,2,5,4,7,8,9,10,11};
static int selection=0, page=0;   /* page 0 = main settings, 1 = render options */
static int active_count(void){return page==0?PAGE0_COUNT:PAGE1_COUNT;}
/* Row index into items[] for the current page selection, or -1 for SAVE/CANCEL/BACK. */
static int selected_item(void){
    if(page==0)return selection<=OPTIONS_ITEM?main_rows[selection]:-1;
    return selection<PAGE1_ITEMS?render_rows[selection]:-1;
}
/* Keep saved preferences when a mode makes an option unavailable. */
static const char *blocked_reason(int it){
    int threaded=items[3].index==0;
    if(it==7&&!threaded)return "REQUIRES THREADED";
    if((it==9||it==11)&&threaded)return "REQUIRES SINGLE";
    return NULL;
}
static void change_item(int it,int direction){
    if(it<0||blocked_reason(it))return;
    items[it].index=(items[it].index+direction+items[it].count)%items[it].count;
}
static int chosen_action=-1;
static const char *config_path;
static const char *game_launcher;
static const char *subtitle="SETTINGS";
static volatile sig_atomic_t interrupted=0;
static void interrupt_handler(int sig){(void)sig;interrupted=1;}

/* 5x7 font, rows top to bottom, bits 4..0 left to right. */
typedef struct { char c; uint8_t rows[7]; } Glyph;
static const Glyph glyphs[]={
    {'A',{0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}},{'B',{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}},
    {'C',{0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}},{'D',{0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}},
    {'E',{0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}},{'F',{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}},
    {'G',{0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}},{'H',{0x11,0x11,0x11,0x1F,0x11,0x11,0x11}},
    {'I',{0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}},{'J',{0x07,0x02,0x02,0x02,0x02,0x12,0x0C}},
    {'K',{0x11,0x12,0x14,0x18,0x14,0x12,0x11}},{'L',{0x10,0x10,0x10,0x10,0x10,0x10,0x1F}},
    {'M',{0x11,0x1B,0x15,0x15,0x11,0x11,0x11}},{'N',{0x11,0x19,0x15,0x13,0x11,0x11,0x11}},
    {'O',{0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}},{'P',{0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}},
    {'Q',{0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}},{'R',{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}},
    {'S',{0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}},{'T',{0x1F,0x04,0x04,0x04,0x04,0x04,0x04}},
    {'U',{0x11,0x11,0x11,0x11,0x11,0x11,0x0E}},{'V',{0x11,0x11,0x11,0x11,0x11,0x0A,0x04}},
    {'W',{0x11,0x11,0x11,0x15,0x15,0x15,0x0A}},{'X',{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}},
    {'Y',{0x11,0x11,0x0A,0x04,0x04,0x04,0x04}},{'Z',{0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}},
    {'0',{0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}},{'1',{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}},
    {'2',{0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}},{'3',{0x1F,0x02,0x04,0x02,0x01,0x11,0x0E}},
    {'4',{0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}},{'5',{0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}},
    {'6',{0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}},{'7',{0x1F,0x01,0x02,0x04,0x08,0x08,0x08}},
    {'8',{0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}},{'9',{0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}},
    {' ',{0,0,0,0,0,0,0}},{'%',{0x19,0x1A,0x02,0x04,0x08,0x0B,0x13}},
    {':',{0,0x04,0x04,0,0x04,0x04,0}},{'>',{0x08,0x04,0x02,0x01,0x02,0x04,0x08}},
    {'<',{0x02,0x04,0x08,0x10,0x08,0x04,0x02}},{'-',{0,0,0,0x1F,0,0,0}},
    {'.',{0,0,0,0,0,0x0C,0x0C}},{'/',{0x01,0x01,0x02,0x04,0x08,0x10,0x10}},
    {'&',{0x0C,0x12,0x14,0x08,0x15,0x12,0x0D}},{'+',{0,0x04,0x04,0x1F,0x04,0x04,0}},
    {'!',{0x04,0x04,0x04,0x04,0x04,0,0x04}},{'?',{0x0E,0x11,0x01,0x02,0x04,0,0x04}},
    {'\'',{0x04,0x04,0,0,0,0,0}},{'X',{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}},
};
#define GLYPH_COUNT ((int)(sizeof glyphs/sizeof glyphs[0]))

static void glyph_rows(char c,int rows[7]){
    for(int i=0;i<7;i++)rows[i]=0;
    if(c>='a'&&c<='z')c=(char)(c-'a'+'A');
    for(int i=0;i<GLYPH_COUNT;i++)if(glyphs[i].c==c){for(int r=0;r<7;r++)rows[r]=glyphs[i].rows[r];return;}
}
static int text_width(const char *s,int scale){return (int)strlen(s)*(FONT_W+1)*scale;}
static void draw_text(SDL_Renderer *ren,const char *s,int x,int y,int scale,SDL_Color color){
    SDL_SetRenderDrawColor(ren,color.r,color.g,color.b,color.a);
    for(;*s;s++,x+=(FONT_W+1)*scale){
        int rows[7];glyph_rows(*s,rows);
        for(int r=0;r<7;r++)for(int c=0;c<FONT_W;c++)if(rows[r]&(1<<(FONT_W-1-c)))
            for(int dy=0;dy<scale;dy++)for(int dx=0;dx<scale;dx++)
                SDL_RenderDrawPoint(ren,x+c*scale+dx,y+r*scale+dy);
    }
}
static void draw_text_shadow(SDL_Renderer *ren,const char *s,int x,int y,int scale,SDL_Color color){
    SDL_Color shadow={3,6,24,255};draw_text(ren,s,x+2,y+2,scale,shadow);draw_text(ren,s,x,y,scale,color);
}
static void draw_frame(SDL_Renderer *ren){
    SDL_Color bg_top={5,10,44,255},bg_bottom={2,16,66,255};
    for(int y=0;y<H;y++){SDL_Color c;float t=y/(float)(H-1);
        c.r=(Uint8)(bg_top.r+(bg_bottom.r-bg_top.r)*t);c.g=(Uint8)(bg_top.g+(bg_bottom.g-bg_top.g)*t);
        c.b=(Uint8)(bg_top.b+(bg_bottom.b-bg_top.b)*t);c.a=255;
        SDL_SetRenderDrawColor(ren,c.r,c.g,c.b,255);SDL_RenderDrawLine(ren,0,y,W,y);}
    SDL_Color line={90,180,255,255},line2={30,70,150,255};
    SDL_SetRenderDrawColor(ren,line.r,line.g,line.b,line.a);
    SDL_Rect outer={18,14,W-36,H-28};SDL_RenderDrawRect(ren,&outer);
    SDL_Rect inner={22,18,W-44,H-36};SDL_RenderDrawRect(ren,&inner);
    SDL_SetRenderDrawColor(ren,line2.r,line2.g,line2.b,line2.a);
    SDL_Rect third={26,22,W-52,H-44};SDL_RenderDrawRect(ren,&third);
    for(int y=0;y<H;y+=3){SDL_SetRenderDrawColor(ren,0,0,0,26);SDL_RenderDrawLine(ren,20,y,W-20,y);}
    SDL_Color title={180,230,255,255},sub={90,170,240,255};
    draw_text_shadow(ren,"THE RETHING",(W-text_width("THE RETHING",5))/2,44,5,title);
    draw_text_shadow(ren,subtitle,(W-text_width(subtitle,2))/2,104,2,sub);
    SDL_Color divider={60,120,200,255};SDL_SetRenderDrawColor(ren,divider.r,divider.g,divider.b,divider.a);
    SDL_RenderDrawLine(ren,60,126,W-60,126);
}
static void draw_row(SDL_Renderer *ren,const char *label,const char *value,int y,int selected,SDL_Color normal,SDL_Color hot){
    SDL_Color c=selected?hot:normal;
    if(selected){draw_text_shadow(ren,">",56,y,3,hot);
        SDL_SetRenderDrawColor(ren,120,210,255,90);SDL_RenderDrawLine(ren,80,y+24,W-60,y+24);}
    draw_text_shadow(ren,label,86,y,3,c);
    int vw=text_width(value,3);draw_text_shadow(ren,value,W-70-vw,y,3,c);
}
static void load_config(void){
    FILE *f=fopen(config_path,"r");if(!f)return;char line[128];
    while(fgets(line,sizeof line,f)){
        char *eq=strchr(line,'=');if(!eq)continue;*eq=0;
        char *k=line,*v=eq+1;v[strcspn(v,"\r\n")]=0;
        for(Item *it=items;it<items+MAX_ITEMS;it++)if(it->cfg[0]&&!strcmp(k,it->cfg))
            for(int i=0;i<it->count;i++){
                char num[16];snprintf(num,sizeof num,"%d",i);
                int match=0;
                if(!strcmp(it->cfg,"fps_limit")){match=!strcmp(v,"0")&&i==0||!strcmp(v,"60")&&i==1||!strcmp(v,"30")&&i==2;}
                else if(!strcmp(it->cfg,"fps_counter")){match=!strcmp(v,"0")&&i==0||!strcmp(v,"1")&&i==1;}
                else if(!strcmp(it->cfg,"resolution")){static const char *names[]={"native","640x480","512x384"};match=i<3&&!strcmp(v,names[i]);}
                else if(!strcmp(it->cfg,"render_thread")){match=(!strcmp(v,"0")&&i==1)||(!strcmp(v,"1")&&i==0)||(!strcmp(v,"threaded")&&i==0)||(!strcmp(v,"single")&&i==1);}
                else match=!strcmp(v,num);
                if(match){it->index=i;break;}
            }
    }
    fclose(f);
}
static int save_config(void){
    FILE *f=fopen(config_path,"w");if(!f)return -1;
    const char *fps[]={"0","60","30"},*ctr[]={"0","1"},*res[]={"native","640x480","512x384"},*thr[]={"1","0"};
    fprintf(f,"fps_limit=%s\nfps_counter=%s\nresolution=%s\nrender_thread=%s\nbuffer_cache=%d\nshadow_quality=%d\n"
              "async_present=%d\none_upload=%d\nranged_draws=%d\nalpha_elide=%d\ninvalidate_depth=%d\n",
        fps[items[0].index],ctr[items[1].index],res[items[2].index],thr[items[3].index],items[4].index,items[5].index,
        items[7].index,items[8].index,items[9].index,items[10].index,items[11].index);
    fclose(f);return 0;
}
static int launch_game(void){
    setenv("THING_FPS_LIMIT",(items[0].index==0)?"0":(items[0].index==1)?"60":"30",1);
    setenv("THING_FPS_OVERLAY",items[1].index?"1":"0",1);
    unsetenv("THING_BRIGHTNESS");unsetenv("DS_BRIGHTNESS");
    if(items[2].index==2){setenv("THING_RENDER_WIDTH","512",1);setenv("THING_RENDER_HEIGHT","384",1);}
    else if(items[2].index==1){setenv("THING_RENDER_WIDTH","640",1);setenv("THING_RENDER_HEIGHT","480",1);}
    else{unsetenv("THING_RENDER_WIDTH");unsetenv("THING_RENDER_HEIGHT");unsetenv("DS_RENDER_WIDTH");unsetenv("DS_RENDER_HEIGHT");}
    setenv("DS_RENDER_THREAD",items[3].index==0?"1":"0",1);
    setenv("THING_GPU_BUFFER_CACHE",items[4].index?"1":"0",1);
    setenv("THING_SHADOW_QUALITY",items[5].index?"1":"0",1);
    /* Renderer options: preserve saved preferences even for blocked rows; the launcher
     * writes an explicit value so an OFF selection survives. */
    for(int i=7;i<MAX_ITEMS;i++)setenv(items[i].env,items[i].index?"1":"0",1);
    setenv("THING_LAUNCHER_SKIP_MENU","1",1);
    execl("/bin/bash","/bin/bash",game_launcher,(char*)NULL);
    return -1;
}
/* A child process owns extraction; SDL stays responsive throughout installation. */
static int install_game(SDL_Renderer *ren,const char *root,SDL_GameController **pad){
    char progress[2048],log[2048];
    /* Setup is compiled into this launcher; no interpreter required. */
    snprintf(progress,sizeof progress,"%s/tmp/install-status.txt",root);
    snprintf(log,sizeof log,"%s/tmp/install.log",root);
    subtitle="INSTALLATION";
    pid_t child=-1;int failed=0,percent=-1,done=0,cancel=0;
    char message[96]="PREPARING GAME";
    const char *preview=getenv("THING_INSTALLER_PREVIEW");
    if(preview){percent=50;strcpy(message,"APPLYING PATCH 1.1");}
    while(!done){
        if(interrupted){cancel=1;break;}
        if(child<0&&!failed&&!preview){
            unlink(progress);child=fork();
            if(child==0){
                setpgid(0,0);int fd=open(log,O_WRONLY|O_CREAT|O_TRUNC,0644);
                if(fd>=0){dup2(fd,1);dup2(fd,2);close(fd);}
                _exit(thing_native_setup(root,0));
            }
            if(child>0)setpgid(child,child);
            else{failed=1;snprintf(message,sizeof message,"CANNOT START INSTALLER");}
        }
        if(child>0){int code;pid_t result=waitpid(child,&code,WNOHANG);
            if(result==child){child=-1;
                if(WIFEXITED(code)&&WEXITSTATUS(code)==0){done=1;break;}
                failed=1;snprintf(message,sizeof message,"INSTALL FAILED - CHECK TMP/INSTALL.LOG");
            }
        }
        FILE *f=preview?NULL:fopen(progress,"r");if(f){char line[128];
            if(fgets(line,sizeof line,f))percent=atoi(line);
            if(fgets(message,sizeof message,f))message[strcspn(message,"\r\n")]=0;
            fclose(f);
        }
        SDL_Event e;while(SDL_PollEvent(&e)){
            if(e.type==SDL_CONTROLLERDEVICEADDED&&!*pad&&SDL_IsGameController(e.cdevice.which))*pad=SDL_GameControllerOpen(e.cdevice.which);
            if(e.type==SDL_CONTROLLERDEVICEREMOVED&&*pad&&!SDL_GameControllerGetAttached(*pad)){SDL_GameControllerClose(*pad);*pad=NULL;}
            if(e.type==SDL_QUIT||(e.type==SDL_KEYDOWN&&e.key.keysym.sym==SDLK_ESCAPE)||(e.type==SDL_CONTROLLERBUTTONDOWN&&e.cbutton.button==SDL_CONTROLLER_BUTTON_B)){cancel=1;done=1;}
            if(failed&&((e.type==SDL_KEYDOWN&&e.key.keysym.sym==SDLK_RETURN)||(e.type==SDL_CONTROLLERBUTTONDOWN&&e.cbutton.button==SDL_CONTROLLER_BUTTON_A))){failed=0;percent=-1;strcpy(message,"PREPARING GAME");}
        }
        draw_frame(ren);SDL_Color text={150,200,255,255},bright={220,245,255,255};
        draw_text_shadow(ren,failed?"INSTALLATION STOPPED":"FIRST RUN SETUP",60,172,3,bright);
        /* Wrap the status message without clipping the 640-pixel logical display. */
        for(int row=0;row<2;row++){char line[43]={0};size_t start=row*42;
            if(strlen(message)>start){strncpy(line,message+start,42);draw_text(ren,line,60,225+row*18,2,text);}}
        SDL_SetRenderDrawColor(ren,8,25,70,255);SDL_Rect bar={48,280,544,26};SDL_RenderFillRect(ren,&bar);
        int pulse=(SDL_GetTicks()/70)%72;
        for(int col=0;col<66;col++)for(int row=0;row<3;row++){
            int active=percent<0?(col>=pulse-6&&col<pulse):(col<percent*66/100);
            SDL_SetRenderDrawColor(ren,active?90:25,active?180:60,active?255:110,255);
            SDL_Rect cell={50+col*8,284+row*6,5,4};SDL_RenderFillRect(ren,&cell);
        }
        draw_text(ren,"PROGRESS BY INSTALLATION STAGE",60,320,2,text);
        draw_text(ren,failed?"A RETRY   B EXIT":"B CANCEL - SAFE TO RETRY NEXT TIME",60,380,2,text);
        if(preview){int ow,oh;SDL_GetRendererOutputSize(ren,&ow,&oh);
            SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,ow,oh,32,SDL_PIXELFORMAT_RGBA32);
            if(surface){SDL_RenderReadPixels(ren,NULL,SDL_PIXELFORMAT_RGBA32,surface->pixels,surface->pitch);SDL_SaveBMP(surface,preview);SDL_FreeSurface(surface);}
            cancel=1;done=1;}
        SDL_RenderPresent(ren);SDL_Delay(30);
    }
    if(child>0){kill(-child,SIGTERM);
        for(int i=0;i<50;i++){if(waitpid(child,NULL,WNOHANG)==child){child=-1;break;}SDL_Delay(20);}
        if(child>0){kill(-child,SIGKILL);waitpid(child,NULL,0);}}
    subtitle="SETTINGS";return cancel?-1:0;
}
int main(int argc,char **argv){
    if(argc==3 && !strcmp(argv[1],"--setup"))return thing_native_setup(argv[2],0);
    if(argc==3 && !strcmp(argv[1],"--apply-shadows"))return thing_native_setup(argv[2],1);
    signal(SIGTERM,interrupt_handler);signal(SIGINT,interrupt_handler);signal(SIGHUP,interrupt_handler);
    const char *root=getenv("THING_ROOT");
    if(argc>1)root=argv[1];
    if(argc>2)game_launcher=argv[2];
    if(!root)root=".";
    static char cfg[1024],fallback[1024];
    const char *dirs[]={"config","tmp","userdata","cache","saves"};
    for(int i=0;i<5;i++){snprintf(cfg,sizeof cfg,"%s/%s",root,dirs[i]);mkdir(cfg,0755);}
    snprintf(cfg,sizeof cfg,"%s/config/launcher.cfg",root?root:".");
    config_path=cfg;
    snprintf(fallback,sizeof fallback,"%s/run-game.sh",root);
    if(!game_launcher)game_launcher=fallback;
    if(getenv("THING_LAUNCHER_SKIP_MENU")){load_config();return launch_game()==0?0:1;}
    if(getenv("THING_LAUNCHER_SELFTEST")){
        load_config();
        items[3].index=1;   /* SINGLE */
        items[2].index=1;   /* 640X480 */
        if(save_config())return 1;
        for(Item *it=items;it<items+MAX_ITEMS;it++)if(it->cfg[0])it->index=0;
        load_config();
        printf("fps=%d counter=%d resolution=%d renderer=%d cache=%d\n",
            items[0].index,items[1].index,items[2].index,items[3].index,items[4].index);
        return 0;
    }
    load_config();
    /* Diagnostic-only: screenshot the render-options page without input. */
    if(getenv("THING_LAUNCHER_SHOT_PAGE")&&!strcmp(getenv("THING_LAUNCHER_SHOT_PAGE"),"1")){page=1;selection=0;}
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS)){fprintf(stderr,"launcher: SDL_Init: %s\n",SDL_GetError());return 1;}
    uint32_t wflags=getenv("THING_LAUNCHER_WINDOWED")?SDL_WINDOW_SHOWN:(SDL_WINDOW_FULLSCREEN_DESKTOP|SDL_WINDOW_SHOWN);
    SDL_Window *win=SDL_CreateWindow("The ReThing",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,W,H,wflags);
    if(!win)win=SDL_CreateWindow("The ReThing",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,W,H,SDL_WINDOW_SHOWN);
    SDL_Renderer *ren=win?SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC):NULL;
    if(!ren&&win)ren=SDL_CreateRenderer(win,-1,SDL_RENDERER_SOFTWARE);
    if(!ren){fprintf(stderr,"launcher: no renderer: %s\n",SDL_GetError());return 1;}
    SDL_RenderSetLogicalSize(ren,W,H);
    SDL_GameController *pad=NULL;for(int i=0;i<SDL_NumJoysticks();i++)if(SDL_IsGameController(i)){pad=SDL_GameControllerOpen(i);if(pad)break;}
    SDL_ShowCursor(SDL_DISABLE);
    if(!getenv("THING_LAUNCHER_SHOT") && install_game(ren,root,&pad)){if(pad)SDL_GameControllerClose(pad);SDL_DestroyRenderer(ren);SDL_DestroyWindow(win);SDL_Quit();return 0;}
    int running=1,shot=0;
    const char *shot_path=getenv("THING_LAUNCHER_SHOT");
    uint32_t last_move=0;
    while(running&&!interrupted){
        SDL_Event ev;
        while(SDL_PollEvent(&ev)){
            if(ev.type==SDL_CONTROLLERDEVICEADDED&&!pad&&SDL_IsGameController(ev.cdevice.which))pad=SDL_GameControllerOpen(ev.cdevice.which);
            else if(ev.type==SDL_CONTROLLERDEVICEREMOVED&&pad&&!SDL_GameControllerGetAttached(pad)){SDL_GameControllerClose(pad);pad=NULL;}
            else if(ev.type==SDL_QUIT)running=0;
            else if(ev.type==SDL_KEYDOWN){
                int k=ev.key.keysym.sym;
                if(k==SDLK_ESCAPE)running=0;
                else if(k==SDLK_UP){selection=(selection+active_count()-1)%active_count();}
                else if(k==SDLK_DOWN){selection=(selection+1)%active_count();}
                else if(k==SDLK_LEFT){int it=selected_item();change_item(it,-1);}
                else if(k==SDLK_RIGHT){int it=selected_item();change_item(it,1);}
                else if(k==SDLK_RETURN||k==SDLK_SPACE){
                    if(page==0){
                        if(selection==PAGE0_SAVE){if(save_config()==0){chosen_action=1;running=0;}}
                        else if(selection==PAGE0_CANCEL){chosen_action=0;running=0;}
                        else if(selection==OPTIONS_ITEM){page=1;selection=0;}
                        else change_item(selected_item(),1);
                    }else{
                        if(selection==PAGE1_BACK){page=0;selection=OPTIONS_ITEM;}
                        else change_item(selected_item(),1);
                    }
                }
            } else if(ev.type==SDL_CONTROLLERBUTTONDOWN){
                if(ev.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_UP)selection=(selection+active_count()-1)%active_count();
                else if(ev.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_DOWN)selection=(selection+1)%active_count();
                else if(ev.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_LEFT){int it=selected_item();change_item(it,-1);}
                else if(ev.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_RIGHT){int it=selected_item();change_item(it,1);}
                else if(ev.cbutton.button==SDL_CONTROLLER_BUTTON_A){
                    if(page==0){
                        if(selection==PAGE0_SAVE){if(save_config()==0){chosen_action=1;running=0;}}
                        else if(selection==PAGE0_CANCEL){chosen_action=0;running=0;}
                        else if(selection==OPTIONS_ITEM){page=1;selection=0;}
                        else change_item(selected_item(),1);
                    }else{
                        if(selection==PAGE1_BACK){page=0;selection=OPTIONS_ITEM;}
                        else change_item(selected_item(),1);
                    }
                } else if(ev.cbutton.button==SDL_CONTROLLER_BUTTON_B){
                    if(page==1){page=0;selection=OPTIONS_ITEM;}else{chosen_action=0;running=0;}
                }
            } else if(ev.type==SDL_CONTROLLERAXISMOTION){
                Sint16 v=ev.caxis.value;uint32_t now=SDL_GetTicks();
                if(now-last_move>180){
                    if(v<-20000||v>20000){
                        int dir=v<0?-1:1;
                        if(ev.caxis.axis==SDL_CONTROLLER_AXIS_LEFTY)selection=(selection+dir+active_count())%active_count();
                        else if(ev.caxis.axis==SDL_CONTROLLER_AXIS_LEFTX){int it=selected_item();change_item(it,dir);}
                        last_move=now;
                    }
                }
            }
        }
        subtitle=page==0?"SETTINGS":"RENDER OPTIONS";
        draw_frame(ren);
        SDL_Color normal={150,200,255,255},hot={220,245,255,255},dim={80,140,210,255};(void)dim;
        if(page==0){
            for(int i=0;i<=OPTIONS_ITEM;i++){
                int it=main_rows[i];
                const char *value=i<KEY_COUNT?items[it].values[items[it].index]:"OPEN";
                draw_row(ren,items[it].label,value,130+i*30,selection==i,normal,hot);
            }
            draw_row(ren,"SAVE & LAUNCH","",340,selection==PAGE0_SAVE,hot,hot);
            draw_row(ren,"CANCEL","",372,selection==PAGE0_CANCEL,normal,hot);
        }else{
            for(int i=0;i<PAGE1_ITEMS;i++){
                int it=render_rows[i];const char *reason=blocked_reason(it);
                SDL_Color disabled={85,95,110,255};
                draw_row(ren,items[it].label,reason?"BLOCKED":items[it].values[items[it].index],135+i*25,selection==i,reason?disabled:normal,reason?disabled:hot);
            }
            draw_row(ren,"BACK","",135+PAGE1_ITEMS*25,selection==PAGE1_BACK,normal,hot);
        }
        SDL_Color hint={60,110,180,255};
        const char *reason=blocked_reason(selected_item());
        const char *hint_text=reason?reason:page==0?"UP/DOWN MOVE  LEFT/RIGHT CHANGE  A SELECT  B CANCEL":"UP/DOWN MOVE  LEFT/RIGHT CHANGE  A SELECT  B BACK";
        draw_text(ren,hint_text,(W-text_width(hint_text,1))/2,H-52,1,hint);
        if(shot_path&&!shot){int ow=W,oh=H;SDL_GetRendererOutputSize(ren,&ow,&oh);SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,ow,oh,32,SDL_PIXELFORMAT_RGBA32);if(s){SDL_RenderReadPixels(ren,NULL,SDL_PIXELFORMAT_RGBA32,s->pixels,s->pitch);SDL_SaveBMP(s,shot_path);SDL_FreeSurface(s);}shot=1;}
        SDL_RenderPresent(ren);if(shot_path&&shot)break;SDL_Delay(16);
    }
    if(pad)SDL_GameControllerClose(pad);SDL_DestroyRenderer(ren);SDL_DestroyWindow(win);SDL_Quit();
    if(shot_path)return 0;
    if(chosen_action==1)return launch_game()==0?0:1;
    return 0;
}
