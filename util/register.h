#ifndef OTSIM_REGISTER_H
#define OTSIM_REGISTER_H

#include <modbus/modbus.h>

typedef struct _registers_updated
{
  int *addrs;
  int size;
  int last_updated_idx;
} registers_updated_t;

extern registers_updated_t *registers_updated;

static int coils_min;
static int coils_max;

static int discretes_min;
static int discretes_max;

static int inputs_min;
static int inputs_max;

static int holdings_min;
static int holdings_max;

void register_init(modbus_mapping_t *registers);

void register_destroy();

void register_update(modbus_mapping_t *registers, int addr, int val);

int register_value(modbus_mapping_t *registers, int addr);

int register_is_updated(int addr);

void register_clear_updated();

#endif // OTSIM_REGISTER_H