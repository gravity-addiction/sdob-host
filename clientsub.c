#include "zhelpers.h"

int main (int argc, char *argv [])
{
  printf ("Setting up sub...\n");
  void *context = zmq_ctx_new ();
  void *command_raw = zmq_socket(context, ZMQ_PULL);
  int rc = zmq_bind(command_raw, "tcp://192.168.126.85:5559");
  assert (rc == 0);

printf("Booted Writer\n");
  char msg[4096];

  while(1) {
    char *test = s_recv(command_raw);
    printf("Test: %s\n", test);

    int size = zmq_recv(command_raw, msg, 4096 - 1, ZMQ_DONTWAIT);
    printf("Here %d\n", size);
    if (size > 0) {
      msg[size < 4096 ? size : 4096 - 1] = '\0';

    // char *msg = s_recv (command_rep, ZMQ_DONTWAIT);
    // if (msg) {
      // Parse txt message
      printf("Got Message %s\n", msg);
      char * sep = ";";
      // const char** result = splitCSV(msg, sep);
//      free(msg);

      // Assign next Request ID
      uint64_t nextId = 1; //nextRequestId();
      size_t nextLen = snprintf(NULL, 0, "%ld", nextId) + 1;
      char* nextStr = (char*)malloc(nextLen * sizeof(char));
      snprintf(nextStr, nextLen, "%ld", nextId);

      printf("Next: %s\n", nextStr);
      // free(result);
      free(nextStr);
    } else {
      s_sleep(1000);
    }
  }
  return 0;
}
