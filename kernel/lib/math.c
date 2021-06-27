#include <math.h>

static uint32_t rseed = 1;

/*
 * Generate a random number.
 */
static uint32_t rand_r(uint32_t *seed)
{
  uint32_t next = *seed;
  int result;

  next *= 1103515245;
  next += 12345;
  result = (uint32_t) (next / 65536) % 2048;

  next *= 1103515245;
  next += 12345;
  result <<= 10;
  result ^= (uint32_t) (next / 65536) % 2048;

  next *= 1103515245;
  next += 12345;
  result <<= 10;
  result ^= (uint32_t) (next / 65536) % 2048;

  *seed = next;

  return result;
}

/*
 * Generate a random number.
 */
uint32_t rand()
{
  return rand_r(&rseed);
}

/*
 * Generate a random number.
 */
uint32_t srand(uint32_t seed)
{
  rseed = seed;
  return rand_r(&rseed);
}
