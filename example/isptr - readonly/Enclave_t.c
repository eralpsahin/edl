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

typedef struct ms_get_length_t
{
  unsigned int ms_retval;
  ccptr ms_str;
} ms_get_length_t;

static sgx_status_t SGX_CDECL sgx_get_length(void *pms)
{
  CHECK_REF_POINTER(pms, sizeof(ms_get_length_t));
  //
  // fence after pointer checks
  //
  sgx_lfence();
  ms_get_length_t *ms = SGX_CAST(ms_get_length_t *, pms);
  sgx_status_t status = SGX_SUCCESS;
  ccptr _tmp_str = ms->ms_str;

  ms->ms_retval = get_length(_tmp_str);

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
        {(void *)(uintptr_t)sgx_get_length, 0, 0},
    }};

SGX_EXTERNC const struct
{
  size_t nr_ocall;
} g_dyn_entry_table = {
    0,
};
