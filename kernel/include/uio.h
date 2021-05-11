#ifndef _UIO_H_
#define _UIO_H_

#include <stddef.h>

/*
 * I/O vectors structure.
 */
struct iovec_t {
  void *  iov_base;
  size_t  iov_len;
};

#endif
