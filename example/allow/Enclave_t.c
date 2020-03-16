#include "Enclave_t.h"

#include "sgx_trts.h" /* for sgx_ocalloc, sgx_is_outside_enclave */
#include "sgx_lfence.h" /* for sgx_lfence */

#include <errno.h>
#include <mbusafecrt.h> /* for memcpy_s etc */
#include <stdlib.h> /* for malloc/free etc */

#define CHECK_REF_POINTER(ptr, siz) do {	\
	if (!(ptr) || ! sgx_is_outside_enclave((ptr), (siz)))	\
		return SGX_ERROR_INVALID_PARAMETER;\
} while (0)

#define CHECK_UNIQUE_POINTER(ptr, siz) do {	\
	if ((ptr) && ! sgx_is_outside_enclave((ptr), (siz)))	\
		return SGX_ERROR_INVALID_PARAMETER;\
} while (0)

#define CHECK_ENCLAVE_POINTER(ptr, siz) do {	\
	if ((ptr) && ! sgx_is_within_enclave((ptr), (siz)))	\
		return SGX_ERROR_INVALID_PARAMETER;\
} while (0)

#define ADD_ASSIGN_OVERFLOW(a, b) (	\
	((a) += (b)) < (b)	\
)


typedef struct ms_generate_secret_value_t {
	int* ms_i;
} ms_generate_secret_value_t;

typedef struct ms_get_secret_digit_count_t {
	int* ms_secret;
	int* ms_digit;
} ms_get_secret_digit_count_t;

typedef struct ms_ocall_printf_t {
	int* ms_secret;
} ms_ocall_printf_t;

static sgx_status_t SGX_CDECL sgx_generate_secret_value(void* pms)
{
	CHECK_REF_POINTER(pms, sizeof(ms_generate_secret_value_t));
	//
	// fence after pointer checks
	//
	sgx_lfence();
	ms_generate_secret_value_t* ms = SGX_CAST(ms_generate_secret_value_t*, pms);
	sgx_status_t status = SGX_SUCCESS;
	int* _tmp_i = ms->ms_i;
	size_t _len_i = sizeof(int);
	int* _in_i = NULL;

	CHECK_UNIQUE_POINTER(_tmp_i, _len_i);

	//
	// fence after pointer checks
	//
	sgx_lfence();

	if (_tmp_i != NULL && _len_i != 0) {
		if ( _len_i % sizeof(*_tmp_i) != 0)
		{
			status = SGX_ERROR_INVALID_PARAMETER;
			goto err;
		}
		if ((_in_i = (int*)malloc(_len_i)) == NULL) {
			status = SGX_ERROR_OUT_OF_MEMORY;
			goto err;
		}

		memset((void*)_in_i, 0, _len_i);
	}

	generate_secret_value(_in_i);
	if (_in_i) {
		if (memcpy_s(_tmp_i, _len_i, _in_i, _len_i)) {
			status = SGX_ERROR_UNEXPECTED;
			goto err;
		}
	}

err:
	if (_in_i) free(_in_i);
	return status;
}

static sgx_status_t SGX_CDECL sgx_get_secret_digit_count(void* pms)
{
	CHECK_REF_POINTER(pms, sizeof(ms_get_secret_digit_count_t));
	//
	// fence after pointer checks
	//
	sgx_lfence();
	ms_get_secret_digit_count_t* ms = SGX_CAST(ms_get_secret_digit_count_t*, pms);
	sgx_status_t status = SGX_SUCCESS;
	int* _tmp_secret = ms->ms_secret;
	size_t _len_secret = sizeof(int);
	int* _in_secret = NULL;
	int* _tmp_digit = ms->ms_digit;
	size_t _len_digit = sizeof(int);
	int* _in_digit = NULL;

	CHECK_UNIQUE_POINTER(_tmp_secret, _len_secret);
	CHECK_UNIQUE_POINTER(_tmp_digit, _len_digit);

	//
	// fence after pointer checks
	//
	sgx_lfence();

	if (_tmp_secret != NULL && _len_secret != 0) {
		if ( _len_secret % sizeof(*_tmp_secret) != 0)
		{
			status = SGX_ERROR_INVALID_PARAMETER;
			goto err;
		}
		_in_secret = (int*)malloc(_len_secret);
		if (_in_secret == NULL) {
			status = SGX_ERROR_OUT_OF_MEMORY;
			goto err;
		}

		if (memcpy_s(_in_secret, _len_secret, _tmp_secret, _len_secret)) {
			status = SGX_ERROR_UNEXPECTED;
			goto err;
		}

	}
	if (_tmp_digit != NULL && _len_digit != 0) {
		if ( _len_digit % sizeof(*_tmp_digit) != 0)
		{
			status = SGX_ERROR_INVALID_PARAMETER;
			goto err;
		}
		if ((_in_digit = (int*)malloc(_len_digit)) == NULL) {
			status = SGX_ERROR_OUT_OF_MEMORY;
			goto err;
		}

		memset((void*)_in_digit, 0, _len_digit);
	}

	get_secret_digit_count(_in_secret, _in_digit);
	if (_in_secret) {
		if (memcpy_s(_tmp_secret, _len_secret, _in_secret, _len_secret)) {
			status = SGX_ERROR_UNEXPECTED;
			goto err;
		}
	}
	if (_in_digit) {
		if (memcpy_s(_tmp_digit, _len_digit, _in_digit, _len_digit)) {
			status = SGX_ERROR_UNEXPECTED;
			goto err;
		}
	}

err:
	if (_in_secret) free(_in_secret);
	if (_in_digit) free(_in_digit);
	return status;
}

SGX_EXTERNC const struct {
	size_t nr_ecall;
	struct {void* ecall_addr; uint8_t is_priv; uint8_t is_switchless;} ecall_table[2];
} g_ecall_table = {
	2,
	{
		{(void*)(uintptr_t)sgx_generate_secret_value, 0, 0},
		{(void*)(uintptr_t)sgx_get_secret_digit_count, 1, 0},
	}
};

SGX_EXTERNC const struct {
	size_t nr_ocall;
	uint8_t entry_table[1][2];
} g_dyn_entry_table = {
	1,
	{
		{0, 1, },
	}
};


sgx_status_t SGX_CDECL ocall_printf(int* secret)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_printf_t* ms = NULL;
	size_t ocalloc_size = sizeof(ms_ocall_printf_t);
	void *__tmp = NULL;


	__tmp = sgx_ocalloc(ocalloc_size);
	if (__tmp == NULL) {
		sgx_ocfree();
		return SGX_ERROR_UNEXPECTED;
	}
	ms = (ms_ocall_printf_t*)__tmp;
	__tmp = (void *)((size_t)__tmp + sizeof(ms_ocall_printf_t));
	ocalloc_size -= sizeof(ms_ocall_printf_t);

	ms->ms_secret = secret;
	status = sgx_ocall(0, ms);

	if (status == SGX_SUCCESS) {
	}
	sgx_ocfree();
	return status;
}

