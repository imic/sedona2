#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <sys/time.h>
#include <time.h>
#else
#include <process.h>
#include <winsock2.h>
#define snprintf sprintf_s
#endif

#include <mosquitto.h>
#include <mqtt_protocol.h>

#include "constants.h"
#include "mqtt_client.h"

static volatile int status = STATUS_CONNECTING;
static int connack_result = 0;

void mqtt_log_callback(struct mosquitto *mosq, void *obj, int level,
                       const char *str) {
  UNUSED(mosq);
  UNUSED(obj);
  UNUSED(level);

  printf("%s\n", str);
}

void mqtt_connect_callback_v5(struct mosquitto *mosq, void *obj, int result,
                              int flags, const mosquitto_property *properties) {
  int rc = MOSQ_ERR_SUCCESS;

  UNUSED(obj);
  UNUSED(flags);
  UNUSED(properties);

  // connack_result = result;

  if (!result) {
    /*
            switch(cfg.pub_mode){
                    case MSGMODE_CMD:
                    case MSGMODE_FILE:
                    case MSGMODE_STDIN_FILE:
                            //rc = my_publish(mosq, &mid_sent, cfg.topic,
       cfg.msglen, cfg.message, cfg.qos, cfg.retain); break; case MSGMODE_NULL:
                            rc = my_publish(mosq, &mid_sent, cfg.topic, 0, NULL,
       cfg.qos, cfg.retain); break; case MSGMODE_STDIN_LINE: status =
       STATUS_CONNACK_RECVD; break;
            }*/
    if (rc) {
      /*
    switch (rc) {
    case MOSQ_ERR_INVAL:
      err_printf(
          &cfg,
          "Error: Invalid input. Does your topic contain '+' or '#'?\n");
      break;
    case MOSQ_ERR_NOMEM:
      err_printf(&cfg,
                 "Error: Out of memory when trying to publish message.\n");
      break;
    case MOSQ_ERR_NO_CONN:
      err_printf(&cfg,
                 "Error: Client not connected when trying to publish.\n");
      break;
    case MOSQ_ERR_PROTOCOL:
      err_printf(&cfg,
                 "Error: Protocol error when communicating with broker.\n");
      break;
    case MOSQ_ERR_PAYLOAD_SIZE:
      err_printf(&cfg, "Error: Message payload is too large.\n");
      break;
    case MOSQ_ERR_QOS_NOT_SUPPORTED:
      err_printf(
          &cfg,
          "Error: Message QoS not supported on broker, try a lower QoS.\n");
      break;
    }
    */
      mosquitto_disconnect_v5(mosq, 0, /*cfg.disconnect_props*/ NULL);
    }
  } else {
    if (result) {
      /*
                  if(cfg.protocol_version == MQTT_PROTOCOL_V5){
                          if(result == MQTT_RC_UNSUPPORTED_PROTOCOL_VERSION){
                                  err_printf(&cfg, "Connection error: %s. Try
         connecting to an MQTT v5 broker, or use MQTT v3.x mode.\n",
         mosquitto_reason_string(result)); }else{ err_printf(&cfg, "Connection
         error: %s\n", mosquitto_reason_string(result));
                          }
                  }else{
                          err_printf(&cfg, "Connection error: %s\n",
         mosquitto_connack_string(result));
                  }*/
      /* let the loop know that this is an unrecoverable connection */
      status = STATUS_NOHOPE;
    }
  }
}

void mqtt_connect_callback(struct mosquitto *mosq, void *obj, int result) {
  mqtt_connect_callback_v5(mosq, obj, result, 0, NULL);
}

void mqtt_disconnect_callback_v5(struct mosquitto *mosq, void *obj, int rc,
                                 const mosquitto_property *properties) {
  UNUSED(mosq);
  UNUSED(obj);
  UNUSED(rc);
  UNUSED(properties);

  if (rc == 0) {
    status = STATUS_DISCONNECTED;
  }
}

void mqtt_disconnect_callback(struct mosquitto *mosq, void *obj, int rc) {
  mqtt_disconnect_callback_v5(mosq, obj, rc, NULL);
}

///////////////////////////////////////////////////////
// Native Method Slots
///////////////////////////////////////////////////////
Cell cbcmw_CbcMiddlewareService_startSession(SedonaVM *vm, Cell *params) {
  char *host = params[0].aval;
  int32_t port = params[1].ival;
  char *clientid = params[2].aval;
  char *username = params[3].aval;
  char *password = params[4].aval;

  struct mosquitto *mosq = NULL;
  int rc;

  mosquitto_lib_init();

  mosq = mosquitto_new(MW_CLIENT_NAME, MW_CLEAN_SESSION, NULL);
  if (!mosq) {
    /*
            switch(errno){
                    case ENOMEM:
                            err_printf(&cfg, "Error: Out of memory.\n");
                            break;
                    case EINVAL:
                            err_printf(&cfg, "Error: Invalid id.\n");
                            break;
            }
            goto cleanup;
    */
    status = STATUS_INTERNAL_ERROR;
  } else {
    mosquitto_log_callback_set(mosq, mqtt_log_callback);

    mosquitto_connect_callback_set(mosq, mqtt_connect_callback);
    mosquitto_disconnect_callback_set(mosq, mqtt_disconnect_callback);
    /*
    mosquitto_loop_start(mosq);
    mosquitto_connect_async(mosq, host, port, -1);
    */
    rc = mosquitto_connect(mosq, host, port, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
      if (rc == MOSQ_ERR_INVAL) {
        printf("ERROR: Invalid parameters %s (%d)\n", host, port);
      } else {
        printf("ERROR: Illegal call (rc = %d)\n", rc);
      }
    }
  }

  Cell result;
  result.aval = (void *)mosq;
  return result;
}

Cell cbcmw_CbcMiddlewareService_stopSession(SedonaVM *vm, Cell *params) {
  struct mosquitto *mosq = (struct mosquitto *)params[0].aval;
  if (!mosq)
    return falseCell;

  mosquitto_disconnect(mosq);

  return nullCell;
}

Cell cbcmw_CbcMiddlewareService_isSessionLive(SedonaVM *vm, Cell *params) {
  struct mosquitto *mosq = (struct mosquitto *)params[0].aval;
  if (!mosq)
    return falseCell;

  return (status == STATUS_WAITING) ? trueCell : falseCell;
}

Cell cbcmw_CbcMiddlewareService_execute(SedonaVM *vm, Cell *params) {
  struct mosquitto *mosq = (struct mosquitto *)params[0].aval;

  // Parameters
  // - mosq	a valid mosquitto instance.
  // - timeout	Maximum number of milliseconds to wait for network activity in
  // the select() call before timing out.  Set to 0 for instant return.  Set
  // negative to use the default of 1000ms.
  // - max_packets	this parameter is currently unused and should be set to
  // 1 for future compatibility.
  mosquitto_loop(mosq, -1, 1);
  return trueCell;
}