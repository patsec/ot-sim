#ifndef OTSIM_REGISTER_H
#define OTSIM_REGISTER_H

#include <modbus/modbus.h>

#define COILS_START     0
#define DISCRETES_START 10000
#define HOLDINGS_START  40000
#define INPUTS_START    30000

typedef struct _registers_updated
{
  int *addrs;
  int size;
  int last_updated_idx;
} registers_updated_t;

int coils_min;
int coils_max;

int discretes_min;
int discretes_max;

int inputs_min;
int inputs_max;

int holdings_min;
int holdings_max;

modbus_mapping_t *registers;
pthread_mutex_t mu_reg_lock;
registers_updated_t *registers_updated;

int register_init(unsigned int c_count, unsigned int d_count, unsigned int h_count, unsigned int i_count);

void register_destroy();

int register_update(unsigned int addr, int val);

void register_updated(unsigned int addr);

int register_value(unsigned int addr);

int register_is_updated(unsigned int addr);

void register_clear_updated();

int register_reply_detect_write(modbus_t *ctx, const uint8_t *req, int req_length);

#endif // OTSIM_REGISTER_H