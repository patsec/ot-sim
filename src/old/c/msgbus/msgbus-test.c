#include "czmq.h"

int main(int argc, char *argv[]) {
  if (argc != 2) {
    puts("PULL endpoint must be provided as argument");
    return 1;
  }

  zsock_t *pusher = zsock_new(ZMQ_PUSH);
  assert (pusher);

  zactor_t *mon = zactor_new(zmonitor, pusher);
  assert (mon);

  zstr_send(mon, "VERBOSE");
  zstr_sendx(mon, "LISTEN", "ALL", NULL);
  zstr_send(mon, "START");
  zsock_wait(mon);

  printf("connecting to %s\n", argv[1]);

  if (zsock_connect(pusher, "%s", argv[1]) != 0) {
    printf("error connecting: %s\n", zmq_strerror(errno));
  };

  int rc = zstr_sendx(pusher, "LOG", "Hello, world!", NULL);
  if (rc != 0 ) {
    printf("error sending log message: %s\n", zmq_strerror(errno));
  }

  zmq_poll(NULL, 0, 200);

  zactor_destroy(&mon);
  zsock_destroy(&pusher);

  return 0;
}