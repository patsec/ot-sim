#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <helics/chelics.h>
#include <modbus/modbus.h>

#include <json.h>
#include "register.h"

// ---- BEGIN PUB/SUB STUFF
// ---- PUTTING THIS HERE SINCE HAVING IT IN A SEPARATE HEADER/SOURCE FILE
// ---- CAUSES "MULTIPLE DEFINITION" ERRORS WITH HELICS CONSTANTS
typedef struct _subscriptions
{
  const char **names;
  const char **types;
  int *registers;

  unsigned int size;

  helics_input *subs;
} otsim_subscriptions_t;

typedef struct _publications
{
  const char **names;
  const char **types;
  int *registers;

  unsigned int size;

  helics_publication *pubs;
} otsim_publications_t;

otsim_subscriptions_t *otsim_subscriptions_new(unsigned int size)
{
  otsim_subscriptions_t *subs;

  subs = (otsim_subscriptions_t *)malloc(sizeof(otsim_subscriptions_t));
  if (subs == NULL)
    return NULL;

  
  subs->names = (const char **)malloc(size * sizeof(const char *));
  subs->types = (const char **)malloc(size * sizeof(const char *));
  subs->registers = (int *)malloc(size * sizeof(int));
  subs->subs = (helics_input *)malloc(size * sizeof(helics_input));
  subs->size = size;

  return subs;
}

void otsim_subscriptions_free(otsim_subscriptions_t *subs)
{
  free(subs->names);
  free(subs->types);
  free(subs->registers);
  free(subs->subs);
}

int otsim_subscriptions_set_register(otsim_subscriptions_t *subs, const char *name, int reg)
{
  for (int i = 0; i < subs->size; i++)
  {
    if (strcmp(subs->names[i], name) == 0)
    {
      subs->registers[i] = reg;
      return 0;
    }
  }

  return -1;
}

otsim_publications_t *otsim_publications_new(unsigned int size)
{
  otsim_publications_t *pubs;

  pubs = (otsim_publications_t *)malloc(sizeof(otsim_publications_t));
  if (pubs == NULL)
    return NULL;

  
  pubs->names = (const char **)malloc(size * sizeof(const char *));
  pubs->types = (const char **)malloc(size * sizeof(const char *));
  pubs->registers = (int *)malloc(size * sizeof(int));
  pubs->pubs = (helics_publication *)malloc(size * sizeof(helics_publication));
  pubs->size = size;

  return pubs;
}

void otsim_publications_free(otsim_publications_t *pubs)
{
  free(pubs->names);
  free(pubs->types);
  free(pubs->registers);
  free(pubs->pubs);
}

int otsim_publications_set_register(otsim_publications_t *pubs, const char *name, int reg)
{
  for (int i = 0; i < pubs->size; i++)
  {
    if (strcmp(pubs->names[i], name) == 0)
    {
      pubs->registers[i] = reg;
      return 0;
    }
  }

  return -1;
}
// ---- END PUB/SUB STUFF

#define MAX_CONN 10

static modbus_t *ctx;

static helics_federate_info fed_info;
static helics_federate fed;

static int lsocket = -1;

static otsim_subscriptions_t *subs;
static otsim_publications_t *pubs;

static void trap(int dummy)
{
  if (lsocket != -1)
  {
    close(lsocket);
  }

  modbus_free(ctx);
  register_destroy();

  helicsFederateDestroy(fed);
  helicsFederateInfoFree(fed_info);

  otsim_subscriptions_free(subs);
  otsim_publications_free(pubs);

  exit(dummy);
}

