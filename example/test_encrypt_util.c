#include <stdio.h>
#include <stdlib.h>

#include "test_encrypt.h"
int typedefParams(cptr a, carry b, mysecret* secret) {
  a = malloc(sizeof(char));

  secret->key = 1;
  return secret->key;
}

void ecall_private() {}

void ecall_root(int sz) { ocall_func("TEST"); }
