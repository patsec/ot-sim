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

#include <deps/json-c/json.h>
#include <helics/chelics.h>
#include <modbus/modbus.h>

#include "util/table.h"
#include "util/register.h"

#define MAX_CONN 10

static modbus_t *ctx;
static modbus_mapping_t *registers;

static helics_federate_info fed_info;
static helics_federate fed;

static int lsocket = -1;
static int updated[10];

static pthread_mutex_t mu_reg_lock;

struct _subscriptions
{
  char **names;
  char **types;

  int length;

  helics_input *subs;
} subscriptions;

struct _publications
{
  char **names;
  char **types;

  int length;

  helics_publication *pubs;
} publications;

tbl_t *coil_mappings;
tbl_t *discrete_mappings;
tbl_t *input_mappings;
tbl_t *holding_mappings;

static void trap(int dummy)
{
  if (lsocket != -1)
  {
    close(lsocket);
  }

  modbus_free(ctx);
  modbus_mapping_free(registers);

  register_destroy();

  helicsFederateDestroy(fed);
  helicsFederateInfoFree(fed_info);

  pthread_mutex_destroy(&mu_reg_lock);

  tbl_destroy(coil_mappings);
  tbl_destroy(discrete_mappings);
  tbl_destroy(input_mappings);
  tbl_destroy(holding_mappings);

  exit(dummy);
}

void *federate(void *argv)
{
  char *name = (char *)argv;

  printf("starting federate...%s\n", name);

  // fed_cfg_t c = *(fed_cfg_t*) argv;
  // helics_federate_info fedinfo = helicsCreateFederateInfo();
  // helicsFederateInfoLoadFromArgs(fed_info, c.argc, (const char *const *)c.argv, NULL);

  helics_publication closed_pub = NULL;
  helics_input closed_sub = NULL;
  helics_input kva_sub = NULL;

  char message[1024];

  fed = helicsCreateValueFederate(name, fed_info, NULL);
  name = helicsFederateGetName(fed);

  printf("registering publisher line-650632.closed for %s\n", name);

  closed_pub = helicsFederateRegisterPublication(fed, "line-650632.closed", helics_data_type_boolean, NULL, NULL);

  subscriptions.subs = (helics_input *)malloc(subscriptions.length * sizeof(helics_input *));

  for (int i = 0; i < subscriptions.length; i++)
  {
    char *key = subscriptions.names[i];

    printf("registering subscription %s for %s\n", key, name);

    subscriptions.subs[i] = helicsFederateRegisterSubscription(fed, key, NULL, NULL);
  }

  for (int i = 0; i < publications.length; i++)
  {
    char *key = publications.names[i];

    printf("registering publication %s for %s\n", key, name);

    publications.pubs[i] = helicsFederateRegisterTypePublication(fed, key, publications.types[i], NULL, NULL);
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

    for (int i = 0; i < subscriptions.length; i++)
    {
      char *key = subscriptions.names[i];
      char *type = subscriptions.types[i];

      helics_input sub = subscriptions.subs[i];

      if (!helicsInputIsUpdated(sub))
      {
        continue;
      }

      if (strcmp(type, "boolean") == 0)
      {
        helics_bool status = helicsInputGetBoolean(sub, NULL);

        void *addr;

        addr = tbl_get(coil_mappings, key);
        if (addr && !register_is_updated((int)addr))
          update_register(registers, (int)addr, status);

        addr = tbl_get(discrete_mappings, key);
        if (addr && !register_is_updated((int)addr))
          update_register(registers, (int)addr, status);
      }
      else if (strcmp(type, "double") == 0)
      {
        double value = helicsInputGetDouble(sub, NULL);

        void *addr;

        addr = tbl_get(input_mappings, key);
        if (addr && !register_is_updated((int)addr))
          update_register(registers, (int)addr, (int)value);

        addr = tbl_get(discrete_mappings, key);
        if (addr && !register_is_updated((int)addr))
          update_register(registers, (int)addr, (int)value);
      }
    }

    for (int i = 0; i < publications.length; i++)
    {
      char *key = publications.names[i];
      char *type = publications.types[i];

      helics_publication pub = publications.pubs[i];

      if (strcmp(type, "boolean") == 0)
      {
        void *addr = tbl_get(coil_mappings, key);
        if (addr && register_is_updated((int)addr))
        {
          int status = register_value(registers, (int)addr);
          helicsPublicationPublishBoolean(pub, status, NULL);
        }
      }
      else if (strcmp(type, "double") == 0)
      {
        void *addr = tbl_get(holding_mappings, key);
        if (addr && register_is_updated((int)addr))
        {
          int value = register_value(registers, (int)addr);
          helicsPublicationPublishDouble(pub, (double)value, NULL);
        }
      }
    }
  }

  return NULL;
}

int detect_write(modbus_t *ctx, const uint8_t *req, int req_length, modbus_mapping_t *mb_mapping)
{
  int rep = modbus_reply(ctx, req, req_length, mb_mapping);
  if (rep == -1)
    return -1;

  int offset = modbus_get_header_length(ctx);
  int function = req[offset];
  uint16_t address = (req[offset + 1] << 8) + req[offset + 2];

  switch (function)
  {
  case MODBUS_FC_WRITE_SINGLE_COIL:
    printf("write to coil %d detected -- new value: %d\n", address, registers->tab_bits[address]);
    updated[address] = 1;

    break;

  case MODBUS_FC_WRITE_SINGLE_REGISTER:
    break;
  }

  return rep;
}

