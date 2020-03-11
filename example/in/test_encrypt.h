char __attribute__((annotate("sensitive"))) * key;
char* ciphertext;
unsigned int i;

void initkey(int sz);
void encrypt(char* plaintext, int sz);
