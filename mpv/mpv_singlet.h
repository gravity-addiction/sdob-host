#ifndef _MPV_SINGLET_H_
#define _MPV_SINGLET_H_

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "./mpv.h"

int mpvSocketSinglet(struct mpv_conn *conn, char* cmd, int bCancel, char** ret);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // _MPV_SINGLET_H_