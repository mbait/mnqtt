#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <netdb.h>
#include <sys/socket.h>

#include "mnqtt.h"


static const char *
mqtt_strerror (int ret)
{
  switch (ret) {
  case 0:
    return "Connection Accepted";
  case 1:
    return "Connection Refused: unacceptable protocol version";
  case 2:
    return "Connection Refused: identifier rejected";
  case 3:
    return "Connection Refused: server unavailable";
  case 4:
    return "Connection Refused: bad user name or password";
  case 5:
    return "Connection Refused: not authorized";
  default:
    return "uknown errror";
  }
}


static void
conn_cb (struct mnqtt *mnqtt, int ret)
{
  fprintf(stderr, "connected (%s)\n", mqtt_strerror(ret));
  /* mnqtt_sub (mnqtt, 0, "/test", 0); */
  mnqtt_ping (mnqtt);
}


void
sub_cb (struct mnqtt *mnqtt)
{
  struct mnqtt_msg msg = {
    0,
    "/test",
    strlen ("/test"),
    "Hello, MQTT!",
    strlen ("Hello, MQTT!")
  };

  MNQTT_STR(test_topic, "/test");
  MNQTT_STR(x_topic, "/x");
  struct mnqtt_str *topics [] = {test_topic, x_topic};

  /* id, qos, dup */
  mnqtt_sub (mnqtt, header, topics, ARRAY_SIZE(topics));
  
  struct mnqtt_msg msg = { (struct mnqtt_str *) &topic, data, len };
  mnqtt_pub (mnqtt, &msg);
}


void
msg_cb (struct mnqtt *mnqtt)
{
  fprintf (stderr, "received message\n");  
}


static void
log_cb (struct mnqtt *mnqtt, enum mnqtt_log_lvl lvl, const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
}


int sockfd;


static ssize_t
send_cb (const struct iovec *iov, int iovcnt)
{
  return writev (sockfd, iov, iovcnt);
}


int
main()
{
  struct sockaddr_in addr;
  struct hostent *he = gethostbyname("localhost");
  if (-1 == (sockfd = socket(AF_INET, SOCK_STREAM, 0))) {
    perror("socket()");
    goto sock_err;
  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(1883);
  addr.sin_addr = * ((struct in_addr *) he->h_addr);
  memset (&(addr.sin_zero), '\0', 8);

  if (-1 == connect (sockfd, (struct sockaddr *) &addr, sizeof (struct sockaddr))) {
    perror("connect()");
    goto conn_err;
  }

  MNQTT_STR(will_topic, "/goodbye");
  MNQTT_STR(client_id, "foobar");

  struct mnqtt_conn conn;
  memset (&conn, 0, sizeof conn);
  conn.flags = MQTT_CONNECT_CLEAN;
  conn.id = client_id;
  conn.timeout = 60;
  
  struct mnqtt mnqtt = { send_cb, conn_cb, NULL, msg_cb };  
  mnqtt_conn (&mnqtt, &conn);

  ssize_t n;
  unsigned char buf[8192];
  while ((n = recv (sockfd, buf, sizeof buf, 0)) > 0) {
    mnqtt_recv (&mnqtt, buf, n);
  }

  if (n < 0) {
    /* TODO: report error */
  }

  close (sockfd);
  exit (EXIT_SUCCESS);

 conn_err:
  close(sockfd);
 sock_err:
  exit(EXIT_FAILURE);
}  
