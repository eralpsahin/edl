#include <stdio.h>
#include <stdlib.h>

#include "test_encrypt.h"

void generate_secret_value(int* i) {
  *i = 42;
  ocall_printf(i);
}

void get_secret_digit_count(int* secret, int* digit) {
  int count = 0; 
    while (*secret != 0) { 
        *secret = *secret / 10; 
        ++count; 
    } 
    *digit = count;
}