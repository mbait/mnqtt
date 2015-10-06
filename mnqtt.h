#ifndef MNQTT_H
# define MNQTT_H

#include <stdint.h>
#include <sys/uio.h>


#define MQTT_TYPE_CONNECT       1
#define MQTT_TYPE_PUBLISH       3
#define MQTT_TYPE_PUBACK        4
#define MQTT_TYPE_PUBREC        5
#define MQTT_TYPE_PUBREL        6
#define MQTT_TYPE_PUBCOMP       7
#define MQTT_TYPE_SUBSCRIBE     8
#define MQTT_TYPE_SUBACK        9
#define MQTT_TYPE_UNSUBSCRIBE  10
#define MQTT_TYPE_UNSUBACK     11
#define MQTT_TYPE_PINGREQ      12
#define MQTT_TYPE_PINGRESP     13
#define MQTT_TYPE_DISCONNECT   14

#define MQTT_FLAG_RET   (1 << 0)
#define MQTT_FLAG_QOS0  0
#define MQTT_FLAG_QOS1  (1 << 1)
#define MQTT_FLAG_QOS2  (1 << 2)
#define MQTT_FLAG_DUP   (1 << 3)

#define MQTT_CONNECT_CLEAN        (1 << 1)
#define MQTT_CONNECT_WILL         (1 << 2)
#define MQTT_CONNECT_WILL_QOS0    MQTT_CONNECT_WILL
#define MQTT_CONNECT_WILL_QOS1    ((1 << 3 | MQTT_CONNECT_WILL)
#define MQTT_CONNECT_WILL_QOS2    ((1 << 4 | MQTT_CONNECT_WILL)
#define MQTT_CONNECT_WILL_RETAIN  ((1 << 5 | MQTT_CONNECT_WILL)
#define MQTT_CONNECT_PASSWORD     (1 << 6)
#define MQTT_CONNECT_USER         (1 << 7)

#define MNQTT_STR(v,s)							\
  struct { uint16_t s; char c[sizeof s - 1]; } v = { sizeof s - 1, s }


struct mnqtt_str
{
  uint16_t len;
  char buf[];
};

/**
 * MQTT message body.
 */
struct mnqtt_msg
{
  struct mnqtt_str *topic;
  const void *data;
  size_t size;
};

/**
 * MQTT message header.
 */
struct mnqtt_hdr
{
  uint16_t mid;
  uint8_t flags;
};


struct mnqtt
{
#ifndef MNQTT_SEND
  ssize_t (*send) (const void *buf, size_t len);
  ssize_t (*sendv) (const struct iovec *, int);
#endif  
  /* callbacks */							
  void (*conn_cb) (struct mnqtt *, uint8_t );				
  void (*ack_cb)  (struct mnqtt *, uint8_t, uint16_t, const uint8_t *);
  void (*msg_cb)  (struct mnqtt *, struct mnqtt_hdr *, struct mnqtt_msg *);
  void (*err_cb)  (struct mnqtt *);
    
  void *user;

  uint8_t msg_type_flags;
  uint8_t var_header[2];
  int rem_len;
  int rem_len_n;
};


struct mnqtt_conn
{
  struct mnqtt_msg will;  
  struct mnqtt_str *id;
  struct mnqtt_str *user;
  struct mnqtt_str *passwd;
  int timeout;
  int flags;
};

/**
 * Processes a chunk of data.
 */
size_t  mnqtt_recv (struct mnqtt *, const void *, size_t);
/**
 * Sends CONNECT message.
 */
ssize_t mnqtt_conn (struct mnqtt *, struct mnqtt_conn *);
/**
 * Sends DISCONNECT message.
 */
ssize_t mnqtt_disc (struct mnqtt *);
/**
 * Sends PINGREQ message.
 */
ssize_t mnqtt_ping (struct mnqtt *);
/**
 * If topics data cannot be fit into stack, the result will be the same as when
 * there is stack overflow.
 */
ssize_t mnqtt_sub (struct mnqtt *, struct mnqtt_hdr *,
		   struct mnqtt_str *, uint8_t *, size_t);
/**
 * If topics data cannot be fit into stack, the result will be the same as when
 * there is stack overflow.
 */
ssize_t mnqtt_uns (struct mnqtt *, struct mnqtt_hdr *,
		   struct mnqtt_str *, size_t);
ssize_t mnqtt_pub (struct mnqtt *, const struct mnqtt_hdr *,
		   const struct mnqtt_msg *);

#endif	/* MNQTT_H */
