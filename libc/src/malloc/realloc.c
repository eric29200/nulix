#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "__malloc_impl.h"

/*
 * Reallocate memory.
 */
void *realloc(void *ptr, size_t size)
{
	struct heap_block_t *old_block;
	void *new_ptr;

	if (!ptr && size == 0)
		return NULL;

	/* allocate new memory */
	new_ptr = malloc(size);
	if (!new_ptr || !ptr)
		return new_ptr;

	/* get old block */
	old_block = ((struct heap_block_t *) ptr) - 1;

	/* copy old memory to new memory */
	if (old_block->size < size)
		memcpy(new_ptr, ptr, old_block->size);
	else
		memcpy(new_ptr, ptr, size);

	/* free old memory */
	free(ptr);
	return new_ptr;
}