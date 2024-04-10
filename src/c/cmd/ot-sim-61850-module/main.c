#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <czmq.h>
#include <json-c/json.h>
#include <libxml/parser.h>

#include <libiec61850/iec61850_server.h>

// ---- BEGIN PUB/SUB STUFF
// ---- PUTTING THIS HERE SINCE HAVING IT IN A SEPARATE HEADER/SOURCE FILE
// ---- CAUSES "MULTIPLE DEFINITION" ERRORS WITH HELICS CONSTANTS
typedef struct _subscriptions {
  const char **names;
  const char **types;
  const char **tags;

  unsigned int size;
} otsim_subscriptions_t;

typedef struct _publications {
  const char **names;
  const char **types;
  const char **tags;

  double *values;
  bool *updates;

  unsigned int size;
} otsim_publications_t;

otsim_subscriptions_t *otsim_subscriptions_new(unsigned int size) {
  otsim_subscriptions_t *subs;

  subs = (otsim_subscriptions_t *)malloc(sizeof(otsim_subscriptions_t));
  if (subs == NULL)
    return NULL;

  
  subs->names = (const char **) malloc(size * sizeof(const char *));
  subs->types = (const char **) malloc(size * sizeof(const char *));
  subs->tags  = (const char **) calloc(size, sizeof(const char *));
  subs->size  = size;

  return subs;
}

void otsim_subscriptions_free(otsim_subscriptions_t *subs) {
  free(subs->names);
  free(subs->types);
  free(subs->tags);
}

otsim_publications_t *otsim_publications_new(unsigned int size) {
  otsim_publications_t *pubs;

  pubs = (otsim_publications_t *)malloc(sizeof(otsim_publications_t));
  if (pubs == NULL)
    return NULL;

  
  pubs->names   = (const char **) malloc(size * sizeof(const char *));
  pubs->types   = (const char **) malloc(size * sizeof(const char *));
  pubs->tags    = (const char **) calloc(size, sizeof(const char *));
  pubs->values  = (double *)      malloc(size * sizeof(double));
  pubs->updates = (bool *)        calloc(size, sizeof(bool));
  pubs->size    = size;

  return pubs;
}

void otsim_publications_free(otsim_publications_t *pubs) {
  free(pubs->names);
  free(pubs->types);
  free(pubs->tags);
  free(pubs->values);
  free(pubs->updates);
}
// ---- END PUB/SUB STUFF

static otsim_subscriptions_t *subs;
static otsim_publications_t *pubs;

static zsock_t *pusher;
static zsock_t *subscriber;
static zpoller_t *poller;

static int running;

static void trap(int sig) {
  running = 0; // TODO: make this thread-safe?

  otsim_subscriptions_free(subs);
  otsim_publications_free(pubs);

  zsock_destroy(&pusher);
  zsock_destroy(&subscriber);
  zpoller_destroy(&poller);

  exit(sig);
}

typedef struct {
  const char *pull;
  const char *publish;
  const char *broker;
  const char *name;
} config;

#define MATCHXML(e, n) xmlStrcmp(e->name, (const xmlChar*) n) == 0

// handle "subscriptions" element
static int sub_handler(config *c, xmlDoc *doc, xmlNode *node) {
  int count = xmlChildElementCount(node);
  subs = otsim_subscriptions_new(count);

  xmlNode *sub = node->xmlChildrenNode;
  int idx = 0;

  xmlChar *text;

  while (sub != NULL) {
    if (MATCHXML(sub, "subscription")) {
      xmlNode *setting = sub->xmlChildrenNode;

      while (setting != NULL) {
        text = xmlNodeListGetString(doc, setting->xmlChildrenNode, 1);

        if (MATCHXML(setting, "key")) {
          subs->names[idx] = strdup(text);

          if (subs->tags[idx] == NULL) {
            subs->tags[idx] = strdup(text);
          }
        } else if (MATCHXML(setting, "type")) {
          subs->types[idx] = strdup(text);
        } else if (MATCHXML(setting, "tag")) {
          subs->tags[idx] = strdup(text);
        }

        xmlFree(text);
        setting = setting->next;
      }

      idx++;
    }

    sub = sub->next;
  }

  return 0;
}

