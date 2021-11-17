#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h> // catching ctrl-c signal to quit
#include <time.h> // system time clocks
#include <zmq.h>
// #include "./shared.h"
#include "./dbg/dbg.h"
#include "./zhelpers.h"


char* mpvserver;
void *mpvTimerServer = NULL;
int mpvTimerThreadKill;
int mpvTimerThreadRunning;

int m_bQuit;
int m_bSigInt;
void signal_sigint(int sig);
int main(int argc, char* args[]);

struct threadArgs {
  void* context;
  int* bCancel;
};

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



// ------------------------
// SDOB MPV Timer Thread 
// ------------------------
void * mpvTimerThread(void *input) {
  if (mpvTimerThreadRunning) {
    dbgprintf(DBG_DEBUG, "%s\n", "Not Starting MPV Timer Thread, Already Started");
    pthread_exit(NULL);
  }

  mpvTimerThreadRunning = 1;
  
  if (mpvTimerThreadKill) {
    dbgprintf(DBG_DEBUG, "%s\n", "Not Starting MPV Timer Thread, Stop Flag Set");
    goto cleanup;
  }

  if (mpvserver == NULL) {
    dbgprintf(DBG_DEBUG, "%s\n", "Finding MPV Timer Server");
/*
    config_t cfg;
    config_init(&cfg);
    // Read the file. If there is an error, report it and exit.
    if (access(config_path, F_OK) == -1 || !config_read_file(&cfg, config_path)) {
      dbgprintf(DBG_DEBUG, "Cannot Find config_path: %s\n", config_path);
      config_destroy(&cfg);
      goto cleanup;
    }

    const char * retMpvServer;
    if (config_lookup_string(&cfg, "mpvserver", &retMpvServer)) {
      mpvserver = strdup(retMpvServer);
    } else {
      printf("No mpvserver configuration in ~/.config/sdobox/sdobox.conf\n");
      config_destroy(&cfg);
      goto cleanup;
    }
    config_destroy(&cfg);
    */
    mpvserver = strdup("tcp://flittermouse.local:5555");
  }

  dbgprintf(DBG_DEBUG, "Starting SkydiveOrBust Mpv Timer Thread: %s\n", mpvserver);
  int rc = 0;

  // Try starting mpv server subscription
  printf("Sending: --%s-- %d\n", mpvserver, ZMQ_SUB);
  rc = zmq_connect_socket(&mpvTimerServer, mpvserver, ZMQ_SUB);

  while (rc < 0 && !mpvTimerThreadKill) {
    zmq_close(mpvTimerServer);
    sleep(2);
    rc = zmq_connect_socket(&mpvTimerServer, mpvserver, ZMQ_SUB);
    printf("MPV Timer Connect: %d: %s\n", rc, mpvserver);
  }

  // Did connect, try initialize subscriptions
  if (rc > -1 && !mpvTimerThreadKill) {
    dbgprintf(DBG_DEBUG, "MPV Timer Connected, %s - %d\n", mpvserver, rc);
    const char *filterMpvTimer = "";
    rc = zmq_setsockopt (mpvTimerServer, ZMQ_SUBSCRIBE, filterMpvTimer, strlen(filterMpvTimer));

    // Failed to initialize subscriptions;
    if (rc < 0) {
      dbgprintf(DBG_DEBUG, "%s\n", "Cannot Subscribe to Mpv Timer Events");   
      printf("%s\n", "Shutdown SkydiveOrBust Mpv Timer Thread");
      goto cleanup;
    }
 
    // 0MQs sockets to poll
    zmq_pollitem_t items [] = {
      { mpvTimerServer, 0, ZMQ_POLLIN, 0 }
    };
    
    while (!mpvTimerThreadKill) {
      rc = zmq_poll (items, 1, -1);
      if (!m_bQuit && rc > -1) {  
        if (items[0].revents & ZMQ_POLLIN) {
          char *str = s_recv(mpvTimerServer);
          dbgprintf(DBG_DEBUG, "mpvTimerServer: %s\n", str);

          
          free(str);
        }
      }
    }
  } else {
    goto cleanup;
  }
  
 cleanup:
  zmq_close(mpvTimerServer);
  mpvTimerServer = NULL; 
  printf("%s\n", "Closing SkydiveOrBust Mpv Timer Thread");
  mpvTimerThreadRunning = 0;
  pthread_exit(NULL);
}


int mpvTimerThreadStart(pthread_t tid, struct threadArgs threadpassArgs) {
  dbgprintf(DBG_DEBUG, "%s\n", "mpvTimerThreadStart()");
  if (mpvTimerThreadRunning) { return 0; }

  // pg_sdob_pl_sliderForceUpdate = 1;
  dbgprintf(DBG_DEBUG, "SkydiveOrBust Mpv Timer Thread Spinup: %d\n", mpvTimerThreadRunning);
  mpvTimerThreadKill = 0;

  return pthread_create(&tid, NULL, &mpvTimerThread, &threadpassArgs);
}

void mpvTimerThreadStop() {
  dbgprintf(DBG_DEBUG, "%s\n", "mpvTimerThreadStop()");
  // Shutdown MPV FIFO Thread
  int shutdown_cnt = 0;
  if (mpvTimerThreadRunning) {
    mpvTimerThreadKill = 1;
    while (mpvTimerThreadRunning && shutdown_cnt < 20) {
      usleep(100000);
      shutdown_cnt++;
    }
  }
  dbgprintf(DBG_DEBUG, "SkydiveOrBust Mpv Timer Thread Shutdown %d\n", shutdown_cnt);
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
  pthread_t mpv_timer_tid;
  int rc;


  // Assign pointers for multi-threads
  struct threadArgs threadpassArgs;
  threadpassArgs.context = context;
  threadpassArgs.bCancel = &m_bQuit;

  mpvTimerThreadStart(mpv_timer_tid, threadpassArgs);
  // Start Multithreads
  // pthread_create(&mpv_tid, NULL, mpvZeroMQThread, (void *)&threadpassArgs);
  // pthread_create(&timer_tid, NULL, mpvTimerThread, (void *)&threadpassArgs);
  // pthread_create(&writer_tid, NULL, mpvWriterThread, (void *)&threadpassArgs);

  while (!m_bQuit) {
    usleep(10000);
  }

  printf("Shutting Down Main\n");

  // zmq_close (command_raw);
  // zmq_close (quadfive);
  // mpv_handle_destroy(mpvHandle);

  // pthread_join(mpv_tid, NULL); 
  // pthread_join(timer_tid, NULL);
  // pthread_join(writer_tid, NULL);

  zmq_ctx_destroy (context);
  return m_bSigInt;
}
