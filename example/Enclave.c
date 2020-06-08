#include "Enclave.h"

#include <stdio.h>
#include <stdlib.h>

char *key;

void initkey(const char *username, int sz) {
  greeter(username);
  key = (char *)(malloc(sz));
  // init the key randomly; code omitted
  for (int i = 0; i < sz; i++) key[i] = 1;
}

char *encrypt(char *plaintext, int sz) {
  char *ciphertext = (char *)(malloc(sz));
  for (int i = 0; i < sz; i++) ciphertext[i] = plaintext[i] ^ key[i];
  return ciphertext;
}