void update_register(modbus_mapping_t *registers, int addr, int val)
{
  int coils_min = registers->start_bits;
  int coils_max = coils_min + registers->nb_bits;

  int inputs_min = registers->start_input_registers;
  int inputs_max = inputs_min + registers->nb_input_registers;

  pthread_mutex_lock(&mu_reg_lock);

  if (addr >= coils_min && addr < coils_max)
  {
    registers->tab_bits[addr] = val;
  }
  else if (addr >= inputs_min && addr < inputs_max)
  {
    registers->tab_input_registers[addr] = val;
  }

  pthread_mutex_unlock(&mu_reg_lock);
}

int main(int argc, char **argv)
{
  json_object *root = json_object_from_file("configs/modbus.json");
  json_object *name = json_object_object_get(root, "name");
  json_object *subs = json_object_object_get(root, "subscriptions");

  int arr_len = json_object_array_length(subs);
  json_object *temp;

  subscriptions.names = (char **)malloc(arr_len * sizeof(char *));
  subscriptions.types = (char **)malloc(arr_len * sizeof(char *));
  subscriptions.length = arr_len;

  for (int i = 0; i < arr_len; i++)
  {
    temp = json_object_array_get_idx(subs, i);
    json_object *key = json_object_object_get(temp, "key");
    json_object *type = json_object_object_get(temp, "type");
    subscriptions.names[i] = json_object_get_string(key);
    subscriptions.types[i] = json_object_get_string(type);
  }

  int coil_count = 0;
  int discrete_count = 0;
  int holding_count = 0;
  int input_count = 0;

  json_object *mb = json_object_object_get(root, "modbus");
  json_object *coils = json_object_object_get(mb, "coils");
  json_object *inputs = json_object_object_get(mb, "input-registers");

  coil_count = json_object_array_length(coils);
  coil_mappings = tbl_create(coil_count * 2 * 2);

  for (int i = 0; i < coil_count; i++)
  {
    temp = json_object_array_get_idx(coils, i);

    json_object *sub = json_object_object_get(temp, "sub");
    json_object *pub = json_object_object_get(temp, "sub");
    json_object *reg = json_object_object_get(temp, "register");

    table_add(coil_mappings, json_object_get_string(sub), json_object_get_int(reg));
    table_add(coil_mappings, json_object_get_string(pub), json_object_get_int(reg));
  }

  input_count = json_object_array_length(inputs);
  input_mappings = tbl_create(input_count * 2);

  for (int i = 0; i < input_count; i++)
  {
    temp = json_object_array_get_idx(inputs, i);

    json_object *sub = json_object_object_get(temp, "sub");
    json_object *reg = json_object_object_get(temp, "register");

    table_add(input_mappings, json_object_get_string(sub), json_object_get_int(reg));
  }

  json_object *addr = json_object_object_get(mb, "address");
  json_object *port = json_object_object_get(mb, "port");

  ctx = modbus_new_tcp(json_object_get_string(addr), json_object_get_int(port));

  modbus_set_debug(ctx, FALSE);

  registers = modbus_mapping_new_start_address(0, coil_count, 10000, 0, 40000, 0, 30000, input_count);
  if (registers == NULL)
  {
    fprintf(stderr, "failed to allocate registers: %s\n", modbus_strerror(errno));
    trap(-1);
  }

  register_init(registers);

  lsocket = modbus_tcp_listen(ctx, MAX_CONN);
  if (lsocket == -1)
  {
    fprintf(stderr, "failed to listen on port 5502: %s\n", modbus_strerror(errno));
    trap(-1);
  }

  fed_info = helicsCreateFederateInfo();
  helicsFederateInfoLoadFromArgs(fed_info, argc, (const char *const *)argv, NULL);

  pthread_mutex_init(&mu_reg_lock, NULL);

  /*
  coils_tab = calloc(1, sizeof(coils_tab));
  hcreate_r(10, coils_tab);

  ENTRY e;
  
  e.key = "OpenDSS/line-650632.closed";
  e.data = (void *)0;

  hsearch_r(e, ENTER, coils_tab);
  */

  // ensure cleanup on user cancellation
  signal(SIGINT, trap);

  /*
  fed_cfg_t c;

  strncpy(c.msg_target, "fed", 1024);
  strncpy(c.val_target, "fed", 1024);
  strncpy(c.source, "endpoint", 1024);
  strncpy(c.endpoint, "endpoint", 1024);

  c.argc = argc;
  c.argv = (char**)malloc(argc * sizeof(char*));

  for (int i = 0; i < argc; i++) {
    c.argv[i] = strdup(argv[i]);
  }
  */

  pthread_t fedid;
  pthread_create(&fedid, NULL, federate, json_object_get_string(name));

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
          pthread_mutex_lock(&mu_reg_lock);
          if (detect_write(ctx, query, rc, registers) == -1)
          {
            fprintf(stderr, "failed to reply to client: %s\n", modbus_strerror(errno));
          }
          pthread_mutex_unlock(&mu_reg_lock);
        }
      }
    }
  }
}