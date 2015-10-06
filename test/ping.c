#include <mnqtt.h>

int
main (int argc, char *argv[])
{
  struct mnqtt mnqtt;
  mnqtt_ping (&mnqtt);  
  return 0;
}
