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

typedef struct ms_encrypt_t
{
  char *ms_plaintext;
  int ms_sz;
} ms_encrypt_t;

typedef struct ms_initkey_t
{
  int ms_sz;
} ms_initkey_t;

static sgx_status_t SGX_CDECL sgx_encrypt(void *pms)
{
  CHECK_REF_POINTER(pms, sizeof(ms_encrypt_t));
  //
  // fence after pointer checks
  //
  sgx_lfence();
  ms_encrypt_t *ms = SGX_CAST(ms_encrypt_t *, pms);
  sgx_status_t status = SGX_SUCCESS;
  char *_tmp_plaintext = ms->ms_plaintext;
  size_t _len_plaintext = sizeof(char);
  char *_in_plaintext = NULL;

  CHECK_UNIQUE_POINTER(_tmp_plaintext, _len_plaintext);

  //
  // fence after pointer checks
  //
  sgx_lfence();

  if (_tmp_plaintext != NULL && _len_plaintext != 0)
  {
    if (_len_plaintext % sizeof(*_tmp_plaintext) != 0)
    {
      status = SGX_ERROR_INVALID_PARAMETER;
      goto err;
    }
    _in_plaintext = (char *)malloc(_len_plaintext);
    if (_in_plaintext == NULL)
    {
      status = SGX_ERROR_OUT_OF_MEMORY;
      goto err;
    }

    if (memcpy_s(_in_plaintext, _len_plaintext, _tmp_plaintext, _len_plaintext))
    {
      status = SGX_ERROR_UNEXPECTED;
      goto err;
    }
  }

  encrypt(_in_plaintext, ms->ms_sz);

err:
  if (_in_plaintext)
    free(_in_plaintext);
  return status;
}

static sgx_status_t SGX_CDECL sgx_initkey(void *pms)
{
  CHECK_REF_POINTER(pms, sizeof(ms_initkey_t));
  //
  // fence after pointer checks
  //
  sgx_lfence();
  ms_initkey_t *ms = SGX_CAST(ms_initkey_t *, pms);
  sgx_status_t status = SGX_SUCCESS;

  initkey(ms->ms_sz);

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
  } ecall_table[2];
} g_ecall_table = {
    2,
    {
        {(void *)(uintptr_t)sgx_encrypt, 0, 0},
        {(void *)(uintptr_t)sgx_initkey, 0, 0},
    }};

SGX_EXTERNC const struct
{
  size_t nr_ocall;
} g_dyn_entry_table = {
    0,
};
