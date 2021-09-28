#include <unistd.h>
#include <signal.h> // catching ctrl-c signal to quit
#include <pthread.h> // threading
#include <time.h> // system time clocks
#include <jansson.h> // JSON Parsing
#include "zhelpers.h"
#include "mpv/mpv_singlet.h"
#include "mpv/mpv.h"
#include "shared.h"



// struct threadArgs mpvSocketCmdStatus;

int m_bSigInt;
void signal_sigint(int sig);
void *mpvThread(void * args);
void *mpvTimerThread(void * args);
int main(int argc, char* args[]);

// SIGINT handler
void signal_sigint(int sig) { // can be called asynchronously
  printf("SIGINT: %d current m_bQuit value = %d\n", sig, m_bQuit);
  if (m_bQuit > 1) {
    dbgprintf(DBG_DEBUG, "Forcibly exiting now!\n");
    _exit(1);                   /* do NOT call atexit functions */
  } else if (m_bQuit > 0) {
    dbgprintf(DBG_DEBUG, "Turning up the shutdown heat...\n");
    ++m_bQuit;
  } else {
    dbgprintf(DBG_DEBUG, "Requesting shut-down...\n");
    m_bSigInt = sig;
    m_bQuit = 1;
  }
}









void *mpvThread(void * arguments)
{
  struct threadArgs *args = (struct threadArgs*)arguments;
  int bCancel = (int)args->bCancel;
  //  Prepare our context and publisher
  int rc;
  int runLoop = 0;

  void *publisher = zmq_socket (args->context, ZMQ_PUB);
  rc = zmq_bind (publisher, "tcp://*:5556");
  assert (rc == 0);

  struct mpv_conn *mpvEventsConn = MPV_CONN_INIT();

  while (!bCancel && !mpvEventsConn->connected) {
    dbgprintf(DBG_MPV_WRITE, "mpvThread Using Socket: %s\n", mpvEventsConn->socket_path);
    if ((mpvEventsConn->fdSelect = mpv_socket_conn(mpvEventsConn, bCancel)) == -1) {
      dbgprintf(DBG_ERROR, "%s\n", "MPV Socket Error");
      usleep(1000000);      
    } else {
      dbgprintf(DBG_MPV_WRITE, "MPV Socket Connected\n");
      mpvEventsConn->connected = 1;
      runLoop = 1;
    }
  }

  // Grab MPV Events, sent in JSON format
  while(!bCancel && runLoop) {
    if (!fd_is_valid(mpvEventsConn->fdSelect)) {
      dbgprintf(DBG_MPV_WRITE, "MPV Events Sockets Re-Connect\n");
      // try closing fd
      if (mpvEventsConn->fdSelect) { close(mpvEventsConn->fdSelect); }
      // reconnect fd
      if ((mpvEventsConn->fdSelect = mpv_socket_conn(mpvEventsConn, bCancel)) == -1) {
        // MPV Connect Error
        printf("Sleep\n");
        usleep(2000000);
      }
    }

    // Grab Next Socket Line
    char* mpv_event_ret;
    rc = sgetline(mpvEventsConn->fdSelect, &mpv_event_ret);
    if (rc > 0) {

      json_t *root;
      json_error_t error;
      root = json_loads(mpv_event_ret, 0, &error);
      if (json_is_object(root) != 1) {
        dbgprintf(DBG_ERROR, "JSON Object: %s\n", mpv_event_ret);
      } else {
        
        // Successful Response With Data
        json_t *rReqObj = json_object_get(root, "request_id");
        if (json_is_integer(rReqObj)) {
          int rReqId = json_integer_value(rReqObj);
          printf("Has Request ID %d\n", rReqId);
          int qReqI = -1;

          // Find Index
          for (int reqI = 0; reqI < mpvEventsConn->reqQueCnt; reqI++) {
            if (mpvEventsConn->reqQueI[reqI] == rReqId) {
              qReqI = reqI;
              break;
            }
          }
          
          printf("Has Request Index %d - %s\n", qReqI, mpvEventsConn->reqQue[qReqI]);
          json_t *rData = json_object_get(root, "data");
          if (json_is_object(rData) != 1) {
            switch(json_typeof(rData)) {
              case JSON_STRING:
              {
                char* tString = strdup(json_string_value(rData));
                size_t len = snprintf(NULL, 0, "%s,%s", mpvEventsConn->reqQue[qReqI], tString) + 1;
                char* reqSendData = (char*)malloc(len * sizeof(char));
                snprintf(reqSendData, len, "%s,%s", mpvEventsConn->reqQue[qReqI], tString);
                free(tString);
                
                CLEAR(mpvEventsConn->reqQue[qReqI], 32);
                mpvEventsConn->reqQueI[qReqI] = -1;
                mpvEventsConn->reqQueCnt--;

                rc = s_send (publisher, reqSendData);
                free(reqSendData);
                break;
              }
              case JSON_INTEGER:
              {
                int tInt = (double)json_integer_value(rData);
                size_t len = snprintf(NULL, 0, "%s,%d", mpvEventsConn->reqQue[qReqI], tInt) + 1;
                char* reqSendData = (char*)malloc(len * sizeof(char));
                snprintf(reqSendData, len, "%s,%d", mpvEventsConn->reqQue[qReqI], tInt);
                
                CLEAR(mpvEventsConn->reqQue[qReqI], 32);
                mpvEventsConn->reqQueI[qReqI] = -1;
                mpvEventsConn->reqQueCnt--;

                rc = s_send (publisher, reqSendData);
                free(reqSendData);
                break;
              }
              case JSON_REAL:
              {
                double tFloat = (double)json_number_value(rData);
                size_t len = snprintf(NULL, 0, "%s,%f", mpvEventsConn->reqQue[qReqI], tFloat) + 1;
                char* reqSendData = (char*)malloc(len * sizeof(char));
                snprintf(reqSendData, len, "%s,%f", mpvEventsConn->reqQue[qReqI], tFloat);
                CLEAR(mpvEventsConn->reqQue[qReqI], 32);
                mpvEventsConn->reqQueI[qReqI] = -1;
                mpvEventsConn->reqQueCnt--;

                rc = s_send (publisher, reqSendData);
                free(reqSendData);
                break;
              }
              case JSON_TRUE:
              {
                size_t len = snprintf(NULL, 0, "%s,1", mpvEventsConn->reqQue[qReqI]) + 1;
                char* reqSendData = (char*)malloc(len * sizeof(char));
                snprintf(reqSendData, len, "%s,1", mpvEventsConn->reqQue[qReqI]);
                CLEAR(mpvEventsConn->reqQue[qReqI], 32);
                mpvEventsConn->reqQueI[qReqI] = -1;
                mpvEventsConn->reqQueCnt--;

                rc = s_send (publisher, reqSendData);
                free(reqSendData);
                break;
              }
              case JSON_FALSE:
              {
                size_t len = snprintf(NULL, 0, "%s,0", mpvEventsConn->reqQue[qReqI]) + 1;
                char* reqSendData = (char*)malloc(len * sizeof(char));
                snprintf(reqSendData, len, "%s,0", mpvEventsConn->reqQue[qReqI]);
                CLEAR(mpvEventsConn->reqQue[qReqI], 32);
                mpvEventsConn->reqQueI[qReqI] = -1;
                mpvEventsConn->reqQueCnt--;

                rc = s_send (publisher, reqSendData);
                free(reqSendData);
                break;
              }
              case JSON_OBJECT:
              case JSON_ARRAY:
              case JSON_NULL:
              {
                printf("OTHER\n");
                CLEAR(mpvEventsConn->reqQue[qReqI], 32);
                mpvEventsConn->reqQueI[qReqI] = -1;
                mpvEventsConn->reqQueCnt--;                
                break;
              }
            }
          }
          json_decref(rData);
        }
        json_decref(rReqObj);


        json_t *rEvent = json_object_get(root, "event");
        if (json_is_string(rEvent)) {
          const char *strEventData = json_string_value(rEvent);
          char* strEvent = strdup(strEventData);
          printf("Event: %s\n", strEvent);
          if (strcmp(strEvent, "file-loaded") == 0) {
            if (++mpvEventsConn->reqId == mpvEventsConn->reqTop) { mpvEventsConn->reqId = 1; } // reset request ids

            // Find Slot
            int qReqI = 0;
            for (int reqI = 0; reqI < mpvEventsConn->reqQueCnt; reqI++) {
              if (mpvEventsConn->reqQueI[reqI] == -1) {
                qReqI = reqI;
                break;
              }
            }

            mpvEventsConn->reqQueI[qReqI] = mpvEventsConn->reqId;
            strlcpy(mpvEventsConn->reqQue[qReqI], "duration", 32);
            mpvEventsConn->reqQueCnt++;

            char* cmd = "\"get_property\", \"duration\"";
            char* cmd_tmp = "{\"command\":[%s],\"request_id\": %d}\n";
            size_t cmdlen = snprintf(NULL, 0, cmd_tmp, cmd, mpvEventsConn->reqId) + 1;
            char *data = (char*)malloc(cmdlen * sizeof(char));
            if (data == NULL) {
              dbgprintf(DBG_ERROR, "%s\n%s\n", "Error!, No Memory", strerror(errno));
              pthread_mutex_unlock(&mpvEventsConn->cmdLock);
            } else {
              snprintf(data, cmdlen, cmd_tmp, cmd, mpvEventsConn->reqId);
              mpvEventsConn->fdSelect = mpv_fd_write(mpvEventsConn, data, bCancel);
              free(data);
            }
          }
          // Send to clients
          rc = s_send (publisher, strEvent);
          free(strEvent);

        }
        json_decref(rEvent);
      }
      json_decref(root);

      free(mpv_event_ret);
    }
    else {
      // Nothing to Do, Sleep for a moment
      usleep(200000);
    }
  }

  zmq_close (publisher);
  return 0;
}






