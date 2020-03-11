#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_encrypt.h"



int main() {

  char text[1024];

  printf("Length is: %d", get_length(text));
  return 0;
}
