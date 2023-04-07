/* SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/param.h>
#include "osmem.h"
#include "helpers.h"

#define FAIL(assertion, feedback)                                          \
	do                                                                     \
	{                                                                      \
		if (assertion)                                                     \
		{                                                                  \
			fprintf(stderr, "(%s, %d): %s", __FILE__, __LINE__, feedback); \
			exit(SIGABRT);                                                 \
		}                                                                  \
	} while (0)

#define METADATA_SIZE (sizeof(struct block_meta))
#define MOCK_PREALLOC (128 * 1024 - 32)
#define MMAP_THRESHOLD (128 * 1024)
#define NUM_SZ_SM 11
#define NUM_SZ_MD 6
#define NUM_SZ_LG 4
#define MULT_KB 1024

int inc_sz_sm[] = {10, 25, 40, 80, 160, 350, 421, 633, 1000, 2024, 4000};
int dec_sz_sm[] = {4023, 2173, 1077, 653, 438, 342, 160, 82, 44, 25, 10};
int alt_sz_sm[] = {1934, 3654, 23, 432, 824, 12, 2631, 827, 375, 30, 26};
int inc_sz_md[] = {5 * MULT_KB, 46 * MULT_KB + 145, 100 * MULT_KB + 732,
				   200 * MULT_KB, 523 * MULT_KB + 6342, 1000 * MULT_KB + 3754};
int inc_sz_lg[] = {200 * MULT_KB, 525 * MULT_KB + 6342, 1024 * MULT_KB, 5256 * MULT_KB + 12462};

void taint(void *ptr, size_t size)
{
	char buf[4096];
	int fd, bytes = 0, total = 0;

	fd = open("/dev/urandom", O_RDONLY);
	DIE(fd < 0, "open");

	while (total < 4096)
	{
		bytes = read(fd, buf + total, 4096 - total);
		DIE(bytes < 0, "read");
		total += bytes;
	}

	for (size_t i = 0; i < size; i += 4096)
		memcpy(ptr, buf, MIN(4096, size - i));
}

void *os_calloc_checked(size_t nmemb, size_t size)
{
	void *ptr = os_calloc(nmemb, size);
	if (nmemb * size != 0)
	{
		FAIL(ptr == NULL, "os_calloc returned NULL on valid size");
		for (unsigned int i = 0; i < size; i++)
			FAIL(*((char *)(ptr + i)) != 0, "os_calloc returned uninitialized memory");
	}

	return ptr;
}

void *os_malloc_checked(size_t size)
{
	void *ptr = os_malloc(size);
	if (size != 0)
		FAIL(ptr == NULL, "os_malloc returned NULL on valid size");

	return ptr;
}

void *os_realloc_checked(void *ptr, size_t size)
{
	void *ptr_realloc;
	struct block_meta oldBlock;

	if (!ptr)
		return os_realloc(ptr, size);

	memcpy(&oldBlock, ptr - sizeof(struct block_meta), sizeof(oldBlock));

	ptr_realloc = os_realloc(ptr, size);

	if (size == 0 || oldBlock.status == STATUS_FREE)
	{
		char buffer01[50];
		int len01 = sprintf(buffer01, "test01 - %d\n", size);
		write(1, buffer01, len01);

		char buffer02[50];
		int len02 = sprintf(buffer02, "test02 - %d\n", oldBlock.status);
		write(1, buffer02, len02);

		FAIL(ptr_realloc != NULL, "os_realloc returned NULL on valid size\n");
		return ptr_realloc;
	}

	return ptr_realloc;
}

void *mock_preallocate(void)
{
	return os_malloc(MOCK_PREALLOC);
}
