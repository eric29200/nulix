#include <proc/sched.h>
#include <math.h>

#define MAX(a, b)		((a) >= (b) ? (a) : (b))

static int rand_initialized = 0;
static uint32_t z[4];

/*
 * Init random.
 */
static int random_init(const unsigned char *seed, size_t seed_siz)
{
	unsigned char *dst = (unsigned char *) z;
	size_t i, j, k;

	if (seed_siz == 0)
		return -1;

	for (i = 0, j = 0, k = 0; k < MAX(sizeof(z), seed_siz); k++) {
		dst[i] ^= seed[j];
		i++;

		if (i >= sizeof(z))
	    		i = 0;

		j++;
		if (j >= seed_siz)
	    		j = 0;
	}

	rand_initialized = 1;
	return 0;
}

/*
 * Get a random number.
 */
uint32_t rand()
{
	uint32_t r;

	/* init random */
	if (rand_initialized == 0)
		if (random_init((const unsigned char *) &jiffies, sizeof(jiffies)) < 0)
			return -1;

	/* generate random number */
	r = ((z[0] << 6) ^ z[0]) >> 13;
	z[0] = ((z[0] & 4294967294UL) << 18) ^ r;
	r = ((z[1] << 2) ^ z[1]) >> 27;
	z[1] = ((z[1] & 4294967288UL) << 2) ^ r;
	r = ((z[2] << 13) ^ z[2]) >> 21;
	z[2] = ((z[2] & 4294967280UL) << 7) ^ r;
	r = ((z[3] << 3) ^ z[3]) >> 12;
	z[3] = ((z[3] & 4294967168UL) << 13) ^ r;
	r = z[0] ^ z[1] ^ z[2] ^ z[3];

	return r;
}
