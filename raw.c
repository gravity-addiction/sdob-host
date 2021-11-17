#include "zhelpers.h"

int main (int argc, char *argv [])
{
    if (argc > 1) {
      //  Socket to talk to server
      void *context = zmq_ctx_new ();
      void *subscriber = zmq_socket(context, ZMQ_PUSH);
      int rc = zmq_connect (subscriber, "tcp://flittermouse.local:5559");
      assert (rc == 0);
      // s_sleep(2000);
      printf("SENDING\n%s", argv[1]);
      // rc = s_send (subscriber, "\"get_property\", \"time-pos\"");
      rc = s_send (subscriber, argv[1]);
printf("Done %d\n", rc);
      zmq_close (subscriber);
      zmq_ctx_destroy (context);
    }
    return 0;
}
