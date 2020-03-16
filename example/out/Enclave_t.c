#include "Enclave_t.h"

#include "sgx_trts.h"   /* for sgx_ocalloc, sgx_is_outside_enclave */
#include "sgx_lfence.h" /* for sgx_lfence */

#include <errno.h>
#include <mbusafecrt.h> /* for memcpy_s etc */
#include <stdlib.h>     /* for malloc/free etc */

#define CHECK_REF_POINTER(ptr, siz)                      \
  do                                                     \
  {                                                      \
    if (!(ptr) || !sgx_is_outside_enclave((ptr), (siz))) \
      return SGX_ERROR_INVALID_PARAMETER;                \
  } while (0)

#define CHECK_UNIQUE_POINTER(ptr, siz)                  \
  do                                                    \
  {                                                     \
    if ((ptr) && !sgx_is_outside_enclave((ptr), (siz))) \
      return SGX_ERROR_INVALID_PARAMETER;               \
  } while (0)

#define CHECK_ENCLAVE_POINTER(ptr, siz)                \
  do                                                   \
  {                                                    \
    if ((ptr) && !sgx_is_within_enclave((ptr), (siz))) \
      return SGX_ERROR_INVALID_PARAMETER;              \
  } while (0)

#define ADD_ASSIGN_OVERFLOW(a, b) ( \
    ((a) += (b)) < (b))

typedef struct ms_get_secret_value_t
{
  int *ms_i;
} ms_get_secret_value_t;

static sgx_status_t SGX_CDECL sgx_get_secret_value(void *pms)
{
  CHECK_REF_POINTER(pms, sizeof(ms_get_secret_value_t));
  //
  // fence after pointer checks
  //
  sgx_lfence();
  ms_get_secret_value_t *ms = SGX_CAST(ms_get_secret_value_t *, pms);
  sgx_status_t status = SGX_SUCCESS;
  int *_tmp_i = ms->ms_i;
  size_t _len_i = sizeof(int);
  int *_in_i = NULL;

  CHECK_UNIQUE_POINTER(_tmp_i, _len_i);

  //
  // fence after pointer checks
  //
  sgx_lfence();

  if (_tmp_i != NULL && _len_i != 0)
  {
    if (_len_i % sizeof(*_tmp_i) != 0)
    {
      status = SGX_ERROR_INVALID_PARAMETER;
      goto err;
    }
    if ((_in_i = (int *)malloc(_len_i)) == NULL)
    {
      status = SGX_ERROR_OUT_OF_MEMORY;
      goto err;
    }

    memset((void *)_in_i, 0, _len_i);
  }

  get_secret_value(_in_i);
  if (_in_i)
  {
    if (memcpy_s(_tmp_i, _len_i, _in_i, _len_i))
    {
      status = SGX_ERROR_UNEXPECTED;
      goto err;
    }
  }

err:
  if (_in_i)
    free(_in_i);
  return status;
}

SGX_EXTERNC const struct
{
  size_t nr_ecall;
  struct
  {
    void *ecall_addr;
    uint8_t is_priv;
    uint8_t is_switchless;
  } ecall_table[1];
} g_ecall_table = {
    1,
    {
        {(void *)(uintptr_t)sgx_get_secret_value, 0, 0},
    }};

SGX_EXTERNC const struct
{
  size_t nr_ocall;
} g_dyn_entry_table = {
    0,
};
