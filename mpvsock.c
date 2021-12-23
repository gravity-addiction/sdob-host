/*
 * This program fragment sends a datagram to a receiver whose
 * name is retrieved from the command line arguments.  The form 
 * of the command line is udgramsend pathname.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include "mpv/mpv_singlet.h"



int main(argc, argv)
   int argc;
   char *argv[];
{
  mpv_init();
  struct mpv_conn * conn = MPV_CONN_INIT();
  char* retFps;
  int ret;
  ret = mpvSocketSinglet(conn, "time-pos", 0, &retFps);
}