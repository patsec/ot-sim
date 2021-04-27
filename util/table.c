#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "util/table.h"

tbl_t *tbl_create(size_t size)
{
  tbl_t *table = malloc(sizeof(*table));

  *table = TBL_T_INITIALIZER;

  hcreate_r(size, &table->htab);
  table->size = size;
  int ks = size * sizeof(char *);
  table->keys = malloc(ks);

  memset(table->keys, 0, ks);

  return table;
}

void tbl_destroy(tbl_t *table)
{
  free(table->keys);
  hdestroy_r(&table->htab);
  free(table);

  table = NULL;
}

int table_add(tbl_t *table, char *key, void *data)
{
  unsigned n = 0;
  ENTRY e, *ep;

  e.key = strdup(key);
  e.data = data;

  unsigned int fb = table->htab.filled;

  n = hsearch_r(e, ENTER, &ep, &table->htab);

  if (fb < table->htab.filled)
    *(table->keys + fb) = e.key;

  return n;
}

void *table_get(tbl_t *table, char *key)
{
  unsigned n = 0;
  ENTRY e, *ep;

  e.key = key;
  n = hsearch_r(e, FIND, &ep, &table->htab);

  if (!n)
    return NULL;

  return ep->data;
}

static char *keys[] = {"ciro", "hue", "br", "haha", "Lol"};
static char *values[] = {"cccc", "hhhh", "bbbb", "aaa", "oooo"};

int main(int argc, char *argv[])
{
  unsigned i = 0;
  tbl_t *table = tbl_create(5);
  void *d = NULL;

  if (argc < 2)
  {
    fprintf(stderr, "%s\n", "Usage: ./hash-tables <name>\n");
    exit(EXIT_FAILURE);
  }

  for (; i < table->size; i++)
  {
    table_add(table, keys[i], values[i]);
  }

  if ((d = table_get(table, argv[1])) != NULL)
    fprintf(stdout, "%s\n", (char *)d);
  else
    fprintf(stdout, "%s\n", "Not found :(");

  tbl_destroy(table);

  return 0;
}