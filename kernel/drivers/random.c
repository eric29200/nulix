#include <drivers/random.h>

#define MAX(a, b)		((a) >= (b) ? (a) : (b))
#define ALIGN_UP(val, a)	(((val) + ((a) - 1)) & ~((a) - 1))

static int rand_initialized = 0;
static uint32_t z[4];

/*
 * Get a random number.
 */
static uint32_t random_get()
{
	uint32_t r;

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
 * Process unaligned data.
 */
static void random_unaligned(uint8_t *buf, size_t n)
{
    uint32_t r;
    size_t i;

    r = random_get();
    for (i = 0; i < n; i++) {
        *buf++ = (uint8_t) r;
        r >>= 8;
    }
}

/*
 * Open random device.
 */
static int random_open(struct file_t *filp)
{
	UNUSED(filp);
	return 0;
}

/*
 * Close random device.
 */
static int random_close(struct file_t *filp)
{
	UNUSED(filp);
	return 0;
}

/*
 * Read random device.
 */
static int random_read(struct file_t *filp, char *buf, int n)
{
	uint32_t *buf32;
	size_t iter, i;

	/* unused filp */
	UNUSED(filp);

	/* init random */
	if (rand_initialized == 0) {
		if (random_init((const unsigned char *) &jiffies, sizeof(jiffies)) < 0)
			return -1;
	}

	/* eventually process front unaligned data */
	if (((iter = ((uint32_t) buf & 0x03))) != 0)
		random_unaligned((uint8_t *) buf, sizeof(uint32_t) - iter);

	/* at this point, buf is be aligned to a uint32_t boundary (safe cast) */
	iter = (n >> 2);
	buf32 = (uint32_t *) ALIGN_UP((uint32_t) buf, sizeof(uint32_t));
	for (i = 0; i < iter; i++)
		buf32[i] = random_get();

	/* eventually process back unaligned data */
	if ((iter = (n & 0x03)) != 0)
		random_unaligned((unsigned char *)&buf32[i], iter);

    	return n;
}

/*
 * Write to random device.
 */
static int random_write(struct file_t *filp, const char *buf, int n)
{
	UNUSED(filp);
	UNUSED(buf);
	UNUSED(n);
	return n;
}

/*
 * Random device file operations.
 */
static struct file_operations_t random_fops = {
	.open		= random_open,
	.close		= random_close,
	.read		= random_read,
	.write		= random_write,
};

/*
 * Random device inode operations.
 */
struct inode_operations_t random_iops = {
	.fops		= &random_fops,
};
