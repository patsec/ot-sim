#include "register.h"

void register_init(modbus_mapping_t *registers)
{
  int count;

  coils_min = registers->start_bits;
  coils_max = coils_min + registers->nb_bits;
  count += registers->nb_bits;

  discretes_min = registers->start_input_bits;
  discretes_max = discretes_min + registers->nb_input_bits;
  count += registers->nb_input_bits;

  inputs_min = registers->start_input_registers;
  inputs_max = inputs_min + registers->nb_input_registers;
  count += registers->nb_input_registers;

  holdings_min = registers->start_registers;
  holdings_max = holdings_min + registers->nb_registers;
  count += registers->nb_registers;

  registers_updated = malloc(sizeof(registers_updated_t*));
  registers_updated->addrs = (int*)calloc(count, sizeof(int));
  registers_updated->size = count;
  registers_updated->last_updated_idx = -1;
}

void register_destroy()
{
  free(registers_updated->addrs);
  free(registers_updated);
}

void register_update(modbus_mapping_t *registers, int addr, int val)
{
  if (addr >= coils_min && addr < coils_max)
  {
    registers->tab_bits[addr] = val;
  }
  else if (addr >= discretes_min && addr < discretes_max)
  {
    registers->tab_input_bits[addr] = val;
  }
  else if (addr >= inputs_min && addr < inputs_max)
  {
    registers->tab_input_registers[addr] = val;
  }
  else if (addr >= holdings_min && addr < holdings_max)
  {
    registers->tab_registers[addr] = val;
  }
  else
  {
    return;
  }

  registers_updated->last_updated_idx++;
  registers_updated->addrs[registers_updated->last_updated_idx] = addr;
}

int register_value(modbus_mapping_t *registers, int addr)
{
  if (addr >= coils_min && addr < coils_max)
  {
    return registers->tab_bits[addr];
  }
  else if (addr >= discretes_min && addr < discretes_max)
  {
    return registers->tab_input_bits[addr];
  }
  else if (addr >= inputs_min && addr < inputs_max)
  {
    return registers->tab_input_registers[addr];
  }
  else if (addr >= holdings_min && addr < holdings_max)
  {
    return registers->tab_registers[addr];
  }
  else
  {
    return 0;
  }
}

int register_is_updated(int addr)
{
  for (int i = 0; i < registers_updated->size; i++)
  {
    if (registers_updated->addrs[i] == addr)
      return 1;
  }

  return 0;
}

void register_clear_updated()
{
  registers_updated->addrs = (int*)calloc(registers_updated->size, sizeof(int));
  registers_updated->last_updated_idx = -1;
}