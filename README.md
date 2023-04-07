# Memory Allocator

## Objectives

- Learn the basics of memory management by implementing minimal versions of `malloc()`, `calloc()`, `realloc()`, and `free()`
- Accommodate with the memory management syscalls in Linux: `brk()`, `mmap()`, and `munmap()`
- Understand the bottlenecks of memory allocation and how to reduce them

## Statement

Build a minimalistic memory allocator that can be used to manually manage virtual memory.
The goal is to have a reliable library that accounts for explicit allocation, reallocation, and initialization of memory.

## Support Code

The support code consists of three directories:

- `allocator/` will contain your solution based on the `osmem.h` header file
- `tests/` contains the test suite and a Python script to verify your work
- `utils/` contains an implementation for `printf()` function that does **not** use the heap

The test suite consists of `.c` files that will be dynamically linked to your library, `libosmem.so`.
You can find the sources in the `tests/src/` directory.
The results of the previous run can be found in the `tests/out/` directory and the reference files are in the `tests/ref/` directory.

The automated checking is performed using `checker.py` that runs each test and compares the syscalls made by the `os_*` functions with the reference file, providing a diff if the test failed.

## API

1. `void *os_malloc(size_t size)`

   Allocates `size` bytes and returns a pointer to the allocated memory.

   Chunks of memory smaller than `MMAP_THRESHOLD` are allocated with `brk()`.
   Bigger chunks are allocated using `mmap()`.
   The memory is uninitialized.

   - Passing `0` as `size` will return `NULL`

