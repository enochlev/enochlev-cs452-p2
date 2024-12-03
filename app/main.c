#include <stdio.h>
#include "../src/lab.h"

int main(void)
{
  printf("Starting to test buddy_malloc\n");
  //empty argv and argc
  char *argv[] = {""};
  int argc = 1;
  myMain(argc, argv);
  return 0;
}
