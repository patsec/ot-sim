#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
 
#include "register.h"

int register_init(unsigned int c_count, unsigned int d_count, unsigned int h_count, unsigned int i_count)
{
  registers = modbus_mapping_new_start_address(COILS_START, c_count, DISCRETES_START, d_count, HOLDINGS_START, h_count, INPUTS_START, i_count);
  if (registers == NULL)
    return -1;

  int count = c_count + d_count + h_count + i_count;

  coils_min = COILS_START;
  coils_max = COILS_START + c_count;

  discretes_min = DISCRETES_START;
  discretes_max = DISCRETES_START + d_count;

  inputs_min = INPUTS_START;
  inputs_max = INPUTS_START + i_count;

  holdings_min = HOLDINGS_START;
  holdings_max = HOLDINGS_START + h_count;

  registers_updated = malloc(sizeof(registers_updated_t *));
  registers_updated->addrs = (int *)calloc(count, sizeof(int));
  registers_updated->size = count;
  registers_updated->last_updated_idx = -1;

  for (int i = 0; i < registers_updated->size; i++) {
    registers_updated->addrs[i] = -1;
  }

  pthread_mutex_init(&mu_reg_lock, NULL);

  return 0;
}

void register_destroy()
{
  modbus_mapping_free(registers);
  free(registers_updated->addrs);
  free(registers_updated);
  pthread_mutex_destroy(&mu_reg_lock);
}

int register_update(unsigned int addr, int val)
{
  pthread_mutex_lock(&mu_reg_lock);

  if (addr >= coils_min && addr < coils_max)
  {
    printf("COIL %d UPDATED TO %d\n", addr, val);
    registers->tab_bits[addr] = val;
  }
  else if (addr >= discretes_min && addr < discretes_max)
  {
    printf("DISCRETE %d UPDATED TO %d\n", addr, val);
    registers->tab_input_bits[addr] = val;
  }
  else if (addr >= inputs_min && addr < inputs_max)
  {
    printf("INPUT %d UPDATED TO %d\n", addr, val);
    registers->tab_input_registers[addr] = val;
  }
  else if (addr >= holdings_min && addr < holdings_max)
  {
    printf("HOLDING %d UPDATED TO %d\n", addr, val);
    registers->tab_registers[addr] = val;
  }
  else
  {
    pthread_mutex_unlock(&mu_reg_lock);
    return -1;
  }

  pthread_mutex_unlock(&mu_reg_lock);
  return 0;
}

void register_updated(unsigned int addr) {
  // TODO: replace this approach with hsearch?

  pthread_mutex_lock(&mu_reg_lock);

  registers_updated->last_updated_idx++;
  registers_updated->addrs[registers_updated->last_updated_idx] = addr;

  pthread_mutex_unlock(&mu_reg_lock);
}

int register_value(unsigned int addr)
{
  int val = 0;

  pthread_mutex_lock(&mu_reg_lock);

  if (addr >= coils_min && addr < coils_max)
  {
    val = registers->tab_bits[addr];
  }
  else if (addr >= discretes_min && addr < discretes_max)
  {
    val = registers->tab_input_bits[addr];
  }
  else if (addr >= inputs_min && addr < inputs_max)
  {
    val = registers->tab_input_registers[addr];
  }
  else if (addr >= holdings_min && addr < holdings_max)
  {
    val = registers->tab_registers[addr];
  }

  pthread_mutex_unlock(&mu_reg_lock);
  return val;
}

int register_is_updated(unsigned int addr)
{
  pthread_mutex_lock(&mu_reg_lock);

  for (int i = 0; i < registers_updated->size; i++)
  {
    if (registers_updated->addrs[i] == addr) {
      pthread_mutex_unlock(&mu_reg_lock);
      return 1;
    }
  }

  pthread_mutex_unlock(&mu_reg_lock);
  return 0;
}

void register_clear_updated()
{
  pthread_mutex_lock(&mu_reg_lock);

  registers_updated->addrs = (int *)calloc(registers_updated->size, sizeof(int));
  registers_updated->last_updated_idx = -1;

  for (int i = 0; i < registers_updated->size; i++) {
    registers_updated->addrs[i] = -1;
  }

  pthread_mutex_unlock(&mu_reg_lock);
}

int register_reply_detect_write(modbus_t *ctx, const uint8_t *req, int req_length)
{
  pthread_mutex_lock(&mu_reg_lock);
  int rep = modbus_reply(ctx, req, req_length, registers);
  pthread_mutex_unlock(&mu_reg_lock);

  if (rep == -1)
    return -1;

  int offset = modbus_get_header_length(ctx);
  int function = req[offset];
  uint16_t address = (req[offset + 1] << 8) + req[offset + 2];

  switch (function)
  {
  case MODBUS_FC_WRITE_SINGLE_COIL:
  case MODBUS_FC_WRITE_SINGLE_REGISTER:
    register_updated(address);
    break;
  }

  return rep;
}