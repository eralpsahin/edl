#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Enclave.h"

void greeter(const char* str) {
  printf("%s", str);
  printf(", welcome!\n");
}

int main() {
  char username[20], text[1024];
  printf("Enter username: ");
  scanf("%19s", username);
  printf("Enter plaintext: ");
  scanf("%1023s", text);

  initkey(username, strlen(text));
  char* ciphertext = encrypt(text, strlen(text));
  printf("Cipher text: ");
  for (int i = 0; i < strlen(text); i++) printf("%x ", ciphertext[i]);
  return 0;
}