1. `void *os_calloc(size_t nmemb, size_t size)`

   Allocates memory for an array of `nmemb` elements of `size` bytes each and returns a pointer to the allocated memory.

   Chunks of memory smaller than [`page_size`](https://man7.org/linux/man-pages/man2/getpagesize.2.html) are allocated with `brk()`.
   Bigger chunks are allocated using `mmap()`.
   The memory is set to zero.

   - Passing `0` as `nmemb` or `size` will return `NULL`

1. `void *os_realloc(void *ptr, size_t size)`

   Changes the size of the memory block pointed to by `ptr` to `size` bytes.
   If the size is smaller than the previously allocated size, the memory block will be truncated.

   If `ptr` points to a block on heap, `os_realloc()` will first try to expand the block, rather than moving it.
   Otherwise, the block will be reallocated and its contents copied.

   When attempting to expand a block followed by multiple free blocks, `os_realloc()` will coalesce them one at a time and verify the condition for each.
   Blocks will remain coalesced even if the resulting block will not be big enough for the new size.

   - Passing `NULL` as `ptr` will have the same effect as `os_malloc(size)`
   - Passing `0` as `size` will have the same effect as `os_free(ptr)`

1. `void os_free(void *ptr)`

   Frees memory previously allocated by `os_malloc()`, `os_calloc()` or `os_realloc()`.

   `os_free()` will not return memory from the heap to the OS by calling `brk()`, but rather mark it as free and reuse it in future allocations.
   In the case of mapped memory blocks, `os_free()` will call `munmap()`.

1. General

   - Allocations that increase the heap size will only expand the last block if it is free.
   - You must check the error code returned by every syscall. You can use the `DIE()` macro for this.

## Implementation

An efficient implementation must keep data aligned, keep track of memory blocks and reuse freed blocks.
This can be further improved by reducing the number of syscalls and block operations.

### [Memory Alignment](https://stackoverflow.com/a/381368)

Allocated memory should be aligned (i.e. all addresses are multiple of a given size).
This is a space-time tradeoff because memory blocks are padded so each can be read in one transaction.
It also allows for atomicity when interacting with a block of memory.

All memory allocations should be aligned to **8 bytes** as required by 64 bit systems.

### Block Reuse

#### `struct block_meta`

We will consider a **block** to be a continuous zone of memory, allocated and managed by our implementation.
The structure `block_meta` will be used to manage the metadata of a block.
Each allocated zone will comprise of a `block_meta` structure placed at the start, followed by data (payload).
For all functions, the returned address will be that of the **payload** (not the `block_meta` structure).

```C
typedef struct block_meta {
   size_t size;
   int status;
   struct block_meta *next;
} block_meta;
```

_Note_: Both the `struct block_meta` and the payload of a block should be aligned to **8 bytes**.

_Note_: Most compilers will automatically pad the structure, but you should still align it for portability.

![memory-block](./assets/memory-block.svg)

#### Split Block

Reusing memory blocks improves the allocator's performance, but might lead to [Internal Memory Fragmentation](https://www.tutorialspoint.com/difference-between-internal-fragmentation-and-external-fragmentation#:~:text=What%20is%20Internal%20Fragmentation%3F).
This happens when we allocate a size smaller than all available free blocks.
If we use one larger block the remaining size of that block will be wasted since it cannot be used for another allocation.

To avoid this, a block should be truncated to the required size and the remaining bytes should be used to create a new free block.

![Split Block](./assets/split-block.svg)

The resulting free block should be reusable.
The split will not be performed if the remaining size (after reserving space for `block_meta` structure and payload) is not big enough to fit another block (`block_meta` structure and at least **1 byte** of usable memory).

_Note_: Do not forget the alignment!

#### Coalesce Blocks

There are cases when there is enough free memory for an allocation, but it is spread across multiple blocks that cannot be used.
This is called [External Memory Fragmentation](https://www.tutorialspoint.com/difference-between-internal-fragmentation-and-external-fragmentation#:~:text=What%20is%20External%20Fragmentation%3F).

One technique to reduce external memory fragmentation is **block coalescing** which implies merging adjacent free blocks to form a contiguous chunk.

![Coalesce Block Image](./assets/coalesce-blocks.svg)

Coalescing will be used before searching for a block and in `os_realloc()` to expand the current block when possible.

_Note_: You might still need to split the block after coalesce.

#### Find Best Block

Our aim is to reuse a free block with a size closer to what we need in order to reduce the number of future operations on it.
This strategy is called **find best**.
On every allocation we need to search the whole list of blocks and choose the best fitting free block.

In practice, it also uses a list of free blocks to avoid parsing all blocks, but this is out of the scope of the assignment.

_Note_: For consistent results, coalesce all adjacent free blocks before searching.

### Heap Preallocation

Heap is used in most modern programs.
This hints at the possibility of preallocating a relatively big chunk of memory (i.e. **128 kilobytes**) when the heap is used for the first time.
This reduces the number of future `brk()` syscalls.

For example, if we try to allocate 1000 bytes we should first allocate a block of 128 kilobytes and then split it.
On future small allocations, we should proceed to split the preallocated chunk.

## Building Memory Allocator

To build `libosmem.so`, run `make` in the `allocator/` directory:

```console
student@os:~/.../content/assignments/mem-alloc$ cd allocator/

student@os:~/.../assignments/mem-alloc/allocator$ make
```

## Testing and Grading

The testing is automated and performed with the `checker.py` script from the `tests/` directory.

Before running `checker.py` ensure that `libsomem.so` is built in the `allocator/` directory and test binaries are generated in the `tests/bin/` directory.

```console
student@os:~/.../content/assignments/mem-alloc$ cd tests/

student@os:~/.../assignments/mem-alloc/tests$ make
gcc -fPIC -Wall -Wextra -g -I../utils -L../allocator  test-calloc-0.c  -losmem -o test-calloc-0
gcc -fPIC -Wall -Wextra -g -I../utils -L../allocator  test-calloc-1.c  -losmem -o test-calloc-1
gcc -fPIC -Wall -Wextra -g -I../utils -L../allocator  test-calloc-2.c  -losmem -o test-calloc-2
gcc -fPIC -Wall -Wextra -g -I../utils -L../allocator  test-calloc-3.c  -losmem -o test-calloc-3
gcc -fPIC -Wall -Wextra -g -I../utils -L../allocator  test-calloc-4.c  -losmem -o test-calloc-4
gcc -fPIC -Wall -Wextra -g -I../utils -L../allocator  test-malloc-0.c  -losmem -o test-malloc-0
[...]

student@os:~/.../assignments/mem-alloc/tests$ python checker.py
test-malloc-0                    ........................ passed ...   1
test-malloc-1                    ........................ passed ...   2
test-malloc-2                    ........................ passed ...   2
test-malloc-3                    ........................ passed ...   2
test-malloc-4                    ........................ passed ...   2
test-malloc-5                    ........................ passed ...   1
test-malloc-6                    ........................ passed ...   1
test-malloc-7                    ........................ passed ...   1
test-malloc-8                    ........................ passed ...   2
test-malloc-9                    ........................ passed ...   2
test-malloc-10                   ........................ passed ...   2
test-malloc-11                   ........................ passed ...   2
test-malloc-12                   ........................ passed ...   2
test-malloc-13                   ........................ passed ...   2
test-malloc-14                   ........................ passed ...   2
test-malloc-15                   ........................ passed ...   2
test-calloc-0                    ........................ passed ...   1
test-calloc-1                    ........................ passed ...   1
test-calloc-2                    ........................ passed ...   1
test-calloc-3                    ........................ passed ...   2
test-calloc-4                    ........................ passed ...   1
test-calloc-5                    ........................ passed ...   1
test-calloc-6                    ........................ passed ...   1
test-calloc-7                    ........................ passed ...   2
test-calloc-8                    ........................ passed ...   2
test-calloc-9                    ........................ passed ...   2
test-calloc-10                   ........................ passed ...   2
test-malloc-all                  ........................ passed ...   5

Grade                            ..................................... 49
```

### Debugging

`checker.py` uses `ltrace` to capture all the libcalls and syscalls performed.

The output of `ltrace` is formatted to show only top level library calls and nested system calls.
For consistency, the heap start and addresses returned by `mmap()` are replaced with labels.
Every other address is displayed as `<label> + offset`, where the label is the closest mapped address.

To run a single test use `checker.py <test>`, where `<test>` is the name of the test without path or extension.
Append `-v` to see the diff between `out/<test>.out` and `ref/<test>.ref`.

```console
student@os:~/.../assignments/mem-alloc/tests$ python checker.py test-calloc-1 -v
test-calloc-1                    ........................ failed ...   0
--- out/test-calloc-1.out
+++ ref/test-calloc-1.ref
@@ -1,5 +1,8 @@
-os_malloc (['131040'])                                                                    = 0
-os_calloc (['100', '2'])                                                                  = 0
-os_free (['0'])                                                                           = <void>
-os_free (['0'])                                                                           = <void>
+os_malloc (['131040'])                                                                    = HeapStart + 0x18
+  brk (['0'])                                                                             = HeapStart + 0x0
+  brk (['HeapStart + 0x20000'])                                                           = HeapStart + 0x20000
+os_calloc (['100', '2'])                                                                  = HeapStart + 0x20018
+  brk (['HeapStart + 0x200e0'])                                                           = HeapStart + 0x200e0
+os_free (['HeapStart + 0x20018'])                                                         = <void>
+os_free (['HeapStart + 0x18'])                                                            = <void>
 +++ exited (status 0) +++


Grade                            ..................................... 0
```

## Resources

- ["Implementing malloc" slides by Michael Saelee](https://moss.cs.iit.edu/cs351/slides/slides-malloc.pdf)
- [Malloc Tutorial](https://danluu.com/malloc-tutorial/)
