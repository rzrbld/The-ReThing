static void audio_probe_read_end(uint32_t ctx,uint32_t request,int32_t returned,AudioProbeTime t){
 uint64_t end=audio_probe_ns(CLOCK_MONOTONIC),cpu=audio_probe_ns(CLOCK_THREAD_CPUTIME_ID)-t.cpu;
 fprintf(stderr,"[stream-cpu] context=%#x requested=%u returned=%d cpu_ns=%"PRIu64" wall_ns=%"PRIu64" end_ns=%"PRIu64"\n",ctx,request,returned,cpu,end-t.wall,end);
}
