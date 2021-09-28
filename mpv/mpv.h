#ifndef _MPV_H_
#define _MPV_H_

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

struct mpv_cmd_status {
  int resultReqId;
  int resultError;
  int resultData;
  char *jsonReqId;
  char *jsonError;
  char *jsonData;
};
struct mpv_cmd_status mpvSocketCmdStatus;

struct mpv_any_u {
  void* ptr;
  int integer;
  unsigned uinteger;
  double floating;
};

struct mpv_conn {
  char* socket_path;
  int fdSelect; // = -1;
  fd_set fdSet;
  struct timeval timeout; // = (struct timeval){0};
  unsigned int lastConn;
  int connected;
  pthread_mutex_t cmdLock;
  int reqId;
  int reqTop;
  int reqQueCnt;
  int reqQueI[100];
  char reqQue[100][32];
  void (*reqQueCb[100])(struct mpv_conn*, struct mpv_any_u*);
};

// int mpv_socket_fdSelect;
// fd_set mpv_socket_set;
// struct timeval mpv_socket_timeout;
// unsigned int mpv_socket_lastConn;
// static pthread_mutex_t mpvSocketCmdLock;

// ------------------------------
// mpv.c
void mpv_socket_close(int fd);
int mpv_fd_check(int fd);
struct mpv_conn * MPV_CONN_INIT();
void MPV_CONN_DESTROY(struct mpv_conn *conn);
int mpv_socket_conn(struct mpv_conn *conn, int bCancel);
int mpv_fd_write(struct mpv_conn *conn, char *data, int bCancel);
int mpv_cmd(struct mpv_conn *conn, char *cmd_string, int bCancel);
int mpv_fmt_cmd(struct mpv_conn *conn, char* fmt, ...);
int mpv_set_prop_char(struct mpv_conn *conn, char* prop, char* prop_val);
int mpv_set_prop_int(struct mpv_conn *conn, char* prop, int prop_val);
int mpv_set_prop_double(struct mpv_conn *conn, char* prop, double prop_val);
int mpv_cmd_prop_val(struct mpv_conn *conn, char* cmd, char* prop, double prop_val);
void mpv_quit(struct mpv_conn *conn);


#ifdef __cplusplus
}
#endif // __cplusplus
#endif // _MPV_H_
