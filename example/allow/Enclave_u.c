#include "Enclave_u.h"
#include <errno.h>

typedef struct ms_generate_secret_value_t
{
  int *ms_i;
} ms_generate_secret_value_t;

typedef struct ms_get_secret_digit_count_t
{
  int *ms_secret;
  int *ms_digit;
} ms_get_secret_digit_count_t;

typedef struct ms_ocall_printf_t
{
  int *ms_secret;
} ms_ocall_printf_t;

static sgx_status_t SGX_CDECL Enclave_ocall_printf(void *pms)
{
  ms_ocall_printf_t *ms = SGX_CAST(ms_ocall_printf_t *, pms);
  ocall_printf(ms->ms_secret);

  return SGX_SUCCESS;
}

static const struct
{
  size_t nr_ocall;
  void *table[1];
} ocall_table_Enclave = {
    1,
    {
        (void *)Enclave_ocall_printf,
    }};
sgx_status_t generate_secret_value(sgx_enclave_id_t eid, int *i)
{
  sgx_status_t status;
  ms_generate_secret_value_t ms;
  ms.ms_i = i;
  status = sgx_ecall(eid, 0, &ocall_table_Enclave, &ms);
  return status;
}

sgx_status_t get_secret_digit_count(sgx_enclave_id_t eid, int *secret, int *digit)
{
  sgx_status_t status;
  ms_get_secret_digit_count_t ms;
  ms.ms_secret = secret;
  ms.ms_digit = digit;
  status = sgx_ecall(eid, 1, &ocall_table_Enclave, &ms);
  return status;
}