void *mpvTimerThread(void * arguments)
{
  struct threadArgs *args = (struct threadArgs*)arguments;
  //  Prepare our context and publisher
  int rc;
  int runLoop = 1;

  void *timer = zmq_socket(args->context, ZMQ_PUB);
  rc = zmq_bind(timer, "tcp://*:5555");
  assert (rc == 0);

  struct mpv_conn *mpvTimerConn = MPV_CONN_INIT();
  // Grab MPV Events, sent in JSON format
  while(!(*args->bCancel) && runLoop) {
    struct mpv_any_u* mpvPlaybackMpvu;
    rc = mpvSocketSinglet(mpvTimerConn, "\"get_property\", \"playback-time\"", m_bQuit, &mpvPlaybackMpvu);
    s_send(timer, mpvPlaybackMpvu->ptr);
    if (rc == 0) {
      free(mpvPlaybackMpvu->ptr);
      free(mpvPlaybackMpvu);
    } else {
      printf("Bad Timer Reply\n");
    }
    usleep(1000000);
  }
  printf("Shutting Down Timer Thread\n");

  mpv_socket_close(mpvTimerConn->fdSelect);
  MPV_CONN_DESTROY(mpvTimerConn);
  free(mpvTimerConn);
  
  zmq_close(timer);

  printf("Shut Down Timer Thread\n");
  return 0;
}





