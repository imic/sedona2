#include <stdint.h>

#include "log.h"
#include "mqtt_map.h"

static struct mqtt_slot_entry *entries = NULL;

void mqtt_add_slot_entry(struct mosquitto *session, MQTT_SLOT_KEY_TYPE offset,
                         uint8_t tid, uint8_t sid, const char *path) {
  struct mqtt_slot_entry *s;

  HASH_FIND_INT(entries, &offset, s); /* slot already in the hash? */
  if (s == NULL) {
    s = (struct mqtt_slot_entry *)malloc(sizeof(struct mqtt_slot_entry));
    s->session = session;
    s->offset = offset;
    s->slot = sid;
    s->tid = tid;

    log_info("Added slot entry %s (%d)", path, strlen(path));
    // s->path = malloc((strlen(path) + 1) * sizeof(char));
    strncpy((char *)s->path, (const char *)(path + 1), MAX_PATH_LEN);

    HASH_ADD_INT(entries, offset, s); /* slot: name of key field */
  }
}

struct mqtt_slot_entry *mqtt_find_slot_entry(MQTT_SLOT_KEY_TYPE offset) {
  struct mqtt_slot_entry *s;

  HASH_FIND_INT(entries, &offset, s); /* s: output pointer */
  return s;
}
