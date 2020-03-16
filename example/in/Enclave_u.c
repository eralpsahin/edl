#include "Enclave_u.h"
#include <errno.h>

typedef struct ms_encrypt_t
{
  char *ms_plaintext;
  int ms_sz;
} ms_encrypt_t;

typedef struct ms_initkey_t
{
  int ms_sz;
} ms_initkey_t;

static const struct
{
  size_t nr_ocall;
  void *table[1];
} ocall_table_Enclave = {
    0,
    {NULL},
};
sgx_status_t encrypt(sgx_enclave_id_t eid, char *plaintext, int sz)
{
  sgx_status_t status;
  ms_encrypt_t ms;
  ms.ms_plaintext = plaintext;
  ms.ms_sz = sz;
  status = sgx_ecall(eid, 0, &ocall_table_Enclave, &ms);
  return status;
}

sgx_status_t initkey(sgx_enclave_id_t eid, int sz)
{
  sgx_status_t status;
  ms_initkey_t ms;
  ms.ms_sz = sz;
  status = sgx_ecall(eid, 1, &ocall_table_Enclave, &ms);
  return status;
}
