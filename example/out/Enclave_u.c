#include "Enclave_u.h"
#include <errno.h>

typedef struct ms_get_secret_value_t
{
  int *ms_i;
} ms_get_secret_value_t;

static const struct
{
  size_t nr_ocall;
  void *table[1];
} ocall_table_Enclave = {
    0,
    {NULL},
};
sgx_status_t get_secret_value(sgx_enclave_id_t eid, int *i)
{
  sgx_status_t status;
  ms_get_secret_value_t ms;
  ms.ms_i = i;
  status = sgx_ecall(eid, 0, &ocall_table_Enclave, &ms);
  return status;
}
