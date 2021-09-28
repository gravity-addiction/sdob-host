#ifndef DBG_H
#define DBG_H
#include <stdlib.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// DBG_ERROR is enabled by default
typedef unsigned long dbgmask_t;

#define DBG_ERROR     1
#define DBG_DEBUG     10
#define DBG_INFO      100

#define DBG_MPV_WRITE 1000
#define DBG_MPV_READ  10000
#define DBG_MPV_EVENT 100000
#define DBG_QUEUE     1000000
#define DBG_SLIDER    10000000
#define DBG_VIDEOLIST 100000000
#define DBG_SVR       1000000000
#define DBG_WPA       10000000000

void init_dbg();

int16_t gui_debug_out(char x);

void dbgprintf(dbgmask_t fl, const char* fmt, ...);
void sigprintf(const char* fmt, ...); /* same as fprintf(stderr, fmt, ...), but safe from signal handler */

// Returns 1 if the given mask overlaps with the configured mask,
// 0 otherwise.
int dbg_enabled_p(dbgmask_t);

// From GUISlice, returns 0 for no output, 1 for errors, 2 for errors & debug info
int get_debug_err();

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // QUEUE_H
