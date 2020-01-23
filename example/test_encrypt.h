char __attribute__((annotate("sensitive"))) * key;
char *ciphertext;
unsigned int i;

void greeter(char *str);
void initkey(int sz);
void encrypt(char *plaintext, int sz);