// handle "publications" element
static int pub_handler(config *c, xmlDoc *doc, xmlNode *node) {
  int count = xmlChildElementCount(node);
  pubs = otsim_publications_new(count);

  xmlNode *pub = node->xmlChildrenNode;
  int idx = 0;

  xmlChar *text;

  while (pub != NULL) {
    if (MATCHXML(pub, "publication")) {
      xmlNode *setting = pub->xmlChildrenNode;

      while (setting != NULL) {
        text = xmlNodeListGetString(doc, setting->xmlChildrenNode, 1);

        if (MATCHXML(setting, "key")) {
          pubs->names[idx] = strdup(text);

          if (pubs->tags[idx] == NULL) {
            pubs->tags[idx] = strdup(text);
          }
        } else if (MATCHXML(setting, "type")) {
          pubs->types[idx] = strdup(text);
        } else if (MATCHXML(setting, "tag")) {
          pubs->tags[idx] = strdup(text);
        }

        xmlFree(text);
        setting = setting->next;
      }

      idx++;
    }

    pub = pub->next;
  }

  return 0;
}

// handle "io" element
static int xml_handler(config *c, xmlDoc *doc, xmlNode *node) {
  xmlChar *text;

  node = node->xmlChildrenNode;

  while (node != NULL) {
    text = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);

    if (MATCHXML(node, "pull-endpoint")) {
      c->pull = strdup(text);
    } else if (MATCHXML(node, "pub-endpoint")) {
      c->publish = strdup(text);
    } else if (MATCHXML(node, "broker-endpoint")) {
      c->broker = strdup(text);
    } else if (MATCHXML(node, "federate-name")) {
      c->name = strdup(text);
    } else if (MATCHXML(node, "subscriptions")) {
      int rc = sub_handler(c, doc, node);
      if (rc != 0) {
        return rc;
      }
    } else if (MATCHXML(node, "publications")) {
      int rc = pub_handler(c, doc, node);
      if (rc != 0) {
        return rc;
      }
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
    // look for top-level io element only
    if (MATCHXML(node, "iec-61850")) {
      int rc = xml_handler(c, doc, node);

      xmlFreeDoc(doc);
      return rc;
    }

    node = node->next;
  }

  xmlFreeDoc(doc);
  return 0;
}

typedef struct {
  const char *tag;
  double     value;
  uint64_t   ts;
} point_t;

typedef struct {
  point_t    *updates;
  const char *recipient;
  const char *confirm;

  unsigned int size;
} update_t;

typedef struct {
  const char *api_version;
  const char *kind;

  struct {
    const char *sender;
  } metadata;

  json_object *contents;
} envelope_t;

void update_free(update_t *update) {
  free(update->updates);
}

static int envelope_marshal(envelope_t env, json_object *root) {
  json_object *md_obj = json_object_new_object();

  json_object_object_add(root, "apiVersion", json_object_new_string("v1"));
  json_object_object_add(root, "kind", json_object_new_string(env.kind));
  json_object_object_add(root, "metadata", md_obj);

  json_object_object_add(md_obj, "sender", json_object_new_string(env.metadata.sender));

  return 0;
}

static int envelope_add_measurements(json_object *root, int len, point_t *points) {
  json_object *contents_obj     = json_object_new_object();
  json_object *measurements_arr = json_object_new_array_ext(len);

  for (int i = 0; i < len; i++) {
    json_object *point_obj = json_object_new_object();

    json_object_object_add(point_obj, "tag",   json_object_new_string(points[i].tag));
    json_object_object_add(point_obj, "value", json_object_new_double(points[i].value));
    json_object_object_add(point_obj, "ts",    json_object_new_uint64(points[i].ts));

    json_object_array_add(measurements_arr, point_obj);
  }

  json_object_object_add(contents_obj, "measurements", measurements_arr);
  json_object_object_add(root, "contents", contents_obj);

  return 0;
}

