#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <mpv/client.h>

#include "./shared.h"
#include "./dbg/dbg.h"
#include "./zhelpers.h"

int m_bSigInt;
void signal_sigint(int sig);
int main(int argc, char* argv[]);


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



int main(int argc, char* argv[])
{
  int rc;

  m_bSigInt = 0; // Exiting sigint
  m_bQuit = 0; // Global quitter flag, 1 to quit
  reqId = 1; // MPV request_id counter
  reqTop = 2147483647; // MPV request_id maximum

  // Debug printing support
  init_dbg();

  // Register Signals
  signal(SIGINT, signal_sigint);
  signal(SIGTERM, signal_sigint);

printf("XX\n");
  mpv_handle *ctx = mpv_create();
  if (!ctx) {
      printf("failed creating context\n");
      return 1;
  }
printf("YY\n");
  // Enable default key bindings, so the user can actually interact with
  // the player (and e.g. close the window).
  mpv_set_option_string(ctx, "input-default-bindings", "yes");
  mpv_set_option_string(ctx, "input-vo-keyboard", "yes");
  mpv_set_option_string(ctx, "deband", "no");
  mpv_set_option_string(ctx, "interpolation", "no");
  mpv_set_option_string(ctx, "vo", "gpu");
  mpv_set_option_string(ctx, "hwdec", "vaapi");
  mpv_set_option_string(ctx, "hwdec-codecs", "all");
  mpv_set_option_string(ctx, "dscale", "bilinear");
  mpv_set_option_string(ctx, "opengl-pbo", "yes");
  mpv_set_option_string(ctx, "scale", "bilinear");
  mpv_set_option_string(ctx, "video-sync", "display-resample");
  mpv_set_option_string(ctx, "vd-lavc-dr", "yes");
  mpv_set_option_string(ctx, "fullscreen", "yes");
  int val = 1;
  mpv_set_option(ctx, "osc", MPV_FORMAT_FLAG, &val);
printf("ZZ\n");
  // Some minor options can only be set before mpv_initialize().
  rc = mpv_initialize(ctx);
printf("Here - %d\n", rc);

  // Play this file.
  const char *cmd[] = {"loadfile", argv[1], NULL};
  rc = mpv_command(ctx, cmd);
printf("Cmd - %d\n%s\n", rc, *cmd);

  while (!m_bQuit) {
    mpv_event *event = mpv_wait_event(ctx, 10000);
    printf("event: %s\n", mpv_event_name(event->event_id));
    if (event->event_id == MPV_EVENT_SHUTDOWN)
        break;
  }
  
  //pthread_join(mpv_tid, NULL);

  mpv_detach_destroy(ctx);

  printf("Controls are yours!\n");

  return m_bSigInt;
}
