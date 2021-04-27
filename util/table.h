#ifndef OTSIM_TABLES_H
#define OTSIM_TABLES_H

#include <search.h>

typedef struct _tbl
{
  struct hsearch_data htab;
  size_t size;
  char **keys;
} tbl_t;

#define TBL_T_INITIALIZER \
  (tbl_t) { .htab = (struct hsearch_data){0}, .size = 0 }

tbl_t *tbl_create(size_t size);

void tbl_destroy(tbl_t *table);

int table_add(tbl_t *table, char *key, void *data);

void *table_get(tbl_t *table, char *key);

#endif // OTSIM_TABLES_H