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
#include "mpv_singlet.h"
#include "mpv.h"

static pthread_mutex_t mpvSingletLock = PTHREAD_MUTEX_INITIALIZER;

struct mpv_any_u * MPV_ANY_U_NEW() {
  struct mpv_any_u * mpvu = (struct mpv_any_u*)malloc(sizeof(struct mpv_any_u));
  return mpvu;
}

void MPV_ANY_U_FREE(struct mpv_any_u *mpvu) {
  free(mpvu);
}







int mpvSocketSinglet(struct mpv_conn *conn, char* cmd, int bCancel, struct mpv_any_u** ret) {

  int errSuccess = 1;
  int result = -1;

  struct mpv_any_u *mpvu = MPV_ANY_U_NEW();
  conn->reqId = reqId;
  if (++reqId == reqTop) { reqId = 1; } // reset request ids

  char* cmd_tmp = "{\"command\":[%s],\"request_id\": %d}\n";
  size_t cmdlen = snprintf(NULL, 0, cmd_tmp, cmd, conn->reqId) + 1;
  char *data = (char*)malloc(cmdlen * sizeof(char));
  if (data == NULL) {
    dbgprintf(DBG_ERROR, "%s\n%s\n", "Error!, No Memory", strerror(errno));
    return -1;
  }
  snprintf(data, cmdlen, cmd_tmp, cmd, conn->reqId);

  dbgprintf(DBG_MPV_WRITE, "Data: %s\n", data);
  conn->fdSelect = mpv_fd_write(conn, data, 0);

  if (!conn->fdSelect) {
    dbgprintf(DBG_ERROR, "MC: %d\n", conn->fdSelect);
    goto cleanup;
  }


  while (!bCancel) {
    conn->timeout.tv_sec = 2;
    conn->timeout.tv_usec = 0;
    
    // Check for reconnection
    if (mpv_fd_check(conn->fdSelect) > 0) {
      conn->connected = 0;
      // mpv_socket_lastConn = millis();
      conn->fdSelect = mpv_socket_conn(conn, bCancel);

      /* Initialize the file descriptor set. */
      FD_ZERO (&conn->fdSet);
      FD_SET (conn->fdSelect, &conn->fdSet);
      conn->connected = 1;
    }
        
    /* select returns 0 if timeout, 1 if input available, -1 if error. */
    int selT = select(FD_SETSIZE, &conn->fdSet, NULL, NULL, &conn->timeout);
    if (selT == -1) {
      dbgprintf(DBG_ERROR, "%s\n%s\n","Error! Closing MPV Socket, SELECT -1", strerror(errno));
      mpv_socket_close(conn->fdSelect);
      conn->fdSelect = -1;
      goto cleanup;
    } else if (selT == 1) {
      char* mpv_rpc_ret = NULL;
      int rReqId = -1;
      int rc = sgetline(conn->fdSelect, &mpv_rpc_ret);
      if (rc > 0) {
        printf("Got: %s\n", mpv_rpc_ret);

        json_t *root;
        json_error_t error;
        root = json_loads(mpv_rpc_ret, 0, &error);
        if (json_is_object(root) != 1) {
          rReqId = -1;
        } else {
          // Successful Response With Data
          json_t *rReqObj = json_object_get(root, "request_id");
          if (json_is_integer(rReqObj) != 1) {
            // printf("No Request Event\n");
            rReqId = 0;
          } else {
            rReqId = json_integer_value(rReqObj);
          }
          json_decref(rReqObj);
        }

        errSuccess = 1;
        json_t *rError = json_object_get(root, "error");
        if (json_is_string(rError) != 1) {
          // printf("No Error String\n");
        } else {
          const char *strError = json_string_value(rError);
          errSuccess = strcmp(strError, "success");
          // free(&strError);
        }
        json_decref(rError);
        
        if (rReqId == conn->reqId && errSuccess == 0) {
          dbgprintf(DBG_MPV_READ, "mpvread %d:%d : '%s'\n", rReqId, conn->reqId, mpv_rpc_ret);
/*
          size_t retlen = snprintf(NULL, 0, "%s", mpv_rpc_ret) + 1;
          dbgprintf(DBG_MPV_READ, "Ret %ld\n", retlen);
          mpvu->ptr = (char*)malloc(retlen * sizeof(char));
          if (mpvu->ptr == NULL) {
            dbgprintf(DBG_ERROR, "%s\n%s\n", "Error!, No Memory", strerror(errno));
            goto cleanup;
          }
          int cpyrc = strlcpy(mpvu->ptr, mpv_rpc_ret, retlen);
          if (cpyrc == -1) {
            dbgprintf(DBG_ERROR, "Singlet Copy Error: %s\n", mpv_rpc_ret);
            goto cleanup;            
          }
*/
          json_t *rData = json_object_get(root, "data");
          if (json_is_object(rData) != 1) {
            switch(json_typeof(rData)) {
              case JSON_STRING:
              {
                mpvu->ptr = strdup(json_string_value(rData));
                result = 0;

                break;
              }
              case JSON_INTEGER:
              {
                int tInt = (double)json_integer_value(rData);
                mpvu->integer = tInt;
                size_t len = snprintf(NULL, 0, "%d", tInt) + 1;
                mpvu->ptr = (char*)malloc(len * sizeof(char));
                snprintf(mpvu->ptr, len, "%d", tInt);
                result = 0;

                break;
              }
              case JSON_REAL:
              {
                double tFloat = (double)json_number_value(rData);
                mpvu->floating = tFloat;
                size_t len = snprintf(NULL, 0, "%f", tFloat) + 1;
                mpvu->ptr = (char*)malloc(len * sizeof(char));
                snprintf(mpvu->ptr, len, "%f", tFloat);
                result = 0;
                
                break;
              }
              case JSON_TRUE:
              {
                mpvu->floating = 1.0;
                mpvu->integer = 1;
                mpvu->ptr = strdup("true");
                result = 0;
                
                break;
              }
              case JSON_FALSE:
              {
                mpvu->floating = 0.0;
                mpvu->integer = 0;
                mpvu->ptr = strdup("false");
                result = 0;
                
                break;
              }
              case JSON_OBJECT:
              case JSON_ARRAY:
              case JSON_NULL:
              {
                printf("OTHER\n");
                break;
              }
            }
          }
          *ret = mpvu;
          free(mpv_rpc_ret);
          json_decref(root);
          goto cleanup;

        // Error Response
        } else if (rReqId > conn->reqId && errSuccess != 0) {
          dbgprintf(DBG_MPV_READ|DBG_DEBUG,
                    "Error after requesting\n%s\n",
                    mpv_rpc_ret);
        } else {
          dbgprintf(DBG_MPV_READ, "mpvignore %d:%d : '%s'\n", rReqId, conn->reqId, mpv_rpc_ret);
        }
        free(mpv_rpc_ret);
        json_decref(root);
      } else {
        goto cleanup;
      }
    } else {
      dbgprintf(DBG_ERROR, "Singlet Unable to select FD: %d\n", selT);
      // goto cleanup;
    }
  }

 cleanup:
  free(data);
  // free(mpvu);
  return result;
}
