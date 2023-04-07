// SPDX-License-Identifier: BSD-3-Clause

#include "osmem.h"
#include "helpers.h"

#define MMAP_THRESHOLD (128 * 1024)
#define ALIGNMENT 8 // must be a power of 2
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))
#define SIZE_T_SIZE (ALIGN(sizeof(size_t))) // header size
#define PAGE_SIZE 4096
#define BLOCK_SIZE ALIGN(sizeof(struct block_meta))

static struct block_meta *demo = NULL;
static int count = 0;

void *os_malloc(size_t size)
{
	/* TODO: Implement os_malloc */
	// char buffer01[50];
	// int len01 = sprintf(buffer01, "malloc - %d\n", size);
	// write(1, buffer01, len01);
	if (size == 0)
	{
		return NULL;
	}
	else if (count == 0)
	{
		count++;
		if (size >= MMAP_THRESHOLD)
		{
			void *res = mmap(NULL, ALIGN(size + sizeof(struct block_meta)), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			demo = (struct block_meta *)res;
			demo->size = size;
			demo->status = STATUS_MAPPED;
			demo->next = NULL;
			return res + BLOCK_SIZE;
		}
		else
		{
			void *res = sbrk(MMAP_THRESHOLD);
			demo = (struct block_meta *)res;
			demo->size = size;
			demo->status = STATUS_ALLOC;
			demo->next = NULL;
			return res + BLOCK_SIZE;
		}
	}
	else if (count > 0)
	{
		count++;
		if (size >= MMAP_THRESHOLD)
		{
			struct block_meta *temp = demo;
			while (temp->next != NULL)
			{
				temp = temp->next;
			}
			void *res = mmap(NULL, ALIGN(size + sizeof(struct block_meta)), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			struct block_meta *current = (struct block_meta *)res;
			current->size = size;
			current->status = STATUS_MAPPED;
			current->next = NULL;
			temp->next = current;
			return res + BLOCK_SIZE;
		}
		else
		{
			struct block_meta *temp_alloc = demo;
			struct block_meta *save_prev;
			while (temp_alloc != NULL)
			{
				if (temp_alloc->next == NULL && temp_alloc->status == STATUS_FREE && temp_alloc->size < size)
				{
					void *res = (void *)temp_alloc;
					sbrk(ALIGN(size) - ALIGN(temp_alloc->size));
					temp_alloc->size = size;
					temp_alloc->status = STATUS_ALLOC;
					void *ptr = res + BLOCK_SIZE;
					for (unsigned int i = 0; i < size; i++)
					{
						*((char *)(ptr + i)) = 0;
					}
					return res + BLOCK_SIZE;
				}
				else if (temp_alloc->status == STATUS_FREE && temp_alloc->size == size)
				{
					void *res = (void *)temp_alloc;
					temp_alloc->status = STATUS_ALLOC;
					void *ptr = res + BLOCK_SIZE;
					for (unsigned int i = 0; i < size; i++)
					{
						*((char *)(ptr + i)) = 0;
					}
					return res + BLOCK_SIZE;
				}
				else if (temp_alloc->status == STATUS_FREE && ALIGN(temp_alloc->size) > ALIGN(size + sizeof(struct block_meta) + 8))
				{
					void *res = (void *)temp_alloc;
					res = res + ALIGN(size + sizeof(struct block_meta));
					struct block_meta *split02 = (struct block_meta *)res;
					split02->size = temp_alloc->size - size - sizeof(struct block_meta);
					split02->status = STATUS_FREE;

					if (temp_alloc->next == NULL)
					{
						temp_alloc->next = split02;
						temp_alloc->size = size;
						temp_alloc->status = STATUS_ALLOC;
						split02->next = NULL;
					}
					else
					{
						split02->next = temp_alloc->next;
						temp_alloc->next = split02;
						temp_alloc->size = size;
						temp_alloc->status = STATUS_ALLOC;
					}
					void *ptr = (void *)temp_alloc;
					ptr = ptr + BLOCK_SIZE;
					for (unsigned int i = 0; i < ALIGN(size); i++)
					{
						*((char *)(ptr + i)) = 0;
					}
					return (void *)temp_alloc + BLOCK_SIZE;
				}
				else if (temp_alloc->status == STATUS_FREE && temp_alloc->size > size)
				{
					void *res = (void *)temp_alloc;
					temp_alloc->status = STATUS_ALLOC;
					void *ptr = res + BLOCK_SIZE;
					for (unsigned int i = 0; i < size; i++)
					{
						*((char *)(ptr + i)) = 0;
					}
					return res + BLOCK_SIZE;
				}
				save_prev = temp_alloc;
				temp_alloc = temp_alloc->next;
			}

			struct block_meta *temp = demo;
			while (temp->next != NULL)
			{
				temp = temp->next;
			}

			void *res = sbrk(ALIGN(size + sizeof(struct block_meta)));
			struct block_meta *current = (struct block_meta *)res;
			current->size = size;
			current->status = STATUS_ALLOC;
			current->next = NULL;
			temp->next = current;
			return res + BLOCK_SIZE;
		}
	}
}

void os_free(void *ptr)
{
	/* TODO: Implement os_free */

	if (ptr == NULL)
	{
		return;
	}
	else if ((void *)demo == ptr - BLOCK_SIZE && demo->status == STATUS_MAPPED)
	{
		munmap(ptr - ALIGN(sizeof(struct block_meta)), ALIGN(demo->size + sizeof(struct block_meta)));
		demo = NULL;
		return;
	}
	else
	{
		struct block_meta *temp = demo;
		int found = 0;
		struct block_meta *temp_alloc = demo;
		while (temp_alloc != NULL)
		{
			if ((void *)temp_alloc == ptr - BLOCK_SIZE && temp_alloc->status == STATUS_ALLOC)
			{
				temp_alloc->status = STATUS_FREE;
				return;
			}
			temp_alloc = temp_alloc->next;
		}
		while (temp->next != NULL)
		{
			if ((void *)temp->next == ptr - BLOCK_SIZE && temp->next->status == STATUS_MAPPED)
			{
				found = 1;
				break;
			}
			temp = temp->next;
		}
		if (found == 1)
		{
			if (count == 0 || count == 1)
			{
				munmap(ptr - ALIGN(sizeof(struct block_meta)), ALIGN(demo->size + sizeof(struct block_meta)));
				temp->next = NULL;
				return;
			}
			else if (count > 1)
			{
				if (temp->next->next != NULL)
				{
					int size_temp = ALIGN(temp->next->size + sizeof(struct block_meta));
					temp->next = temp->next->next;
					munmap(ptr - ALIGN(sizeof(struct block_meta)), size_temp);
					return;
				}
				else
				{
					int size_temp = ALIGN(temp->next->size + sizeof(struct block_meta));
					munmap(ptr - ALIGN(sizeof(struct block_meta)), size_temp);
					return;
				}
			}
		}
	}
}

void *os_calloc(size_t nmemb, size_t size)
{
	char buffer01[50];
	int len01 = sprintf(buffer01, "calloc01 - %d\n", nmemb);
	write(1, buffer01, len01);
	char buffer02[50];
	int len02 = sprintf(buffer02, "calloc02 - %d\n", size);
	write(1, buffer02, len02);

	if (nmemb == 0 || size == 0)
	{
		return NULL;
	}
	else
	{
		if (demo == NULL)
		{
			if (nmemb * size + sizeof(struct block_meta) >= PAGE_SIZE)
			{
				void *res = mmap(NULL, ALIGN(size * nmemb + sizeof(struct block_meta)), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

				demo = (struct block_meta *)res;
				demo->size = size * nmemb;
				demo->status = STATUS_MAPPED;
				demo->next = NULL;

				return res + BLOCK_SIZE;
			}
			else if (nmemb + sizeof(struct block_meta) < PAGE_SIZE && size + sizeof(struct block_meta) < MMAP_THRESHOLD)
			{
				void *res = sbrk(MMAP_THRESHOLD);

				demo = (struct block_meta *)res;
				demo->size = size * nmemb;
				demo->status = STATUS_ALLOC;
				demo->next = NULL;

				return res + BLOCK_SIZE;
			}
		}
		else
		{
			if (nmemb * size + sizeof(struct block_meta) >= PAGE_SIZE)
			{
				struct block_meta *temp = demo;
				while (temp->next != NULL)
				{
					temp = temp->next;
				}
				void *res = mmap(NULL, ALIGN(size * nmemb + sizeof(struct block_meta)), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
				struct block_meta *current = (struct block_meta *)res;
				current->size = size * nmemb;
				current->status = STATUS_MAPPED;
				current->next = NULL;
				temp->next = current;
				return res + BLOCK_SIZE;
			}
			else
			{
				return os_malloc(size * nmemb);
			}
		}
	}
}

void *os_realloc(void *ptr, size_t size)
{
	/* TODO: Implement os_realloc */
	if (ptr == NULL)
	{
		return os_malloc(size);
	}
	else if (size == 0)
	{
		os_free(ptr);
		return NULL;
	}
	else if (ptr != NULL && size != 0)
	{
		if (demo == NULL)
		{
			if (size >= MMAP_THRESHOLD)
			{
				void *res = mmap(NULL, ALIGN(size + sizeof(struct block_meta)), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

				demo = (struct block_meta *)res;
				demo->size = size;
				demo->status = STATUS_MAPPED;
				demo->next = NULL;

				return res + BLOCK_SIZE;
			}
			else if (size + sizeof(struct block_meta) < MMAP_THRESHOLD)
			{
				void *res = sbrk(MMAP_THRESHOLD);

				demo = (struct block_meta *)res;
				demo->size = size;
				demo->status = STATUS_ALLOC;
				demo->next = NULL;

				return res + BLOCK_SIZE;
			}
		}
		else
		{
			if (size >= MMAP_THRESHOLD)
			{
				struct block_meta *temp = demo;
				while (temp->next != NULL)
				{
					temp = temp->next;
				}
				void *res = mmap(NULL, ALIGN(size + sizeof(struct block_meta)), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
				struct block_meta *current = (struct block_meta *)res;
				current->size = size;
				current->status = STATUS_MAPPED;
				current->next = NULL;
				temp->next = current;
				return res + BLOCK_SIZE;
			}
			else if (size + sizeof(struct block_meta) < MMAP_THRESHOLD)
			{
				struct block_meta *temp = demo;
				struct block_meta *prev;
				int side_count = 0;
				while (temp != NULL)
				{
					side_count++;
					void *res = (void *)temp;
					if (res + BLOCK_SIZE == ptr)
					{
						if (temp->status == STATUS_MAPPED)
						{
							char buffer01[50];
							int len01 = sprintf(buffer01, "realloc01 - %d\n", size);
							write(1, buffer01, len01);

							struct block_meta *next = temp->next;
							void *heap_start = sbrk(MMAP_THRESHOLD);
							os_free(res + BLOCK_SIZE);

							struct block_meta *current = (struct block_meta *)heap_start;
							current->size = size;
							current->status = STATUS_ALLOC;

							if (side_count == 1)
							{
								if (count == 1)
								{
									demo = current;
								}
								else
								{
									current->next = next;
									demo = current;
								}
							}
							else
							{
								prev->next = current;
								current->next = next;
							}
							return heap_start + BLOCK_SIZE;
						}
						else if (temp->status == STATUS_ALLOC)
						{
							temp->size = size;
							return res + BLOCK_SIZE;
						}
						else if (temp->status == STATUS_FREE)
						{
							return NULL;
						}
					}
					prev = temp;
					temp = temp->next;
				}
			}
		}
	}
}
