#include "config.h"
#include "mnqtt.h"

#include <arpa/inet.h>		/* For ntohs/htons. */


#ifdef ARRAY_SIZE
# undef ARRAY_SIZE
#endif
#define ARRAY_SIZE(a)  (sizeof (a) / sizeof ((a) [0]))

#ifndef MNQTT_SEND
# define MNQTT_SEND   mnqtt->send
# define MNQTT_SENDV  mnqtt->sendv
#endif


static void
err (struct mnqtt *mnqtt, uint8_t *buf, size_t n)
{
  mnqtt->err_cb (mnqtt);
}


static void
parse_connack (struct mnqtt *mnqtt, uint8_t *buf, size_t n)
{
  mnqtt->conn_cb (mnqtt, mnqtt->var_header [1]);
}


static void
send_ack (struct mnqtt *mnqtt, int type, uint16_t mid)
{
  uint16_t fixed_header [2] = { htons (type << 4 << 8 | 2), mid };  
  MNQTT_SEND (fixed_header, sizeof fixed_header);
}


static void
parse_publish (struct mnqtt *mnqtt, uint8_t *buf, size_t n)
{  
  /* TODO: message identifiers are only present if QoS > 0 */ 
  size_t topic_len = ntohs (* (uint16_t *) mnqtt->var_header);

  struct mnqtt_hdr hdr = {
    ntohs (* (uint16_t *) &mnqtt->var_header [topic_len + 2]),
    mnqtt->msg_type_flags & 0x0f /* flags */
  };

  int qos = (mnqtt->msg_type_flags & (MQTT_FLAG_QOS1 | MQTT_FLAG_QOS2)) >> 1;
  if (qos > 0) send_ack (mnqtt, MQTT_TYPE_PUBLISH + qos, hdr.mid);

  struct mnqtt_msg msg = {
    (struct mnqtt_str *) (mnqtt->var_header),		/* topic */
    (const void *) (mnqtt->var_header + topic_len + 4),	/* paylaod */
    mnqtt->rem_len - topic_len - 4			/* length */
  };

  mnqtt->msg_cb (mnqtt, &hdr, &msg);
}


static void
parse_ackcompresp (struct mnqtt *mnqtt, uint8_t *buf, size_t n)
{
  /* TODO: parse payload for SUBACK */
  uint16_t mid = * (uint16_t *) mnqtt->var_header;
  mnqtt->ack_cb (mnqtt, mnqtt->msg_type_flags >> 4, mid, 0);
}


static void
parse_recrel (struct mnqtt *mnqtt, uint8_t *buf, size_t n)
{
  uint16_t mid = ntohs (* (uint16_t *) mnqtt->var_header);
  int type = mnqtt->msg_type_flags >> 4;
  send_ack (mnqtt, type + 1, mid);
  mnqtt->ack_cb (mnqtt, type, mid, 0);
}


static void (*parse []) (struct mnqtt *, uint8_t *, size_t)  =
{
  err,  			/* 0 */
  parse_connack,		/* 1 */
  parse_publish,		/* 2 */
  parse_ackcompresp,		/* 3 */
  parse_recrel,			/* 4 */
};


static uint8_t parse_idx [] = { 0, 0, 1, 2, 3, 4, 4, 3, 0, 3, 0, 3, 0, 3, 0, 0 };

/* 0, 3, 0, 3, 0, 3, 0, 0 */
/* 0, 0, 1, 2, 3, 4, 4, 3, */
  

size_t
mnqtt_recv (struct mnqtt *mnqtt, const void *data, size_t n)
{
  uint8_t *buf = (uint8_t *) data;
  while (n > 0) {
    --n;
    uint8_t b = *buf++;
    if (mnqtt->msg_type_flags) {
      /* TODO: infinite loop */
      do {
	mnqtt->rem_len += (b & 0x7f) << mnqtt->rem_len_n;
	mnqtt->rem_len_n += 7;

	if ((b & 0x80) == 0) {
	  /* TODO: variable header storage space */
	  parse [parse_idx [mnqtt->msg_type_flags >> 4]] (mnqtt, buf, n);
	  mnqtt->msg_type_flags = 0;
	  break;
	}
      } while (n > 0);
    } else {
      mnqtt->rem_len = 0;
      mnqtt->rem_len_n = 0;
      mnqtt->msg_type_flags = b;
    }
  }
}


