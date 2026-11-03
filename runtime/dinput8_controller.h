/* Controller-to-keyboard/mouse path for the original PC keyboard bindings.
 * Included after the shared DirectInput state and DIK/VK conversion helpers.
 * All motion uses elapsed monotonic time; it does not change simulation speed.
 */
static uint8_t controller_keyboard[256],controller_mouse[8];
static uint32_t controller_vk[256];
static SDL_GameController *controller_pad;
static DS_PadConfig controller_config;
static DS_PadRaw controller_event_raw;
static int controller_ready,controller_focused=1;
static unsigned controller_previous;
static uint64_t controller_tick;
static double controller_x=320,controller_y=240,controller_fx,controller_fy;

static int controller_native(void){const char *mode=getenv("THING_PAD_MODE");return mode&&!strcmp(mode,"native");}
static void controller_read_raw(SDL_GameController *pad,DS_PadRaw *raw){
    memset(raw,0,sizeof *raw);if(!pad||!SDL_GameControllerGetAttached(pad))return;
    for(int a=0;a<SDL_CONTROLLER_AXIS_MAX;a++)raw->axes[a]=SDL_GameControllerGetAxis(pad,(SDL_GameControllerAxis)a);
    for(int b=0;b<SDL_CONTROLLER_BUTTON_MAX&&b<32;b++)if(SDL_GameControllerGetButton(pad,(SDL_GameControllerButton)b))raw->buttons|=1u<<b;
}
static SDL_GameController *controller_open(void){
    if(controller_pad&&SDL_GameControllerGetAttached(controller_pad))return controller_pad;
    if(controller_pad)SDL_GameControllerClose(controller_pad);controller_pad=NULL;
    for(int i=0;i<SDL_NumJoysticks();i++)if(SDL_IsGameController(i)){
        controller_pad=SDL_GameControllerOpen(i);if(controller_pad)break;
    }
    controller_read_raw(controller_pad,&controller_event_raw);
    return controller_pad;
}
static void controller_keys(const uint8_t *next){
    for(unsigned key=0;key<256;key++){
        int before=keyboard[key]||controller_keyboard[key],after=keyboard[key]||next[key];
        if(before!=after){buffer_kind(K_KEYBOARD,key,after?0x80:0,1);if(controller_vk[key])hle_input_key(after,controller_vk[key],key);}
    }
    memcpy(controller_keyboard,next,256);
}
static void controller_mouse_state(const uint8_t *next){
    static const uint32_t down[]={0x201,0x204,0x207},up[]={0x202,0x205,0x208};
    for(unsigned i=0;i<3;i++){
        int before=mouse_buttons[i]||controller_mouse[i],after=mouse_buttons[i]||next[i];
        if(before!=after){buffer_kind(K_MOUSE,12+i,after?0x80:0,1);hle_input_mouse(after?down[i]:up[i],(int)controller_x,(int)controller_y);}
    }
    memcpy(controller_mouse,next,8);
}
static void controller_release(void){
    uint8_t none[256]={0};controller_keys(none);controller_mouse_state(none);
    controller_previous=0;controller_fx=controller_fy=0;controller_tick=SDL_GetPerformanceCounter();
}
static void controller_key(uint8_t *next,SDL_Scancode scan){
    unsigned key=dik(scan);if(!key)return;next[key]=0x80;
    SDL_Keysym sym={.scancode=scan,.sym=SDL_GetKeyFromScancode(scan)};controller_vk[key]=vk(sym);
}
static void controller_update_impl(int consume_sample,const SDL_Event *event){
    if(controller_native()||!(SDL_WasInit(SDL_INIT_GAMECONTROLLER)&SDL_INIT_GAMECONTROLLER))return;
    if(!controller_ready){
        ds_pad_defaults(&controller_config);const char *path=getenv("THING_PAD_CONFIG");if(!path)path=getenv("DS_PAD_CONFIG");
        if(path&&ds_pad_load(&controller_config,path))fprintf(stderr,"[controller] invalid config %s; defaults retained\n",path);
        controller_ready=1;
        fprintf(stderr,"[controller] keyboard/mouse mode; original W/S/A/D E R Space I Q F bindings; THING_PAD_MODE=native selects joystick ABI\n");
    }
    if(!controller_focused){controller_release();return;}
    const char *automation=getenv("THING_TEST_INPUT");if(!automation)automation=getenv("DS_TEST_INPUT");
    /* Hold synthetic taps until a successful keyboard/mouse sample consumes
     * them; message-queue pumping during loading must not lose both edges. */
    if(automation&&!strcmp(automation,"1")&&!consume_sample)return;
    SDL_GameController *pad=controller_open();DS_PadState state;
    if(event){
        if(!pad)return;
        SDL_JoystickID instance=SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad));
        if(event->type==SDL_CONTROLLERAXISMOTION){
            if(event->caxis.which!=instance||event->caxis.axis>=SDL_CONTROLLER_AXIS_MAX)return;
            controller_event_raw.axes[event->caxis.axis]=event->caxis.value;
        }else{
            if(event->cbutton.which!=instance||event->cbutton.button>=32)return;
            uint32_t bit=1u<<event->cbutton.button;
            if(event->type==SDL_CONTROLLERBUTTONDOWN)controller_event_raw.buttons|=bit;else controller_event_raw.buttons&=~bit;
        }
        ds_pad_encode(&controller_event_raw,&controller_config,&state);
    }else{
        if(consume_sample)ds_test_input_sample(pad,&controller_config,&state);else ds_pad_sample(pad,&controller_config,&state);
        controller_read_raw(pad,&controller_event_raw);
    }
    uint8_t keys[256]={0},buttons[8]={0};unsigned held=0;
    if(state.x < -32)controller_key(keys,SDL_SCANCODE_A);
    if(state.x > 32)controller_key(keys,SDL_SCANCODE_D);
    if(state.y < -32)controller_key(keys,SDL_SCANCODE_W);
    if(state.y > 32)controller_key(keys,SDL_SCANCODE_S);
    for(unsigned i=0;i<16;i++)if(state.buttons[i]){
        held|=1u<<i;
        for(unsigned b=0;b<2;b++){
            int binding=controller_config.key[i][b];
            if(binding>0)controller_key(keys,(SDL_Scancode)binding);
            else if(binding==-1)buttons[0]=0x80;
            else if(binding==-2)buttons[1]=0x80;
            else if(binding==-5)buttons[2]=0x80;
            else if((binding==-3||binding==-4)&&!(controller_previous&(1u<<i))){int delta=binding==-3?120:-120;mouse_dz+=delta;buffer_motion(0,0,delta);}
        }
    }
    controller_keys(keys);controller_mouse_state(buttons);controller_previous=held;
    /* Replay edges only: elapsed-time camera integration belongs to the final
     * sampled state, so intermediate axis events cannot consume its clock. */
    if(event)return;
    uint64_t now=SDL_GetPerformanceCounter(),freq=SDL_GetPerformanceFrequency();
    double dt=controller_tick?(now-controller_tick)/(double)freq:0;controller_tick=now;if(dt>.1)dt=.1;
    double speed=650;const char *configured=getenv("THING_CAMERA_SPEED");
    if(configured){char *end;double n=strtod(configured,&end);if(end!=configured&&!*end&&n>=1&&n<=10000)speed=n;}
    controller_fx+=state.z/128.0*speed*dt;controller_fy+=state.rz/128.0*speed*dt;
    int dx=(int)controller_fx,dy=(int)controller_fy;controller_fx-=dx;controller_fy-=dy;
    if(dx||dy){
        mouse_dx+=dx;mouse_dy+=dy;buffer_motion(dx,dy,0);controller_x+=dx;controller_y+=dy;
        int left=0,top=0,right=640,bottom=480;uint32_t hwnd=hle_first_hwnd();
        if(hwnd)hle_window_screen_rect(hwnd,&left,&top,&right,&bottom);
        if(controller_x<left)controller_x=left;if(controller_y<top)controller_y=top;
        if(controller_x>=right)controller_x=right-1;if(controller_y>=bottom)controller_y=bottom-1;
        hle_input_mouse(0x200,(int)controller_x,(int)controller_y);
    }
}

static void controller_update(int consume_sample){controller_update_impl(consume_sample,NULL);}
/* Replay observed physical edges before taking the final held-state snapshot.
 * A short press/release can both arrive during a slow frame; retaining each
 * buffered transition prevents it vanishing. Command-file taps keep their
 * existing consume-on-guest-sample path and do not pass through here. */
static void controller_event(const SDL_Event *event){controller_update_impl(0,event);}