/*
void mpvMainSocket_Callback(struct mpv_conn *timer, struct mpv_any_u* mpv_time) {
  printf("Go CB! %s\n", (char*)mpv_time->ptr);
  s_send(timer, mpv_time->ptr);
  free(mpv_time->ptr);
  free(mpv_time);
}
*/
int main(int argc, char* args[])
{
  m_bSigInt = 0;
  m_bQuit = 0;

  // Debug printing support
  init_dbg();

  // Register Signals
  signal(SIGINT, signal_sigint);
  signal(SIGTERM, signal_sigint);

  // For running as a daemon
  // if (sigignore(SIGHUP)) {
  //   dbgprintf(DBG_ERROR, "attempt to ignore SIGHUP failed: %s\n", strerror(errno));
  //   abort();
  // }

  void *context = zmq_ctx_new ();
  pthread_t mpv_tid, timer_tid, singlet_tid;
  int rc;

  // Open :5557 for two-way requests
  void *command_rep = zmq_socket (context, ZMQ_REP);
  rc = zmq_bind (command_rep, "tcp://*:5557");
  assert (rc == 0);


  struct mpv_any_u* retClient;
  struct mpv_conn *conn;
  conn = MPV_CONN_INIT();
  rc = mpvSocketSinglet(conn, "\"client_name\"", m_bQuit, &retClient);
  if (rc == 0) {
    printf("Client Name: %s\n", (char*)retClient->ptr);
    free(retClient->ptr);
    free(retClient);
  }
  mpv_socket_close(conn->fdSelect);
  MPV_CONN_DESTROY(conn);
  free(conn);
  
  struct threadArgs threadpassMpv;
  threadpassMpv.context = context;
  threadpassMpv.bCancel = &m_bQuit;
  
  struct threadArgs threadpassTimer;
  threadpassTimer.context = context;
  threadpassTimer.bCancel = &m_bQuit;

  // pthread_create(&mpv_tid, NULL, mpvThread, (void *)&threadpassMpv);
  pthread_create(&timer_tid, NULL, mpvTimerThread, (void *)&threadpassTimer);
  
  while (!m_bQuit) {
    // 0MQs sockets to poll
    zmq_pollitem_t items [] = {
      { command_rep, 0, ZMQ_POLLIN, 0 }
    };

    // Poll Loop
    int rc = zmq_poll (items, 1, -1);
    if (!m_bQuit && rc > -1) {  
      if (items[0].revents & ZMQ_POLLIN) {
        char *msg = s_recv (command_rep);
        if (msg) {
          struct mpv_any_u* mpvRetMpvu;
          struct mpv_conn *conn = MPV_CONN_INIT();
          int cplen = mpvSocketSinglet(conn, msg, m_bQuit, &mpvRetMpvu);
          if (cplen == -1) {
            s_send(command_rep, "{\"error\": \"bad request\"}");
          } else {
            s_send(command_rep, (char*)mpvRetMpvu->ptr);
          }
          mpv_socket_close(conn->fdSelect);
          MPV_CONN_DESTROY(conn);
          free(conn);
        }
        free(msg);
      }
    }
  }
  zmq_close (command_rep);

  // pthread_join(mpv_tid, NULL);
  pthread_join(timer_tid, NULL);

  // free(threadpassMpv);
  // free(threadpassTimer);
  // free(threadpassSinglet);

  zmq_ctx_destroy (context);
  return m_bSigInt;
}
