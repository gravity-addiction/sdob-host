#include "zhelpers.h"

int main (int argc, char *argv [])
{
    if (argc > 1) {
      //  Socket to talk to server
      void *context = zmq_ctx_new ();
      void *subscriber = zmq_socket (context, ZMQ_REQ);
      int rc = zmq_connect (subscriber, "tcp://sdob401.local:4010");
      assert (rc == 0);
      printf("Connected\n");

      void *subx = zmq_socket (context, ZMQ_REQ);
      rc = zmq_connect (subx, "tcp://sdob401.local:4010");
      assert (rc == 0);
      printf("Connected X\n");
      // rc = s_send (subscriber, "\"get_property\", \"time-pos\"");
      rc = s_send (subscriber, argv[1]);
      printf("Sent: %s - %d\n", argv[1], rc);
      rc = s_send (subx, argv[2]);
      printf("Sent X: %s - %d\n", argv[2], rc);

      char *string = s_recv (subscriber);
      printf("Returned: %s\n", string);
      free (string);

      char *strx = s_recv (subx);
      printf("Returned: %s\n", strx);
      free (strx);      

      zmq_close (subscriber);
      zmq_close (subx);
      zmq_ctx_destroy (context);
    }
    return 0;
}
