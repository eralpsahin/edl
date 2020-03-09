typedef struct {
  int key;
  const char* text;
} mysecret;
typedef char* cptr;
typedef char carry[10];
int typedefParams(cptr a, carry b, mysecret* secret);
void ecall_root(int sz);
void ecall_private(void);