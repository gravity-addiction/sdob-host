#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <mpv/client.h>

#include "../shared.h"
#include "../zhelpers.h"
#include "./mpv_zeromq.h"

/*
MPV_EVENT_NONE              = 0
MPV_EVENT_SHUTDOWN          = 1
MPV_EVENT_LOG_MESSAGE       = 2
MPV_EVENT_GET_PROPERTY_REPLY = 3
MPV_EVENT_SET_PROPERTY_REPLY = 4
MPV_EVENT_COMMAND_REPLY     = 5
MPV_EVENT_START_FILE        = 6
MPV_EVENT_END_FILE          = 7
MPV_EVENT_FILE_LOADED       = 8
MPV_EVENT_CLIENT_MESSAGE    = 16
MPV_EVENT_VIDEO_RECONFIG    = 17
MPV_EVENT_AUDIO_RECONFIG    = 18
MPV_EVENT_SEEK              = 20
MPV_EVENT_PLAYBACK_RESTART  = 21
MPV_EVENT_PROPERTY_CHANGE   = 22
MPV_EVENT_QUEUE_OVERFLOW    = 24
MPV_EVENT_HOOK              = 25
*/

void mpvZeroMQSendEventName(void *zmq_conn, mpv_event *event) {
  if (event->event_id > 0) {
    char *event_name = (char*)mpv_event_name(event->event_id);
    printf("Got Other! %d - %s\n", event->event_id, event_name);
    s_sendmore(zmq_conn, "event");
    s_send(zmq_conn, event_name);
  }
}

void mpvZeroMQSendVideoInfo(void *mpvHandle, void *zmq_conn)
{
  // Duration
  char* mpvDur = mpv_get_property_string(mpvHandle, "duration");
  if (mpvDur != NULL) {
    size_t dlen = snprintf(NULL, 0, "duration,%s", mpvDur) + 1;
    char *data = (char*)malloc(dlen * sizeof(char));
    snprintf(data, dlen, "duration,%s", mpvDur);
    s_send(zmq_conn, data);
    mpv_free(mpvDur);
  }

  // is_loaded

  // is_player

  // is_seeking

  // fps

  // pbrate
  /*
  double mpvSpeed = 1.0;
  if (mpv_get_property(mpvHandle, "speed", MPV_FORMAT_DOUBLE, &mpvSpeed) < 0) {
    dbgprintf(DBG_ERROR, "%s\n", "Cannot get mpv property speed");
  }
  size_t dlen = snprintf(NULL, 0, "speed,%f", mpvSpeed) + 1;
  char *data = (char*)malloc(dlen * sizeof(char));
  snprintf(data, dlen, "speed,%f", mpvSpeed);
  s_send(zmq_conn, data);
  */
  // folder

  // file
  char* mpvFilename = mpv_get_property_string(mpvHandle, "filename");
  if (mpvFilename != NULL) {
    size_t dlen = snprintf(NULL, 0, "filename,%s", mpvFilename) + 1;
    char *data = (char*)malloc(dlen * sizeof(char));
    snprintf(data, dlen, "filename,%s", mpvFilename);
    s_send(zmq_conn, data);
    mpv_free(mpvFilename);
  }
}

