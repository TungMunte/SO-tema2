// SPDX-License-Identifier: BSD-3-Clause

#include "test-utils.h"

int main(void)
{
	void *prealloc_ptr, *ptrs[NUM_SZ_SM];

	prealloc_ptr = mock_preallocate();

	/* Test small allocations */
	for (int i = 0; i < NUM_SZ_SM; i++)
	{
		char buffer01[50];
		int len01 = sprintf(buffer01, "array01 - %d\n", inc_sz_sm[i]);
		write(1, buffer01, len01);
		ptrs[i] = os_realloc_checked(NULL, inc_sz_sm[i]);
	}

	/* Test block truncating on small allocations */
	for (int i = 1; i < NUM_SZ_SM; i++)
	{
		char buffer01[50];
		int len01 = sprintf(buffer01, "array02 - %d\n", inc_sz_sm[i - 1]);
		write(1, buffer01, len01);
		ptrs[i] = os_realloc_checked(ptrs[i], inc_sz_sm[i - 1]);
	}

	/* Test block expansion on small allocations */
	for (int i = 0; i < NUM_SZ_SM; i++)
	{
		char buffer01[50];
		int len01 = sprintf(buffer01, "array03 - %d\n", inc_sz_sm[i]);
		write(1, buffer01, len01);
		ptrs[i] = os_realloc_checked(ptrs[i], inc_sz_sm[i]);
	}

	/* Test mapped reallocations */
	for (int i = 0; i < NUM_SZ_LG; i++)
	{
		char buffer01[50];
		int len01 = sprintf(buffer01, "array04 - %d\n", inc_sz_lg[i]);
		write(1, buffer01, len01);
		ptrs[i] = os_realloc_checked(ptrs[i], inc_sz_lg[i]);
	}

	/* Test mixed reallocations */
	for (int i = 0; i < NUM_SZ_MD; i++)
	{
		char buffer01[50];
		int len01 = sprintf(buffer01, "array05 - %d\n", inc_sz_md[i]);
		write(1, buffer01, len01);
		ptrs[i] = os_realloc_checked(ptrs[i], inc_sz_md[i]);
	}

	/* Cleanup using realloc */
	char buffer01[50];
	int len01 = sprintf(buffer01, "cleaning---\n", 20);
	write(1, buffer01, len01);
	os_realloc_checked(prealloc_ptr, 0);
	for (int i = 0; i < NUM_SZ_SM; i++)
		os_realloc_checked(ptrs[i], 0);

	return 0;
}
