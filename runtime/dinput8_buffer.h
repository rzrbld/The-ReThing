/* DirectInput buffered data: fixed-width guest records, real input transitions.
 * API/property references: Microsoft DirectInput8 SetProperty/GetDeviceData,
 * https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee417929(v=vs.85)
 * HRESULT/layout declarations were checked against Wine's public dinput.h;
 * no Wine implementation is copied.
 */
#define INPUT_BUFFER_MAX_EVENTS 4096u
static void buffer_reset(InputDevice *d){d->event_head=d->event_count=0;d->event_overflow=0;}
static int buffer_configure(InputDevice *d,uint32_t requested){
    unsigned capacity=requested?requested-1:0;
    if(capacity>INPUT_BUFFER_MAX_EVENTS)capacity=INPUT_BUFFER_MAX_EVENTS;
    InputEvent *events=capacity?calloc(capacity,sizeof(*events)):NULL;
    if(capacity&&!events)return -1;
    free(d->events);d->events=events;d->event_capacity=capacity;d->buffer_size=requested;
    buffer_reset(d);return 0;
}
static void buffer_event(InputDevice *d,uint32_t offset,uint32_t data,uint32_t stamp,uint32_t serial){
    if(!d->used||!d->acquired||d->lost||!d->buffer_size)return;
    /* Preserve the retained chronological prefix when capacity is exhausted;
     * acknowledge loss with DI_BUFFEROVERFLOW on the next non-peek read. */
    if(d->event_count==d->event_capacity){d->event_overflow=1;return;}
    unsigned at=(d->event_head+d->event_count)%d->event_capacity;
    d->events[at]=(InputEvent){offset,data,stamp,serial,0};++d->event_count;
}
static void buffer_source(InputDevice *d,unsigned source,uint32_t value,unsigned bytes,uint32_t stamp,uint32_t serial){
    if(!d->objects){if(source+bytes<=d->datasize)buffer_event(d,source,value,stamp,serial);return;}
    for(unsigned i=0;i<d->objects;i++){
        const FormatObject *format=&d->format[i];unsigned size=(format->type&0x0c)?1:4;
        if(size==bytes&&source_offset(format,d->kind)==(int)source&&format->offset+size<=d->datasize)
            buffer_event(d,format->offset,value,stamp,serial);
    }
}
static void buffer_kind(int kind,unsigned source,uint32_t value,unsigned bytes){
    uint32_t stamp=hle_ticks_ms(),serial=++sequence;
    for(unsigned i=0;i<MAX_DEVICES;i++)if(devices[i].used&&devices[i].kind==kind)
        buffer_source(&devices[i],source,value,bytes,stamp,serial);
}
static void buffer_motion(int x,int y,int wheel){
    if(x)buffer_kind(K_MOUSE,0,(uint32_t)x,4);
    if(y)buffer_kind(K_MOUSE,4,(uint32_t)y,4);
    if(wheel)buffer_kind(K_MOUSE,8,(uint32_t)wheel,4);
}
static int input_automation(void){
    const char *value=getenv("THING_TEST_INPUT");if(!value)value=getenv("DS_TEST_INPUT");
    return value&&!strcmp(value,"1");
}
static void input_focus_lost(void){
    for(unsigned i=0;i<MAX_DEVICES;i++){
        InputDevice *d=&devices[i];
        if(d->used&&d->acquired&&(d->coop_flags&4)){d->lost=1;d->acquired=0;buffer_reset(d);d->pad_sampled=0;}
    }
}
