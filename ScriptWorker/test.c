#include <stdio.h>
#include <string.h>
#define LAST 10

int main()
{
  char *start = "POST /CreateInstance HTTP/1.0\r\nHost: login.cloud.camunda.io\r\nUser-Agent: ESP32-CAM\r\nContent-Type: application/json\r\nAccept-encoding: *\r\n";
  char *end = "Content-Length: \r\n\r\n";
  char *final;
  printf("%d\n", strlen(start));
  int foo = sprintf(final, "%sContent-Length: %d", start, strlen(start));
  printf("string = %s\n", final);

  return 0;
}
