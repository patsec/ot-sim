#include "czmq.h"

int main(int argc, char *argv[]) {
  if (argc != 2) {
    puts("PULL endpoint must be provided as argument");
    return 1;
  }

  zsock_t *capture = zsock_new_pull(argv[1]);
  assert (capture);

  while(1) {
    puts("waiting...");

    char *topic, *msg;
    if (zstr_recvx(capture, &topic, &msg, NULL) < 0) {
      break;
    }

    printf("%s: %s\n", topic, msg);

    zstr_free(&topic);
    zstr_free(&msg);
  }

  printf("\nexiting debugger\n");

  zsock_destroy(&capture);
  return 0;
}