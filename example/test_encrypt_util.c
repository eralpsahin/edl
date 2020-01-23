#include <stdio.h>
#include <stdlib.h>

#include "test_encrypt.h"

extern char __attribute__((annotate("sensitive"))) * key;
extern char *ciphertext;
extern unsigned int i;

void greeter(char *str) {
  printf("%s\n", str);
  printf(", welcome!\n");
}

void initkey(int sz) {
  key = (char *)(malloc(sz));
  // init the key randomly; code omitted
  for (i = 0; i < sz; i++) key[i] = 1;
}

void encrypt(char *plaintext, int sz) {
  ciphertext = (char *)(malloc(sz));
  for (i = 0; i < sz; i++) ciphertext[i] = plaintext[i] ^ key[i];
}
