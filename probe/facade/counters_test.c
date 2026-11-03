#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAX_SHIMS 2048
static struct {const char *name;} g_shim[MAX_SHIMS]={{"D3D8.Device.50"},{"ReadFile"}};
static int g_shim_n=2;
#include "counters.inc"
int main(void){facade_init();facade_calls[0]=17;facade_calls[1]=3;facade_lookups=23;facade_hits=20;thing_facade_same(0);thing_facade_present(1);if(thing_facade_enabled&&(facade_calls[0]||facade_calls[1]||facade_same[0]))return 1;facade_calls[0]=5;facade_lookups=facade_hits=5;thing_facade_present(2);thing_facade_finish();return 0;}
