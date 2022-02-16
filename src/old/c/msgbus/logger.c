#include "czmq.h"

int main(int argc, char *argv[]) {
  if (argc != 3) {
    puts("PUB endpoint and filter must be provided as arguments");
    return 1;
  }

  zsock_t *sub = zsock_new_sub(argv[1], argv[2]);
  assert (sub);

  while(1) {
    puts("waiting...");

    char *topic, *msg;
    if (zstr_recvx(sub, &topic, &msg, NULL) < 0) {
      break;
    }

    printf("%s: %s\n", topic, msg);

    zstr_free(&topic);
    zstr_free(&msg);
  }

  printf("\nexiting logger\n");

  zsock_destroy(&sub);
  return 0;
}