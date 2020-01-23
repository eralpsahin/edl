#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_encrypt.h"

char __attribute__((annotate("sensitive"))) * key;
char *ciphertext;
unsigned int i;

int main() {
  char username[20], text[1024];
  printf("Enter username: ");
  scanf("%19s", username);
  greeter(username);
  printf("Enter plaintext: ");
  scanf("%1023s", text);

  initkey(strlen(text));
  encrypt(text, strlen(text));
  printf("Cipher text: ");
  for (i = 0; i < strlen(text); i++) printf("%x ", ciphertext[i]);
  return 0;
}
