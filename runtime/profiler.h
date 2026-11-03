#ifndef THING_CPU_PROFILER_H
#define THING_CPU_PROFILER_H
/* Linux main-thread CPU-time samples; enabled only by THING_CPU_PROFILE=1.
 * Call all three functions from the same thread. Flush preserves accumulated
 * samples and resumes sampling; stop flushes and restores the old signal action. */
void thing_cpu_profiler_start(void);
void thing_cpu_profiler_flush(void);
void thing_cpu_profiler_stop(void);
/* Main-thread Present boundary: subsequent samples belong to this frame. */
void thing_cpu_profiler_frame(unsigned next_frame);
#endif
