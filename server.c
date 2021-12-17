#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <jansson.h> // JSON Parsing
#include <signal.h> // catching ctrl-c signal to quit
#include <time.h> // system time clocks
#include <mpv/client.h>

#include "./shared.h"
#include "./dbg/dbg.h"
#include "./zhelpers.h"
#include "./mpv/mpv_zeromq.h"
#include "./mpv/mpv_lib.h"


int m_bSigInt;
void signal_sigint(int sig);
int main(int argc, char* args[]);
static pthread_mutex_t quadfiveLock;

// SIGINT handler
 // can be called asynchronously
void signal_sigint(int sig)
{
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

// Thread for requesting video the timestamp every 1 second to publish on port :5555
void *mpvTimerThread(void * arguments)
{
  struct threadArgs *args = (struct threadArgs*)arguments;
  // Grab MPV Events, sent in JSON format
  char* lastMpvRet = NULL;
  while(!(*args->bCancel)) {
    char* mpvRet = mpv_get_property_string(args->mpvHandle, "time-pos");
    if (mpvRet != NULL && (lastMpvRet == NULL || strcmp(mpvRet, lastMpvRet) != 0)) {
      if (lastMpvRet != NULL) {
        free(lastMpvRet);
        lastMpvRet = NULL;
      }
      lastMpvRet = strdup(mpvRet);
      pthread_mutex_lock(&quadfiveLock);
      s_sendmore(args->quadfive, "timer");
      s_send(args->quadfive, mpvRet);
      pthread_mutex_unlock(&quadfiveLock);
    }
    usleep(1000000);
  }
  if (lastMpvRet != NULL) {
    free(lastMpvRet);
  }  
  dbgprintf(DBG_DEBUG, "%s\n", "Shutting Down Timer Thread");
  return 0;
}



int libmpv2_parse_msg(mpv_handle* mpvHandle, char* msg, int async, char** ret) {

  printf("Got Parse Message::%s--\n", msg);
  char * sep = ";";
  char * findSep = strstr(msg, sep);
  const char** result;
  if (findSep != NULL) {
    result = splitCSV(msg, sep);
    // free(msg);
  } else {
    result = malloc(sizeof(char*) * 2);
    result[0] = strdup(msg);
    result[1] = NULL;
  }

  int tCnt = 0;
  while(*result != 0) {
    ++result;
    tCnt++;
  }
  result = (result - tCnt);

  int getSet = 0; // 0 - not set, 1 - _get_prop, 2 - _set_prop
  int formatFlag = 0; // mpv_event enum
  char* name = NULL; // prop name, only for getSet = 2
  const char ** cmd = NULL; // data to send to mpv
  void* data;
  int dataInt;
  double dataDbl;

  if (tCnt > 1 && strcmp(*result, "get_prop_string") == 0) {
    getSet = 1;
    formatFlag = MPV_FORMAT_STRING;
    name = (char*)*(++result);
  } else if (tCnt > 1 && strcmp(*result, "get_prop_int") == 0) {
    getSet = 1;
    formatFlag = MPV_FORMAT_INT64;
    name = (char*)*(++result);
  } else if (tCnt > 1 && strcmp(*result, "get_prop_double") == 0) {
    getSet = 1;
    formatFlag = MPV_FORMAT_DOUBLE;
    name = (char*)*(++result);
  } else if (tCnt > 1 && strcmp(*result, "get_prop_flag") == 0) {
    getSet = 1;
    formatFlag = MPV_FORMAT_FLAG;
    name = (char*)*(++result);
  } else if (tCnt > 2 && strcmp(*result, "set_prop_string") == 0) {
    name = (char*)*(++result);
    getSet = 2;
    formatFlag = MPV_FORMAT_STRING;
    data = (char*)*(++result);
  } else if (tCnt > 2 && strcmp(*result, "set_prop_int") == 0) {
    name = (char*)*(++result);
    getSet = 2;
    formatFlag = MPV_FORMAT_INT64;
    dataInt = atoi((char*)*(++result));
    data = &dataInt;
  } else if (tCnt > 2 && strcmp(*result, "set_prop_double") == 0) {
    name = (char*)*(++result);
    getSet = 2;
    formatFlag = MPV_FORMAT_DOUBLE;
    dataDbl = strtod((char*)*(++result), NULL);
    data = &dataDbl;
  } else if (tCnt > 2 && strcmp(*result, "set_prop_flag") == 0) {
    name = (char*)*(++result);
    getSet = 2;
    formatFlag = MPV_FORMAT_FLAG;
    data = (void*)malloc(sizeof(int*) + 1);
    char *rFlag = (char*)*(++result);
    if (strcmp(rFlag, "true") == 0) {
      dataInt = 1;
      data = &dataInt;
    } else if (strcmp(rFlag, "false") == 0) {
      dataInt = 0;
      data = &dataInt;
    }
  // } else if (tCnt > 1 && strcmp(*result, "video") == 0) {

  } else {
    cmd = result;
  }

  // Echo input back out to quadfive
  if (getSet == 2 && name) {
    pthread_mutex_lock(&quadfiveLock);
    s_sendmore(quadfive, name);
    s_send(quadfive, (char*)*(result));
    pthread_mutex_unlock(&quadfiveLock);  
  }
// printf("Async: %d, getSet: %d\n", async, getSet);
  // Execute MPV Command
  int rcCmd = -1;
  if (async == 1) {
    // Assign next Request ID
    uint64_t nextId = nextRequestId();
    size_t nextLen = snprintf(NULL, 0, "%ld", nextId) + 1;
    char* nextStr = (char*)malloc(nextLen * sizeof(char));
    snprintf(nextStr, nextLen, "%ld", nextId);

    if (getSet == 1) {
      rcCmd = mpv_get_property_async(mpvHandle, nextId, name, formatFlag);
      printf("Sending Async Msg::%s--%lu-%d\n", name, nextId, formatFlag);
      
    } else if (getSet == 2) {
      rcCmd = mpv_set_property_async(mpvHandle, nextId, name, formatFlag, data);
    } else {
      printf("NextID: %lu\n", nextId);
      rcCmd = mpv_command_async(mpvHandle, nextId, cmd);
    }
    
    if (rcCmd > -1) {
      *ret = nextStr;
    } else {
      // free(nextStr);
      goto cleanup;
    }
  } else {
    if (getSet == 1) {
      void *dRet = NULL;
      char * snFlag = "%s";
      printf("Asking: %s\n", name);
      if (strcmp(name, "video-player") == 0) {
        printf("Send Video Setup\n");
        goto cleanup;
      }

      rcCmd = mpv_get_property(mpvHandle, name, formatFlag, &dRet);

      if (formatFlag == MPV_FORMAT_STRING) {
        snFlag = "%s";
        size_t dLen = snprintf(NULL, 0, snFlag, (char*)dRet) + 1;
        *ret = (char*)malloc(dLen * sizeof(char));
        snprintf(*ret, dLen, snFlag, (char*)dRet);

      } else if (formatFlag == MPV_FORMAT_INT64) {
        snFlag = "%ld";
        size_t dLen = snprintf(NULL, 0,snFlag, (int64_t*)&dRet) + 1;
        *ret = (char*)malloc(dLen * sizeof(char));
        snprintf(*ret, dLen, snFlag, (int64_t*)&dRet);

      } else if (formatFlag == MPV_FORMAT_DOUBLE) {
        snFlag = "%f";
        size_t dLen = snprintf(NULL, 0, snFlag, *(double*)&dRet) + 1;
        *ret = (char*)malloc(dLen * sizeof(char));
        snprintf(*ret, dLen, snFlag, *(double*)&dRet);
        
      } else if (formatFlag == MPV_FORMAT_FLAG) {
        if ((int*)&dRet) {
          *ret = strdup("true");
        } else {
          *ret = strdup("false");
        }
        // mpv_free(dRet);
        goto cleanup;
      }


      printf("Return Msg::%s--%d\n", *ret, formatFlag);
      // mpv_free(dRet);
    } else if (getSet == 2) {
      rcCmd = mpv_set_property(mpvHandle, name, formatFlag, data);
    } else {
      printf("Run Cmd: %s\n", data);
      rcCmd = mpv_command(mpvHandle, data);
    }
  }

cleanup:
  if (name != NULL) {
    // free(name);
  }
  //free(result);
  return rcCmd;
}




// ZeroMQ Thread for receiving raw mpv commands on port :5558
// no reply
void *mpvWriterThread(void * arguments)
{
  struct threadArgs *args = (struct threadArgs*)arguments;
  int rc;

  // Open :5558 for sync requests
  void *command_req = zmq_socket (args->context, ZMQ_REP);
  rc = zmq_bind (command_req, "tcp://*:5558");
  assert (rc == 0); 

  while(!(*args->bCancel)) {
    char msg[4096];
    int size = zmq_recv(command_req, msg, 4096 - 1, ZMQ_DONTWAIT);
    if (size > 0) {
      msg[size < 4096 ? size : 4096 - 1] = '\0';
      char* retData = NULL;
      int rcCmd = libmpv2_parse_msg(args->mpvHandle, msg, 0, &retData);
      printf("Got Msg: %s\n", msg);

      if (rcCmd > -1) {
        printf("Sending Reply: %s\n", retData);
        s_send(command_req, retData);
        if (retData != NULL) {
          // free(retData);
        }            
      } else {
        size_t errLen = snprintf(NULL, 0, "%d", rcCmd) + 1;
        char* rcStr = (char*)malloc(errLen * sizeof(char));
        snprintf(rcStr, errLen, "%d", rcCmd);

        s_send(command_req, rcStr);
        // free(rcStr);
      }

    } else {
      usleep(100);
    }

  }
  dbgprintf(DBG_DEBUG, "%s\n", "Shutting Down Writer Thread");

  zmq_close(command_req);

  return 0;
}




int main(int argc, char* args[])
{
  m_bSigInt = 0; // Exiting sigint
  m_bQuit = 0; // Global quitter flag, 1 to quit

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





  // ZeroMQ Context for entire process, including threads
  void *context = zmq_ctx_new ();
  pthread_t mpv_tid, timer_tid, writer_tid;
  int rc;

  // Open :5555 for publishing
  quadfive = zmq_socket(context, ZMQ_PUB);
  rc = zmq_bind(quadfive, "tcp://*:5555");
  assert (rc == 0);

  // Open :5559 for one-way requests
  void *command_raw = zmq_socket (context, ZMQ_PULL);
  rc = zmq_bind (command_raw, "tcp://*:5559");
  assert (rc == 0);

  // Create MPV Player Handle
  mpv_handle *mpvHandle = mpv_handle_init();

  // Assign pointers for multi-threads
  struct threadArgs threadpassArgs;
  threadpassArgs.mpvHandle = mpvHandle;
  threadpassArgs.quadfive = quadfive;
  threadpassArgs.context = context;
  threadpassArgs.bCancel = &m_bQuit;

  // Start Multithreads
  pthread_create(&mpv_tid, NULL, mpvZeroMQThread, (void *)&threadpassArgs);
  pthread_create(&timer_tid, NULL, mpvTimerThread, (void *)&threadpassArgs);
  pthread_create(&writer_tid, NULL, mpvWriterThread, (void *)&threadpassArgs);

  while (!m_bQuit) {
    char msg[4096];
    int size = zmq_recv(command_raw, msg, 4096 - 1, ZMQ_DONTWAIT);
    if (size > 0) {
      msg[size < 4096 ? size : 4096 - 1] = '\0';
      char* retData = NULL;
      int rcCmd = libmpv2_parse_msg(mpvHandle, msg, 1, &retData);
      printf("Raw Send: %d - %s\n%s\n", rcCmd, msg, retData);
    }
  }

  printf("Shutting Down Main\n");

  zmq_close (command_raw);
  zmq_close (quadfive);
  mpv_handle_destroy(mpvHandle);

  pthread_join(mpv_tid, NULL); 
  pthread_join(timer_tid, NULL);
  pthread_join(writer_tid, NULL);

  zmq_ctx_destroy (context);
  return m_bSigInt;
}
