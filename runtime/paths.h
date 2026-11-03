#ifndef THING_PATHS_H
#define THING_PATHS_H
/* Resolve THING_ROOT or the native executable's directory, validate bin/data,
 * and create local writable storage. Does not modify HOME or runtime sockets. */
int ds_paths_init(void);
#endif
