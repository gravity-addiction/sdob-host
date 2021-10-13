#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <mpv/client.h>

#include "../shared.h"
#include "./mpv_lib.h"

mpv_handle * mpv_handle_init() {
  mpv_handle *ctx = mpv_create();
  if (!ctx) {
      printf("failed creating context\n");
      return NULL;
  }
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
  mpv_set_option_string(ctx, "keep-open", "yes");

  int val = 1;
  mpv_set_option(ctx, "osc", MPV_FORMAT_FLAG, &val);

  // Some minor options can only be set before mpv_initialize().
  mpv_initialize(ctx);
  return ctx;
}

void mpv_handle_destroy(mpv_handle *ctx) {
  mpv_detach_destroy(ctx);
}