void *mpvZeroMQThread(void * arguments)
{
  
  struct threadArgs *args = (struct threadArgs*)arguments;
  int rc;

  void *command_events = zmq_socket(args->context, ZMQ_PUB);
  rc = zmq_bind (command_events, "tcp://*:5556");

  while(!(*args->bCancel)) {
    mpv_event *event = mpv_wait_event(args->mpvHandle, 2);
    switch (event->event_id) {
      case MPV_EVENT_SET_PROPERTY_REPLY: {

        printf("Prop Reply: %d\n", event->error);
        // mpv_node node = (*(mpv_node *)event->data);
      }
      break;
      case MPV_EVENT_COMMAND_REPLY:
      case MPV_EVENT_GET_PROPERTY_REPLY: {
        mpv_node node = (*(mpv_node *)event->data);
        mpv_event_property *prop = event->data;
        uint64_t eFormat = prop->format;
        char* snFlag = "%s";
        size_t strLen;
        char *strReply = NULL;

        printf("Reply %ld - %d - %ld\n", event->reply_userdata, event->error, eFormat); // *(double *)event->data);
        switch (eFormat) {
          case MPV_FORMAT_NONE:
            //printf("None Reply %ld - %d - %d\n", event->reply_userdata, event->error, (*(char *)event->data));
          break;

          case MPV_FORMAT_STRING:
          case MPV_FORMAT_OSD_STRING:
          case MPV_FORMAT_FLAG:
            printf("String Reply %d - %s\n", event->error, *(char **)prop->data);
            snFlag = "reply,%d,%s";
            strLen = snprintf(NULL, 0, snFlag, event->reply_userdata, *(char **)prop->data) + 1;
            strReply = (char*)malloc(strLen * sizeof(char *));
            snprintf(strReply, strLen, snFlag, event->reply_userdata, *(char **)prop->data);
            s_sendmore(command_events, "event");
            s_send(command_events, strReply);
          break;

          case MPV_FORMAT_INT64:
            printf("Int Reply %ld - %d - %ld\n", event->reply_userdata, event->error, *(int64_t*)prop->data);
            snFlag = "reply,%d,%ld";
            strLen = snprintf(NULL, 0, snFlag, event->reply_userdata, *(int64_t*)prop->data) + 1;
            strReply = (char*)malloc(strLen * sizeof(char *));
            snprintf(strReply, strLen, snFlag, event->reply_userdata, *(int64_t*)prop->data);
            s_sendmore(command_events, "event");
            s_send(command_events, strReply);            
          break;

          case MPV_FORMAT_DOUBLE: {
            printf("Dbl Reply %ld - %d - %f\n", event->reply_userdata, event->error, *(double*)prop->data);
            snFlag = "reply,%d,%f";
            strLen = snprintf(NULL, 0, snFlag, event->reply_userdata, *(double*)prop->data) + 1;
            strReply = (char*)malloc(strLen * sizeof(char *));
            snprintf(strReply, strLen, snFlag, event->reply_userdata, *(double*)prop->data);
            s_sendmore(command_events, "event");
            s_send(command_events, strReply);   
          }
          break;

          case MPV_FORMAT_NODE: {
            printf("Noded %ld - Err: %d\n", event->reply_userdata, event->error);
            if (event->error == 0) {
              printf("Good: %s\n", *(char **)prop->data);
            }
          }
          break;
          case MPV_FORMAT_NODE_ARRAY:
          case MPV_FORMAT_NODE_MAP:
          case MPV_FORMAT_BYTE_ARRAY:
            printf("Other Reply %ld - %d\n%s: %s\n", event->reply_userdata, event->error, *(char **)event->data, *(char **)prop->data);
          break;
        }
      }
      break;
      case MPV_EVENT_IDLE: {
        // Send standard event name
        mpvZeroMQSendEventName(command_events, event);
        /*const char *cmd[] = {"loadfile", "/home/vid/3001_1.MTS", NULL};
        uint64_t retVal = 1;
        rc = mpv_command_async(args->mpvHandle, retVal, cmd);

        retVal = 2;
        rc = mpv_get_property_async(args->mpvHandle, retVal, "volume", MPV_FORMAT_DOUBLE);
        printf("Prop %d\n", rc);
        */
      }
      break;
      case MPV_EVENT_SHUTDOWN:
        // Send standard event name
        mpvZeroMQSendEventName(command_events, event);
        printf("Shutdown\n");
        (*args->bCancel) = 1;

      break;
      case MPV_EVENT_FILE_LOADED: {
        // Send standard event name
        mpvZeroMQSendEventName(command_events, event);
        mpvZeroMQSendVideoInfo(args->mpvHandle, command_events);

      }
      break;
      default: {
        mpvZeroMQSendEventName(command_events, event);
      }
      break;
    }
  }

shutdown:
  dbgprintf(DBG_DEBUG, "%s\n", "Shutting Down MPV Thread");  
  zmq_close(command_events);
  return 0;
}