static int envelope_unmarshal(char *data, json_object *root, envelope_t *env) {
  root = json_tokener_parse(data);

  json_object *version_obj  = json_object_object_get(root, "apiVersion");
  json_object *kind_obj     = json_object_object_get(root, "kind");
  json_object *md_obj       = json_object_object_get(root, "metadata");
  json_object *contents_obj = json_object_object_get(root, "contents");

  json_object *sender_obj = json_object_object_get(md_obj, "sender");

  env->api_version     = json_object_get_string(version_obj);
  env->kind            = json_object_get_string(kind_obj);
  env->metadata.sender = json_object_get_string(sender_obj);

  env->contents = contents_obj;

  return 0;
}

static int *update_unmarshal(json_object *root, update_t *update) {
  json_object *updates   = json_object_object_get(root, "updates");
  json_object *recipient = json_object_object_get(root, "recipient");
  json_object *confirm   = json_object_object_get(root, "confirm");

  if (recipient != NULL) {
    update->recipient = json_object_get_string(recipient);
  }

  if (confirm != NULL) {
    update->confirm   = json_object_get_string(confirm);
  }

  int len = json_object_array_length(updates);

  update->size    = len;
  update->updates = (point_t*) malloc(len * sizeof(point_t));

  for (int i = 0; i < len; i++) {
    json_object *el    = json_object_array_get_idx(updates, i);
    json_object *tag   = json_object_object_get(el, "tag");
    json_object *value = json_object_object_get(el, "value");

    point_t point = {
      .tag   = json_object_get_string(tag),
      .value = json_object_get_double(value),
    };

    update->updates[i] = point;
  }

  return 0;
}

int main(int argc, char **argv) {
  config c;

  c.pull    = "tcp://127.0.0.1:1234";
  c.publish = "tcp://127.0.0.1:5678";
  c.name    = "IEC-61850";

  if (argc == 2) {
    printf("loading config %s\n", argv[1]);

    if (xml_parse(argv[1], &c) != 0) {
      puts("cannot load XML config");
      return 1;
    }
  }

  pusher = zsock_new_push(c.pull);
  subscriber = zsock_new_sub(c.publish, "RUNTIME");
  poller = zpoller_new(subscriber, NULL);

  // Initialize subs so for loops below don't error out.
  if (subs == NULL) {
    subs = otsim_subscriptions_new(0);
  }

  // Initialize pubs so for loops below don't error out.
  if (pubs == NULL) {
    pubs = otsim_publications_new(0);
  }

  signal(SIGINT, trap);
  running = 1;

  IedServerConfig config = IedServerConfig_create();

  DataObject iedModel_GenericIO_GGIO1_SPCSO1 = {
    DataObjectModelType,
    "SPCSO1",
    (ModelNode*) &iedModel_GenericIO_GGIO1,
    (ModelNode*) &iedModel_GenericIO_GGIO1_SPCSO2,
    (ModelNode*) &iedModel_GenericIO_GGIO1_SPCSO1_origin,
    0
};

  while (running) {
    // Use poller here so we can break out of this thread quickly if program is
    // canceled.
    zsock_t *sock = (zsock_t*) zpoller_wait(poller, 5000);

    if (sock == subscriber) {
      char *filter;
      char *data;

      if (zstr_recvx(sock, &filter, &data, NULL) != 2) {
        printf("[I/O] expected more messages\n");
        continue;
      }

      json_object *root;
      envelope_t env;

      envelope_unmarshal(data, root, &env);

      if (strcmp(c.name, env.metadata.sender) == 0) {
        continue;
      }

      if (strcmp("Update", env.kind) != 0) {
        continue;
      }

      update_t update;

      update_unmarshal(env.contents, &update);

      if (strlen(update.recipient) > 0 && strcmp(c.name, update.recipient) != 0) {
        continue;
      }

      for (int i = 0; i < pubs->size; i++) {
        const char *tag = pubs->tags[i];

        for (int j = 0; j < update.size; j++) {
          point_t point = update.updates[j];

          if (strcmp(tag, point.tag) == 0) {
            printf("updating publication %s to %f\n", pubs->names[i], point.value);

            pubs->values[i] = point.value;
            pubs->updates[i] = true;

            break;
          }
        }
      }

      update_free(&update);
      json_object_put(root);
    }
  }
}