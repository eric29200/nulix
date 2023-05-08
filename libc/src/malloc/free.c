#include "__malloc_impl.h"

/*
 * Free memory.
 */
void free(void *ptr)
{
	struct heap_block_t *block;

	if (!ptr)
		return;

	block = ((struct heap_block_t *) ptr) - 1;
	block->free = 1;
}