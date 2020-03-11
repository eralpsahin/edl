#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "test_encrypt.h"

void ocall_printf(int* secret) {
  int* digit = malloc(sizeof(int));
  get_secret_digit_count(secret, digit);
  
  printf("%d\n",*digit);
 
}

int main() {
  int* i = malloc(sizeof(int));
  *i = 0;
  generate_secret_value(i);
  return 0;
}