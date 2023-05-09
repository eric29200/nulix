#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Swap 2 items.
 */
static inline void swap(void *e1, void *e2, void *tmp, size_t size)
{
	memcpy(tmp, e1, size);
	memcpy(e1, e2, size);
	memcpy(e2, tmp, size);
}

/*
 * Recursive quick sort.
 */
static void __quick_sort(void *base, size_t nmemb, size_t size, void *tmp, int (*compare)(const void *, const void *))
{
	void *pivot;
	int i, j;

	if (nmemb < 2)
		return;

	/* choose pivot */
	pivot = base + (nmemb / 2) * size;

	/* sort */
	for (i = 0, j = nmemb - 1;; i++, j--) {
		/* find misplaced item in left partition */
		while (compare(base + i * size, pivot) < 0)
			i++;

		/* find misplaced item in right partition */
		while (compare(base + j * size, pivot) > 0)
			j--;

		if (i >= j)
			break;

		/* swap items */
		swap(base + i * size, base + j * size, tmp, size);
	}

	/* quicksort remaining */
	__quick_sort(base, i, size, tmp, compare);
	__quick_sort(base + i * size, nmemb - i, size, tmp, compare);
}

/*
 * Quick sort.
 */
void qsort(void *base, size_t nmemb, size_t size, int (*compare)(const void *, const void *))
{
	void *tmp;

	/* check input array */
	if (!base || nmemb == 0 || size < 2)
		return;

	/* allocate tmp */
	tmp = malloc(size);
	if (!tmp)
		return;

	/* quick sort */
	__quick_sort(base, nmemb, size, tmp, compare);

	/* free tmp */
	free(tmp);
}
