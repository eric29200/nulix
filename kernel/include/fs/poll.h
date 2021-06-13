#ifndef _POLL_H_
#define _POLL_H_

#include <stddef.h>

#define POLLIN          0x0001
#define POLLPRI         0x0002
#define POLLOUT         0x0004
#define POLLERR         0x0008
#define POLLHUP         0x0010
#define POLLNVAL        0x0020

/*
 * Poll file descriptore.
 */
struct pollfd_t {
  int       fd;
  uint16_t  events;
  uint16_t  revents;
};

#endif
