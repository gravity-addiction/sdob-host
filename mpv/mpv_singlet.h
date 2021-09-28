#ifndef _MPV_SINGLET_H_
#define _MPV_SINGLET_H_

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "mpv.h"

unsigned int reqId;
unsigned int reqTop;

struct mpv_any_u * MPV_ANY_U_NEW();
void MPV_ANY_U_FREE(struct mpv_any_u *mpvu);
int mpvSocketSinglet(struct mpv_conn *conn, char* cmd, int bCancel, struct mpv_any_u** ret);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // _MPV_SINGLET_H_