#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <jansson.h>
#include <arpa/inet.h>

#include "../shared.h"
#include "mpv.h"

void mpv_socket_close(int fd) {
  if (fd) { close(fd); }
}

int mpv_fd_check(int fd) {

  // 16-Feb-2020 - disabling this -- after the synchonization fix
  // applied last week, this really serves no purpose.
  // Reset socket if more than 10 seconds since last reconnection
  /*if (0 && millis() - mpv_socket_lastConn > 10000) {
    dbgprintf(DBG_MPV_WRITE|DBG_MPV_READ, "Proactively closing MPV socket\n");
    mpv_socket_close(fd);
    return 0;
  }*/

  // Check Socket is in good standing
  if (fd > -1) {
    int error = 0;
    socklen_t len = sizeof(error);
    int retval = getsockopt (fd, SOL_SOCKET, SO_ERROR, &error, &len);

    if (retval != 0) {
      /* there was a problem getting the error code */
      dbgprintf(DBG_ERROR, "error getting socket error code: %s\n", strerror(retval));
      mpv_socket_close(fd);
      return retval;
    }

    if (error != 0) {
      /* socket has a non zero error status */
      dbgprintf(DBG_ERROR, "socket error: %s\n", strerror(error));
      mpv_socket_close(fd);
    }
    return error;
  }
  return 1;
}





struct mpv_conn * MPV_CONN_INIT() {
  struct mpv_conn *conn = (struct mpv_conn *)malloc(sizeof(struct mpv_conn));
  conn->socket_path = "/opt/sdobox/mpv.socket";
  conn->connected = 0;
  conn->fdSelect = -1;
  // conn->fdSet;
  conn->timeout = (struct timeval){0};
  // conn->lastConn;
  // conn->cmdLock;
  conn->reqId = 1;
  conn->reqTop = 100000;
  conn->reqQueCnt = 0;
  
  return conn;
}

void MPV_CONN_DESTROY(struct mpv_conn *conn) {
  if (conn->fdSelect > -1) {
    mpv_socket_close(conn->fdSelect);
  }
}




int mpv_socket_conn(struct mpv_conn *conn, int bCancel) {
  int fd;
  // struct sockaddr_in addr;
  struct sockaddr_un addr;
  int socket_try = 0;
  int socket_retries = 100;

  // Wait for socket to arrive
  while (!bCancel && (socket_retries > -1 && socket_try < socket_retries) && access(conn->socket_path, R_OK) == -1) {
    dbgprintf(DBG_INFO, "Waiting to See %s\n", conn->socket_path);
    socket_try++;
    usleep(1000000);
  }

  if (access(conn->socket_path, R_OK) == -1) {
    dbgprintf(DBG_ERROR, "No Socket Available for Singlet %s\n", conn->socket_path);
    fd = -1;
    goto cleanup;
  }

  if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    dbgprintf(DBG_ERROR, "%s\n", "MPV Socket Error");
    fd = -1;
    goto cleanup;
  }

  // Set Socket Non-Blocking
  setnonblock(fd);

  memset(&addr, 0, sizeof(struct sockaddr_un));
  addr.sun_family = AF_UNIX;
  if (*conn->socket_path == '\0') {
    *addr.sun_path = '\0';
    strncpy(addr.sun_path+1, conn->socket_path+1, sizeof(addr.sun_path)-2);
  } else {
    strncpy(addr.sun_path, conn->socket_path, sizeof(addr.sun_path)-1);
  }

  if (connect(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
    dbgprintf(DBG_ERROR, "%s\n%s\n", "MPV Singlet Connect Error", strerror(errno));
    fd = -1;
    goto cleanup;
  }

 cleanup:
  return fd;
}

