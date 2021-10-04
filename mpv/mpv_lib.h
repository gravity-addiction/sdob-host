#ifndef _MPV_LIB_H_
#define _MPV_LIB_H_

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <mpv/client.h>

mpv_handle * mpv_handle_init();
void mpv_handle_destroy(mpv_handle *ctx);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // _MPV_LIB_H_