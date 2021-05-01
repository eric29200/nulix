#include <stdlib.h>
#include <unistd.h>

#define ALIGN                 (sizeof(void *))
#define ALIGN_UP(x)           (((x) + ((ALIGN) - 1)) & ~((ALIGN) - 1))
#define ALIGN_DOWN(x)         ((x) & ~((ALIGN) - 1))

#define TO_DATA(x)            ((void *) ALIGN_UP((uint32_t) x + sizeof(struct malloc_header_t)))
#define TO_HEAD(x)            ((struct malloc_header_t *) ALIGN_DOWN((uint32_t) x - sizeof(struct malloc_header_t)))

#define MIN_ALLOC             (1024 * (ALIGN))

/*
 * Malloc header.
 */
struct malloc_header_t {
  struct malloc_header_t *next;
  size_t size;
};

/* malloc base header */
static struct malloc_header_t base;
static struct malloc_header_t *freep = NULL;

/*
 * Grow process data segment.
 */
static struct malloc_header_t *morecore(size_t size)
{
  struct malloc_header_t *p;

  if (size < MIN_ALLOC)
    size = MIN_ALLOC;

  p = (struct malloc_header_t *) sbrk(size);
  if (p == (struct malloc_header_t *) - 1)
    return NULL;

  p->size = size;
  free(TO_DATA(p));
  return freep;
}

/*
 * Allocate memory on the heap.
 */
void *malloc(size_t size)
{
  struct malloc_header_t *curr, *prev;

  /* adjust size */
  size = ALIGN_UP(size + sizeof(struct malloc_header_t));

  /* first malloc */
  prev = freep;
  if (!prev) {
    base.next = freep = prev = &base;
    base.size = 0;
  }

  /* try to find an empty block */
  for (curr = freep->next; ; prev = curr, curr = curr->next) {
    if (curr->size >= size) {
      if (curr->size == size) {
        prev->next = curr->next;
      } else {
        curr->size -= size;
        curr = (struct malloc_header_t *) ((char *) curr + curr->size);
        curr->size = size;
      }

      freep = prev;
      return TO_DATA(curr);
    }

    /* try to grow heap */
    if (curr == freep) {
      curr = morecore(size);
      if (!curr)
        return NULL;
    }
  }
}

/*
 * Free memory on the heap.
 */
void free(void *ptr)
{
  struct malloc_header_t *curr, *prev;

  curr = TO_HEAD(ptr);
  for (prev = freep; prev >= curr || curr >= prev->next; prev = prev->next)
    if (prev >= prev->next && (curr > prev || curr < prev->next))
      break;

  if ((char *) curr + curr->size == (char *) prev->next) {
    curr->size += prev->next->size;
    curr->next = prev->next->next;
  } else {
    curr->next = prev->next;
  }

  if ((char *) prev + prev->size == (char *) curr) {
    prev->size += curr->size;
    prev->next = curr->next;
  } else {
    prev->next = curr;
  }

  freep = prev;
}