int mpv_fd_write(struct mpv_conn *conn, char *data, int bCancel) {

  // Check for reconnection
  if (mpv_fd_check(conn->fdSelect) > 0) {
    dbgprintf(DBG_MPV_WRITE, "%s\n", "Write Connecting");
    // mpv_socket_lastConn = millis();
    conn->fdSelect = mpv_socket_conn(conn, bCancel);
    if (conn->fdSelect == -1) {
      return conn->fdSelect;
    }

    /* Initialize the file descriptor set. */
    FD_ZERO (&conn->fdSet);
    FD_SET (conn->fdSelect, &conn->fdSet);
  }

  /* Initialize the timeout data structure. */
  conn->timeout.tv_sec = 2;
  conn->timeout.tv_usec = 0;

  dbgprintf(DBG_MPV_WRITE,
            (data[strlen(data)-1] == '\n' ? "mpvwrite: %s" : "mpvwrite: %s\n"),
            data);
  int writeSz = write(conn->fdSelect, data, strlen(data));
  if (writeSz != strlen(data)) {
    dbgprintf(DBG_ERROR, "Bad MPV Write\n%s\n", data);
    // conn->fdSelect = -1;
  }
  return conn->fdSelect;
}

int mpv_cmd(struct mpv_conn *conn, char *cmd_string, int bCancel) {
  int fdWrite = mpv_fd_write(conn, cmd_string, bCancel);

  free(cmd_string);
  return fdWrite;
}


/*
 * mpv_fmt_cmd -- like printf.  Takes a format string and variable arguments, formats
 * the message and sends via mpv_cmd.
 */
int mpv_fmt_cmd(struct mpv_conn *conn, char* fmt, ...) {
  va_list ap;
  char* p = NULL;
  int size = 0;

  va_start(ap, fmt);
  size = vsnprintf(p, size, fmt, ap);
  va_end(ap);

  if (size < 0)
    return -1;

  ++size;                       /* for '\0' */
  p = malloc(size);
  if (!p)
    return -1;

  va_start(ap, fmt);
  size = vsnprintf(p,size,fmt,ap);
  va_end(ap);

  if (size < 0) {
    free(p);
    return -1;
  }

  return mpv_cmd(conn, p, 0);
}

int mpv_set_prop_char(struct mpv_conn *conn, char* prop, char* prop_val) {
  return mpv_fmt_cmd(conn, "set %s %s\n", prop, prop_val);
}

int mpv_set_prop_int(struct mpv_conn *conn, char* prop, int prop_val) {
  return mpv_fmt_cmd(conn, "set %s %d\n", prop, prop_val);
}

int mpv_set_prop_double(struct mpv_conn *conn, char* prop, double prop_val) {
  return mpv_fmt_cmd(conn, "set %s %f\n", prop, prop_val);
}

int mpv_cmd_prop_val(struct mpv_conn *conn, char* cmd, char* prop, double prop_val) {
  return mpv_fmt_cmd(conn, "%s %s %f\n", cmd, prop, prop_val);
}

void mpv_quit(struct mpv_conn *conn) {
  // fbbg_stop();
  mpv_fmt_cmd(conn, "quit\n", 0);
  mpv_socket_close(conn->fdSelect);
}


/*
// https://mpv.io/manual/stable/#terminal-status-line
// The audio/video sync as A-V:  0.000. This is the difference between audio
// and video time. Normally it should be 0 or close to 0. If it's growing, it
// might indicate a playback problem. (avsync property.)
// Total A/V sync change, e.g. ct: -0.417. Normally invisible. Can show up if
// there is audio "missing", or not enough frames can be dropped. Usually this
// will indicate a problem. (total-avsync-change property.)
int check_avsync() {
  char* retPosition;
  int ret;
  ret = mpvSocketSinglet("avsync", &retPosition);
  // debug_print("Video Async: %s\n", retPosition);
  return ret;
}

int video_display_fps() {
  char* retFps;
  int ret;
  ret = mpvSocketSinglet("display-fps", &retFps);
  // debug_print("Video Display FPS: %s\n", retFps);
  char* pEnd;
  libMpvVideoInfo->fps = strtod(retFps, &pEnd);
  return ret;
}

int video_estimated_display_fps() {
  char* retFps;
  int ret;
  ret = mpvSocketSinglet("estimated-display-fps", &retFps);
  // debug_print("Video Estimated Display FPS: %s\n", retFps);
  return ret;
}
*/