static size_t
enc_rem_len (uint8_t *rem_len, int32_t len)
{  
  uint8_t *p = rem_len + 1;
  size_t n = 0;
  for (;;) {
    *p = len % 128;
    len /= 128;
    ++n;
    if (len == 0) break;
    *p |= 0x80;
  } 
  return n;
}


ssize_t
mnqtt_conn (struct mnqtt *mnqtt, struct mnqtt_conn *conn)
{
  /* CONNECT is always the first message, so initialize everything here. */
  mnqtt->msg_type_flags = 0;

  uint8_t var_header [] = "\x00\x06MQIsdp\x03\x00\x00\x00";
  var_header [9] = conn->flags << 2;
  * (uint16_t *) &var_header [10] = htons (conn->timeout);
  
  /* headers + payload */
  /* TODO: constant 2, while can not be present in header */
  
  int32_t rem_len = sizeof var_header + conn->id->len +
    2 + conn->will.topic->len +
    2 + conn->will.size;
  
  uint8_t fixed_header [5] = { MQTT_TYPE_CONNECT << 4 | conn->flags };

  struct mnqtt_msg *will = &conn->will;
  will->size = htons (will->size);
  
  struct iovec iov [] = {
    { fixed_header, enc_rem_len (fixed_header, rem_len) },
    { var_header, sizeof var_header },
    { conn->id, conn->id->len },	 /* client ID */
    { will->topic, will->topic->len },	 /* will topic */
    { &will->size, sizeof will->size },	 /* will message length */
    { (void *) will->data, will->size }, /* will message */
    { conn->user, conn->user->len },	 /* user */
    { conn->passwd, conn->passwd->len }	 /* password */
  };

  return MNQTT_SENDV (iov, ARRAY_SIZE (iov));
}


ssize_t
mnqtt_ping (struct mnqtt *mnqtt)
{
  static const uint8_t fixed_header [2] = { MQTT_TYPE_PINGREQ << 4, 0 };
  return MNQTT_SEND (fixed_header, sizeof fixed_header);
}


ssize_t
mnqtt_disc (struct mnqtt *mnqtt)
{
  static const uint8_t fixed_header [2] = { MQTT_TYPE_DISCONNECT << 4, 0 };  
  return MNQTT_SEND (fixed_header, sizeof fixed_header);
}


ssize_t
mnqtt_sub (struct mnqtt *mnqtt, struct mnqtt_hdr *hdr,
	   struct mnqtt_str *topic, uint8_t *qos, size_t n)
{
  int32_t rem_len = 0;
  uint8_t fixed_header [5] = { MQTT_TYPE_SUBSCRIBE << 4 | hdr->flags };
  struct iovec iov [4 * n];

  int i;
  for (i = 0; i < n; ++i) {
    rem_len += sizeof *topic + topic [i].len;
    iov [0].iov_base = topic + i;
    iov [0].iov_len = sizeof *topic + topic [i].len;
  }

  return MNQTT_SENDV (iov, ARRAY_SIZE (iov));
}


ssize_t
mnqtt_uns (struct mnqtt *mnqtt, struct mnqtt_hdr *hdr,
	   struct mnqtt_str *topic, size_t n)
{
  int32_t rem_len = sizeof hdr->mid;
  uint8_t fixed_header [5] = { MQTT_TYPE_UNSUBSCRIBE << 4 || hdr->flags };
  struct iovec iov [3 * n + 1];

  iov [0].iov_base = &hdr->mid;
  iov [0].iov_len = sizeof hdr->mid;  

  int i;
  for (i = 0; i < n; ++i) {
    rem_len += sizeof *topic + topic [i].len;
    iov [0].iov_base = topic + i;
    iov [0].iov_len = sizeof *topic + topic [i].len;
  }

  return MNQTT_SENDV (iov, ARRAY_SIZE (iov));
}


ssize_t
mnqtt_pub (struct mnqtt * mnqtt,
	   const struct mnqtt_hdr *hdr, const struct mnqtt_msg *msg)
{
  int32_t rem_len = 2 + msg->topic->len + sizeof hdr->mid + msg->size;  
  uint8_t fixed_header [5] = { MQTT_TYPE_PUBLISH << 4 | hdr->flags };

  struct iovec iov [] = {
    { fixed_header, enc_rem_len (fixed_header, rem_len) },
    { (void *) msg->topic, msg->topic->len }, /* topic */
    { (void *) &hdr->mid, sizeof hdr->mid },  /* message id */
    { (void *) msg->data, msg->size }	      /* payload */
  };

  return MNQTT_SENDV (iov, ARRAY_SIZE (iov));
}