void *federate(void *argv)
{
  char *name = (char *)argv;

  printf("starting federate %s...\n", name);

  fed = helicsCreateValueFederate(name, fed_info, NULL);

  for (int i = 0; i < subs->size; i++)
  {
    const char *key = subs->names[i];

    printf("registering subscription %s for %s\n", key, name);

    subs->subs[i] = helicsFederateRegisterSubscription(fed, key, NULL, NULL);
  }

  for (int i = 0; i < pubs->size; i++)
  {
    const char *key = pubs->names[i];

    printf("registering publication %s for %s\n", key, name);

    pubs->pubs[i] = helicsFederateRegisterTypePublication(fed, key, pubs->types[i], NULL, NULL);
  }

  printf("entering init Mode\n");
  helicsFederateEnterInitializingMode(fed, NULL);

  printf("entered init Mode\n");
  helicsFederateEnterExecutingMode(fed, NULL);

  printf("entered execution Mode\n");

  for (int t = 0; t < 3600; ++t)
  {
    helics_time time = helicsFederateRequestTime(fed, (helics_time)t, NULL);

    // printf("granted time %f\n", time);

    for (int i = 0; i < subs->size; i++)
    {
      const char *key = subs->names[i];
      const char *type = subs->types[i];
      int addr = subs->registers[i];

      printf("SUB -- KEY: %s, TYPE: %s, REG: %d\n", key, type, addr);

      helics_input sub = subs->subs[i];

      if (!helicsInputIsUpdated(sub))
      {
        printf("  NO UPDATE AVAILABLE\n");
        continue;
      }

      if (strcmp(type, "boolean") == 0)
      {
        helics_bool status = helicsInputGetBoolean(sub, NULL);

        if (!register_is_updated(addr)) {
          printf("  UPDATING REGISTER: ADDR: %d, STATUS: %d\n", addr, status);
          register_update(addr, status);
        } else {
          printf("  PROCESSING MODBUS WRITE -- SKIPPING SUB\n");
        }
      }
      else if (strcmp(type, "double") == 0)
      {
        double value = helicsInputGetDouble(sub, NULL);

        if (!register_is_updated(addr)) {
          printf("  UPDATING REGISTER: ADDR: %d, VALUE: %f (%d)\n", addr, value, (int)value);
          if (register_update(addr, (int)value) != 0) {
            printf("  ERROR UPDATING REGISTER\n");
          }
        } else {
          printf("  PROCESSING MODBUS WRITE -- SKIPPING SUB\n");
        }
      }
    }

    for (int i = 0; i < pubs->size; i++)
    {
      const char *key = pubs->names[i];
      const char *type = pubs->types[i];
      int addr = pubs->registers[i];

      if (register_is_updated(addr)) {
        helics_publication pub = pubs->pubs[i];

        if (strcmp(type, "boolean") == 0)
        {
          int status = register_value(addr);
          helicsPublicationPublishBoolean(pub, status, NULL);
        }
        else if (strcmp(type, "double") == 0)
        {
          int value = register_value(addr);
          helicsPublicationPublishDouble(pub, (double)value, NULL); // TODO: scaling
        }
      }
    }

    register_clear_updated();
  }

  return NULL;
}

