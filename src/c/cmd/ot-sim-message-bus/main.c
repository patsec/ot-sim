#include <czmq.h>
#include <libxml/parser.h>

typedef struct {
  int verbose;
  const char *pull;
  const char *pub;
  const char *debug;
} config;

#define MATCHXML(e, n) xmlStrcmp(e->name, (const xmlChar*) n) == 0

static int xml_handler(config *c, xmlDoc *doc, xmlNode *node) {
  xmlChar *text;

  node = node->xmlChildrenNode;

  while (node != NULL) {
    text = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);

    if (MATCHXML(node, "verbose")) {
      c->verbose = atoi(text);
    } else if (MATCHXML(node, "pull-endpoint")) {
      c->pull = strdup(text);
    } else if (MATCHXML(node, "pub-endpoint")) {
      c->pub = strdup(text);
    } else if (MATCHXML(node, "debug-endpoint")) {
      c->debug = strdup(text);
    }

    xmlFree(text);
    node = node->next;
  }

  return 0;
}

static int xml_parse(char *path, config *c) {
  xmlDoc *doc;
  xmlNode *node;

  doc = xmlParseFile(path);

  if (doc == NULL) {
    return -1;
  }

  node = xmlDocGetRootElement(doc);

  if (node == NULL) {
    xmlFreeDoc(doc);
    return -1;
  }

  if (!MATCHXML(node, "ot-sim")) {
    xmlFreeDoc(doc);
    return -1;
  }

  node = node->xmlChildrenNode;

  while (node != NULL) {
    // look for top-level message-bus element only
    if (MATCHXML(node, "message-bus")) {
      int rc = xml_handler(c, doc, node);

      xmlFreeDoc(doc);
      return rc;
    }

    node = node->next;
  }

  xmlFreeDoc(doc);
  return 0;
}

int main(int argc, char *argv[]) {
  config c;

  c.verbose = 0;
  c.pull    = "tcp://127.0.0.1:1234";
  c.pub     = "tcp://127.0.0.1:5678";
  c.debug   = NULL;

  if (argc == 2) {
    printf("loading config %s\n", argv[1]);

    if (xml_parse(argv[1], &c) != 0) {
      puts("cannot load XML config");
      return 1;
    }
  }

  zactor_t *proxy = zactor_new(zproxy, NULL);
  assert (proxy);

  if (c.verbose) {
    puts("setting proxy to verbose");

    zstr_sendx(proxy, "VERBOSE", NULL);
    zsock_wait(proxy);
  }

  printf("using %s for PULL endpoint\n", c.pull);

  zstr_sendx(proxy, "FRONTEND", "PULL", c.pull, NULL);
  zsock_wait(proxy);

  printf("using %s for PUB endpoint\n", c.pub);

  zstr_sendx(proxy, "BACKEND", "PUB", c.pub, NULL);
  zsock_wait(proxy);

  if (c.debug) {
    printf("setting up proxy capture to debug endpoint %s\n", c.debug);

    zstr_sendx(proxy, "CAPTURE", c.debug, NULL);
    zsock_wait(proxy);
  }

  while(1) {
    puts("proxy running");

    unsigned int remaining = sleep(UINT_MAX);

    // likely not interrupted with signal
    if (remaining == 0) {
      continue;
    }

    // interrupted with signal
    break;
  }

  printf("\nexiting proxy\n");

  zactor_destroy(&proxy);
  return 0;
}