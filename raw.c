#include "zhelpers.h"

int main (int argc, char *argv [])
{
    if (argc > 1) {
      //  Socket to talk to server
      void *context = zmq_ctx_new ();
      void *subscriber = zmq_socket (context, ZMQ_REQ);
      int rc = zmq_connect (subscriber, "tcp://192.168.126.85:5558");
      assert (rc == 0);
      
      // rc = s_send (subscriber, "\"get_property\", \"time-pos\"");
      rc = s_send (subscriber, argv[1]);
      char *string = s_recv (subscriber);
      printf("%s\n", string);
      free (string);

      zmq_close (subscriber);
      zmq_ctx_destroy (context);
    }
    return 0;
}
