#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_encrypt.h"


int main() {
  int* i = malloc(sizeof(int));
  *i = 0;
  get_secret_value(i);
  printf("Secret: %d",*i);
  return 0;
}
