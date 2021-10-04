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


int mpvSocketSinglet(struct mpv_conn *conn, char* cmd, int bCancel, char** ret) {

  conn->reqId = nextRequestId();
  int errSuccess = 1;
  int result = -1;
  char* ret2;

  char* cmd_tmp = "{\"command\":[%s],\"request_id\": %d}\n";
  size_t cmdlen = snprintf(NULL, 0, cmd_tmp, cmd, conn->reqId) + 1;
  char *data = (char*)malloc(cmdlen * sizeof(char));
  if (data == NULL) {
    dbgprintf(DBG_ERROR, "%s\n%s\n", "Error!, No Memory", strerror(errno));
    return -1;
  }
  snprintf(data, cmdlen, cmd_tmp, cmd, conn->reqId);

  // dbgprintf(DBG_MPV_WRITE, "Data: %s\n", data);
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
      dbgprintf(DBG_MPV_WRITE, "%s\n", "Reconnecting singlet");
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
        dbgprintf(DBG_MPV_READ, "Raw: %s\n", mpv_rpc_ret);

        json_t *root;
        json_error_t error;
        json_t *rReqObj;
        json_t *rError;
        root = json_loads(mpv_rpc_ret, 0, &error);
        
        if (json_is_object(root) != 1) {
          dbgprintf(DBG_ERROR, "Bad Root: %s\n", mpv_rpc_ret);
          rReqId = -1;
        } else {
          // Successful Response With Data
          rReqObj = json_object_get(root, "request_id");
          if (json_is_integer(rReqObj)) {
            rReqId = json_integer_value(rReqObj);
          }
        }
  
        errSuccess = 1;
        rError = json_object_get(root, "error");
        if (json_is_string(rError) != 1) {
          // printf("No Error String\n");
        } else {
          const char *strError = json_string_value(rError);
          errSuccess = strcmp(strError, "success");
        }

        if (rReqId == conn->reqId && errSuccess == 0) {
          dbgprintf(DBG_MPV_READ, "mpvread %d:%d : '%s'\n", rReqId, conn->reqId, mpv_rpc_ret);

          json_t *rData = json_object_get(root, "data");
          if (rData == NULL) {
            size_t len = snprintf(NULL, 0, "%s", mpv_rpc_ret) + 1;
            ret2 = (char*)malloc(len * sizeof(char));
            snprintf(ret2, len, "%s", mpv_rpc_ret);
            result = 0;

          } else if (json_is_object(rData) != 1) {

            switch(json_typeof(rData)) {
              case JSON_STRING:
              {
                // printf("S\n");
                const char *jsonStr = json_string_value(rData);
                size_t len = snprintf(NULL, 0, "%s", jsonStr) + 1;
                ret2 = (char*)malloc(len * sizeof(char));
                snprintf(ret2, len, "%s", jsonStr);        
                result = 0;
                break;
              }
              case JSON_INTEGER:
              {
                // printf("I\n");
                int tInt = (double)json_integer_value(rData);
                size_t len = snprintf(NULL, 0, "%d", tInt) + 1;
                ret2 = (char*)malloc(len * sizeof(char));
                snprintf(ret2, len, "%d", tInt);
                result = 0;
                break;
              }
              case JSON_REAL:
              {
                // printf("R\n");
                double tFloat = (double)json_number_value(rData);
                size_t len = snprintf(NULL, 0, "%f", tFloat) + 1;
                ret2 = (char*)malloc(len * sizeof(char));
                snprintf(ret2, len, "%f", tFloat);
                result = 0;
                break;
              }
              case JSON_TRUE:
              {
                // printf("T\n");
                size_t len = snprintf(NULL, 0, "%s", "true") + 1;
                ret2 = (char*)malloc(len * sizeof(char));
                snprintf(ret2, len, "%s", "true");    
                result = 0;
                break;
              }
              case JSON_FALSE:
              {
                // printf("F\n");
                size_t len = snprintf(NULL, 0, "%s", "false") + 1;
                ret2 = (char*)malloc(len * sizeof(char));
                snprintf(ret2, len, "%s", "false");    
                result = 0;
                break;
              }
              case JSON_OBJECT:
              case JSON_ARRAY:
              case JSON_NULL:
              {
                // printf("OTHER\n");
                result = 2;
                break;
              }
              default:
              {
                result = 3;
                break;
              }
            }
 
          } else {
            // data: is an object, send back entire data string
            ret2 = json_dumps(rData, JSON_COMPACT);
            result = 0;
          }

        // Error Response
        } else if (rReqId == conn->reqId && errSuccess != 0) {
          dbgprintf(DBG_MPV_READ|DBG_DEBUG,
                    "Error after requesting\n%s\n",
                    mpv_rpc_ret);
          result = 1;
    
        } else {
          dbgprintf(DBG_MPV_READ, "mpvignore %d:%d : '%s'\n", rReqId, conn->reqId, mpv_rpc_ret);
        }

        mpvclean:
          free(mpv_rpc_ret);
          json_decref(rReqObj);
          json_decref(rError);
          json_decref(root);
          if (result > -1) {
            goto cleanup;
          }
      } else {
        dbgprintf(DBG_ERROR, "Cannot Parse JSON: %s\n", mpv_rpc_ret);
      }
      
    } else {
      dbgprintf(DBG_ERROR, "Singlet Unable to select FD: %d\n", selT);
      conn->fdSelect = -1;
      conn->connected = 0;
      // goto cleanup;
    }
  }

 cleanup:
  free(data);
  *ret = ret2;
  return result;
}
