#include <fs/fs.h>
#include <fs/cramfs_fs.h>
#include <lib/zlib.h>
#include <stdio.h>

/* zlib stream */
static struct zlib_stream stream;

/*
 * Uncompress a block.
 */
int cramfs_uncompress_block(void *src, int src_len, void *dst, int dst_len)
{
	int ret;

	/* init zlib stream */
	stream.in = src;
	stream.in_len = src_len;
	stream.out = dst;
	stream.out_len = dst_len;
	ret = zlib_inflate_reset(&stream);
	if (ret != Z_OK)
		goto err;

	/* inflate */
	ret = zlib_inflate(&stream);
	if (ret != Z_OK)
		goto err;

	return stream.out_written;
err:
	printf("[Cramfs] Error %d while decompressing\n", ret);
	return 0;
}