int main(int argc, char **argv)
{
  const char *file = NULL;

  for (int i = 0; i < argc; ++i) {
    if (strcmp(argv[i], "--config") == 0) {
      file = argv[i + 1];
      ++i;
    }
  }

  if (!file) {
    fprintf(stderr, "must provide config file option (--config)\n");
    return -1;
  }

  json_object *root = json_object_from_file(file);
  json_object *name = json_object_object_get(root, "name");

  json_object *subscriptions = json_object_object_get(root, "subscriptions");

  int arr_len = json_object_array_length(subscriptions);
  json_object *temp;

  subs = otsim_subscriptions_new(arr_len);

  for (int i = 0; i < arr_len; i++)
  {
    temp = json_object_array_get_idx(subscriptions, i);
    json_object *key = json_object_object_get(temp, "key");
    json_object *type = json_object_object_get(temp, "type");
    subs->names[i] = json_object_get_string(key);
    subs->types[i] = json_object_get_string(type);
  }

  json_object *publications = json_object_object_get(root, "publications");

  arr_len = json_object_array_length(publications);

  pubs = otsim_publications_new(arr_len);

  for (int i = 0; i < arr_len; i++)
  {
    temp = json_object_array_get_idx(publications, i);
    json_object *key = json_object_object_get(temp, "key");
    json_object *type = json_object_object_get(temp, "type");
    pubs->names[i] = json_object_get_string(key);
    pubs->types[i] = json_object_get_string(type);
  }

  json_object *mb = json_object_object_get(root, "modbus");
  json_object *coils = json_object_object_get(mb, "coils");
  json_object *discretes = json_object_object_get(mb, "discrete-inputs");
  json_object *holdings = json_object_object_get(mb, "holding-registers");
  json_object *inputs = json_object_object_get(mb, "input-registers");

  int coil_count = json_object_array_length(coils);

  for (int i = 0; i < coil_count; i++)
  {
    temp = json_object_array_get_idx(coils, i);

    json_object *sub = json_object_object_get(temp, "sub");
    json_object *pub = json_object_object_get(temp, "pub");
    json_object *reg = json_object_object_get(temp, "reg");

    otsim_subscriptions_set_register(subs, json_object_get_string(sub), json_object_get_int(reg));
    otsim_publications_set_register(pubs, json_object_get_string(pub), json_object_get_int(reg));
  }

  int discrete_count = json_object_array_length(discretes);

  for (int i = 0; i < discrete_count; i++)
  {
    temp = json_object_array_get_idx(discretes, i);

    json_object *sub = json_object_object_get(temp, "sub");
    json_object *reg = json_object_object_get(temp, "reg");

    otsim_subscriptions_set_register(subs, json_object_get_string(sub), json_object_get_int(reg));
  }

  int holding_count = json_object_array_length(holdings);

  for (int i = 0; i < holding_count; i++)
  {
    temp = json_object_array_get_idx(holdings, i);

    json_object *sub = json_object_object_get(temp, "sub");
    json_object *pub = json_object_object_get(temp, "pub");
    json_object *reg = json_object_object_get(temp, "reg");

    otsim_subscriptions_set_register(subs, json_object_get_string(sub), json_object_get_int(reg));
    otsim_publications_set_register(pubs, json_object_get_string(pub), json_object_get_int(reg));
  }

  int input_count = json_object_array_length(inputs);

  for (int i = 0; i < input_count; i++)
  {
    temp = json_object_array_get_idx(inputs, i);

    json_object *sub = json_object_object_get(temp, "sub");
    json_object *reg = json_object_object_get(temp, "reg");

    otsim_subscriptions_set_register(subs, json_object_get_string(sub), json_object_get_int(reg));
  }

  register_init(coil_count, discrete_count, holding_count, input_count);

  json_object *addr = json_object_object_get(mb, "address");
  json_object *port = json_object_object_get(mb, "port");

  ctx = modbus_new_tcp(json_object_get_string(addr), json_object_get_int(port));

  modbus_set_debug(ctx, FALSE);

  lsocket = modbus_tcp_listen(ctx, MAX_CONN);
  if (lsocket == -1)
  {
    fprintf(stderr, "failed to listen on port 5502: %s\n", modbus_strerror(errno));
    trap(-1);
  }

  fed_info = helicsCreateFederateInfo();
  helicsFederateInfoLoadFromArgs(fed_info, argc, (const char *const *)argv, NULL);

  signal(SIGINT, trap);

  // TODO: move threaded federate code to same loop as Modbus listener to get rid of threaded hassles.
  // 1. Process Modbus requests.
  // 2. Process HELICS publications.
  // 3. Process HELICS subscriptions.
  pthread_t fedid;
  pthread_create(&fedid, NULL, federate, (void *)json_object_get_string(name));

  fd_set refset;
  fd_set rdset;
  int fdmax = lsocket;

  FD_ZERO(&refset);
  FD_SET(lsocket, &refset);

  while (1)
  {
    rdset = refset;

    if (select(fdmax + 1, &rdset, NULL, NULL, NULL) == -1)
    {
      fprintf(stderr, "server select failure");
      trap(-1);
    }

    int main;

    for (main = 0; main <= fdmax; main++)
    {
      if (!FD_ISSET(main, &rdset))
      {
        continue;
      }

      if (main == lsocket)
      { // new connection
        socklen_t addrlen;
        struct sockaddr_in clientaddr;
        int newfd;

        addrlen = sizeof(clientaddr);
        memset(&clientaddr, 0, sizeof(clientaddr));
        newfd = accept(lsocket, (struct sockaddr *)&clientaddr, &addrlen);

        if (newfd == -1)
        {
          fprintf(stderr, "server accept failure");
        }
        else
        {
          FD_SET(newfd, &refset);

          if (newfd > fdmax)
          {
            fdmax = newfd;
          }

          printf("new connection from %s:%d on socket %d\n", inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port, newfd);
        }
      }
      else
      { // data on existing connection
        modbus_set_socket(ctx, main);

        uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
        int rc = -1;

        rc = modbus_receive(ctx, query);

        if (rc == -1)
        {
          printf("connection closed on socket %d\n", main);
          close(main);

          FD_CLR(main, &refset);

          if (main == fdmax)
          {
            fdmax--;
          }
        }

        if (rc > 0)
        {
          if (register_reply_detect_write(ctx, query, rc) == -1)
          {
            fprintf(stderr, "failed to reply to client: %s\n", modbus_strerror(errno));
          }
        }
      }
    }
  }
}