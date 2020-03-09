#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_encrypt.h"

void ocall_func(const char * str) {
  printf("%s\n", str);

  mysecret* secret = malloc(sizeof(mysecret));
  cptr a;
  carry b;
  typedefParams(a, b, secret);
  ecall_private();
}

int main() {
  char username[20], text[1024];

  ecall_root(10);

  return 0;
}
