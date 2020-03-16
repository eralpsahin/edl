#include "Enclave_u.h"
#include <errno.h>

typedef struct ms_get_length_t
{
  unsigned int ms_retval;
  ccptr ms_str;
} ms_get_length_t;

static const struct
{
  size_t nr_ocall;
  void *table[1];
} ocall_table_Enclave = {
    0,
    {NULL},
};
sgx_status_t get_length(sgx_enclave_id_t eid, unsigned int *retval, ccptr str)
{
  sgx_status_t status;
  ms_get_length_t ms;
  ms.ms_str = str;
  status = sgx_ecall(eid, 0, &ocall_table_Enclave, &ms);
  if (status == SGX_SUCCESS && retval)
    *retval = ms.ms_retval;
  return status;
}
