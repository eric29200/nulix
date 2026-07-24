#include <lib/inflate.h>

#define MAX_BITS	15						/* maximum bits in a code */
#define FIX_LCODES	288						/* number of fixed literal/length codes */
#define MAX_LCODES	286						/* maximum number of literal/length codes */
#define MAX_DCODES	30						/* maximum number of distance codes */
#define MAX_CODES	(MAX_LCODES + MAX_DCODES)			/* maximum codes lengths to read */

/*
 * Inflate context.
 */
struct inflate_context {
	uint8_t *	in;						/* input buffer */
	size_t		in_len;						/* input buffer length */
	size_t		in_pos;						/* position in input buffer */
	uint8_t		in_bit_pos;					/* position in current byte in input buffer */
	uint8_t *	out;						/* output buffer */
	size_t		out_len;					/* output buffer length */
	size_t		out_pos;					/* position in output buffer */
};

/*
 * Huffman table
 */
struct huffman {
	uint16_t	counts[MAX_BITS + 1];				/* number of symbols of each length */
	uint16_t	symbols[FIX_LCODES];				/* ordered symbols */
};

/*
 * Read bits from input buffer.
 */
static int read_bits(struct inflate_context *ctx, int nr_bits)
{
	int i, value = 0;

	for (i = 0; i < nr_bits; i++) {
		/* load next byte if needed */
		if (ctx->in_bit_pos == 8) {
			ctx->in_pos++;
			ctx->in_bit_pos = 0;

			if (ctx->in_pos >= ctx->in_len)
				return value;
		}

		/* read next bit */
		value |= ((ctx->in[ctx->in_pos] >> ctx->in_bit_pos++) & 0x01) << i;
	}

	return value;
}

/*
 * Build a huffman table from an array of lengths.
 */
static void build_huffman(struct huffman *table, uint16_t *lengths, size_t size)
{
	uint16_t offsets[MAX_BITS + 1];
	int count = 0;
	size_t i;

	/* count number of codes of each length */
	for (i = 0; i <= MAX_BITS; i++)
		table->counts[i] = 0;
	for (i = 0; i < size; i++)
		table->counts[lengths[i]]++;
	table->counts[0] = 0;

	/* generate offsets  */
	for (i = 0; i <= MAX_BITS; i++) {
		offsets[i] = count;
		count += table->counts[i];
	}

	/* put symbols in table sorted by length */
	for (i = 0; i < size; i++)
		if (lengths[i])
			table->symbols[offsets[lengths[i]]++] = i;
}

/*
 * Build fixed huffman tables.
 */
static void build_fixed(struct huffman *lengths_table, struct huffman *distances_table)
{
	uint16_t lengths[FIX_LCODES];
	int i;

	/* literal/length table */
	for (i = 0; i < 144; i++)
		lengths[i] = 8;
	for (i = 144; i < 256; i++)
		lengths[i] = 9;
	for (i = 256; i < 280; i++)
		lengths[i] = 7;
	for (i = 280; i < FIX_LCODES; i++)
		lengths[i] = 8;
	build_huffman(lengths_table, lengths, FIX_LCODES);

	/* distance table */
	for (i = 0; i < MAX_DCODES; i++)
		lengths[i] = 5;
	build_huffman(distances_table, lengths, MAX_DCODES);
}

/*
 * Decode a symbol.
 */
static int decode_symbol(struct inflate_context *ctx, const struct huffman *table)
{
	int count = 0, cur = 0, i;

	for (i = 1; cur >= 0; i++) {
		cur = (cur << 1) | read_bits(ctx, 1);
		count += table->counts[i];
		cur -= table->counts[i];
	}

	return table->symbols[count + cur];
}

/*
 * Decode dynamic huffman tables.
 */
static void decode_huffman(struct inflate_context *ctx, struct huffman *lengths_table, struct huffman *distances_table)
{
	int nr_literals, nr_distances, nr_lengths, symbol, length, i;
	static const uint16_t len_order[] = {
		16, 17, 18, 0, 8, 7, 9, 6, 10,
		5, 11, 4, 12, 3, 13, 2, 14, 1, 15
	};
	uint16_t lengths[MAX_CODES] = { 0 };
	struct huffman len_codes_table;

	/* read number of literals, distances and lengths */
	nr_literals = read_bits(ctx, 5) + 257;
	nr_distances = read_bits(ctx, 5) + 1;
	nr_lengths = read_bits(ctx, 4) + 4;

	/* read length codes lengths */
	for (i = 0; i < nr_lengths; ++i)
		lengths[len_order[i]] = read_bits(ctx, 3);

	/* build huffman table for code lengths codes (use lencode temporarily) */
	build_huffman(&len_codes_table, lengths, 19);

	/* decode literals/distances tables */
	for (i = 0; i < nr_literals + nr_distances;) {
		/* read next symbol */
		symbol = decode_symbol(ctx, &len_codes_table);

		/* length in 0..15 */
		if (symbol < 16) {
			lengths[i++] = symbol;
			continue;
		}

		/* repeat instructions */
		length = 0;
		switch (symbol) {
			case 16:					/* repeat previous length (from 3 to 6) */
				length = lengths[i - 1];
				symbol = read_bits(ctx, 2) + 3;
				break;
			case 17:					/* repeat 0 length (from 3 to 10) */
				symbol = read_bits(ctx, 3) + 3;
				break;
			default:					/* repeat 0 length (from 11 to 138) */
				symbol = read_bits(ctx, 7) + 11;
				break;
		}

		/* repeat last or zero symbol times */
		while (symbol--)
			lengths[i++] = length;
	}

	/* build huffman tables */
	build_huffman(lengths_table, lengths, nr_literals);
	build_huffman(distances_table, lengths + nr_literals, nr_distances);
}


/*
 * Decode a block.
 */
static int decode_block(struct inflate_context *ctx, struct huffman *lengths_table, struct huffman *distances_table)
{
	/* size base for length codes 257..285 */
	static const uint16_t lens[] = {
		3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51,
		59, 67, 83, 99, 115, 131, 163, 195, 227, 258
	};
	/* extra bits for length codes 257..285 */
	static const uint16_t lext[] = {
		0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4,
		4, 5, 5, 5, 5, 0
	};
	/* offsets base for distance codes 0..29 */
	static const uint16_t dists[] = {
		1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385,
		513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
	};
	/* extra bits for distance codes 0..29 */
	static const uint16_t dext[] = {
		0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10,
		10, 11, 11, 12, 12, 13, 13
	};
	int symbol, length, distance, offset, i;

	for (;;) {
		/* read next symbol */
		symbol = decode_symbol(ctx, lengths_table);

		/* end of block */
		if (symbol == 256)
			break;

		/* literal : just add it to output buffer */
		if (symbol < 256) {
			if (ctx->out_pos == ctx->out_len)
				return INFLATE_ERROR;

			ctx->out[ctx->out_pos++] = symbol;
			continue;
		}

		/* get and compute length */
		symbol -= 257;
		length = read_bits(ctx, lext[symbol]) + lens[symbol];

		/* get distance and offset */
		distance = decode_symbol(ctx, distances_table);
		offset = read_bits(ctx, dext[distance]) + dists[distance];

		/* check output buffer length */
		if (ctx->out_pos + length > ctx->out_len)
			return INFLATE_ERROR;

		/* duplicate pattern */
		for (i = 0; i < length; ++i) {
			ctx->out[ctx->out_pos] = ctx->out[ctx->out_pos - offset];
			ctx->out_pos++;
		}
	}

	return INFLATE_OK;
}

/*
 * Decode an uncompressed block.
 */
static int no_compression(struct inflate_context * ctx)
{
	size_t len;

	/* discard leftover bits from current byte */
	ctx->in_bit_pos = 0;
	ctx->in_pos++;

	/* block must contain at least 4 bytes : 2 bytes for length and 2 bytes length one's complement */
	if (ctx->in_pos + 4 > ctx->in_len)
		return INFLATE_ERROR;

	/* read block length */
	len = ctx->in[ctx->in_pos++];
	len |= ctx->in[ctx->in_pos++] << 8;

	/* read block length one's complement */
	if (ctx->in[ctx->in_pos++] != (~len & 0xFF) || ctx->in[ctx->in_pos++] != ((~len >> 8) & 0xFF))
        	return INFLATE_ERROR;

	/* check if there is enough space in input and output buffers */
	if (ctx->in_pos + len > ctx->in_len)
		return INFLATE_ERROR;
	if (ctx->out_pos + len > ctx->out_len)
		return INFLATE_ERROR;

	/* copy bytes from input to output */
	while (len--)
		ctx->out[ctx->out_pos++] = ctx->in[ctx->in_pos++];

	return INFLATE_OK;
}

/*
 * Decode a fix compressed block.
 */
static int fixed(struct inflate_context *ctx)
{
	static struct huffman fixed_lengths, fixed_distances;
	static int built = 0;

	/* build fixed huffman tables */
	if (!built) {
		build_fixed(&fixed_lengths, &fixed_distances);
		built = 1;
	}

	/* decode block */
	return decode_block(ctx, &fixed_lengths, &fixed_distances);
}


/*
 * Decode a dynamic compressed block.
 */
static int dynamic(struct inflate_context *ctx)
{
	static struct huffman dynamic_lengths, dynamic_distances;

	/* build dynamic tables */
	decode_huffman(ctx, &dynamic_lengths, &dynamic_distances);

	/* decode block */
	return decode_block(ctx, &dynamic_lengths, &dynamic_distances);
}

/*
 * Uncompress a block with inflate algorithm.
 */
int inflate(uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_len)
{
	struct inflate_context ctx = { 0 };
	int last, type, ret = INFLATE_OK;

	/* check src length */
	if (!src_len)
		return ret;

	/* init context */
	ctx.in = src;
	ctx.in_len = src_len;
	ctx.out = dst;
	ctx.out_len = dst_len;

	for (;;) {
		/* read block header */
		last = read_bits(&ctx, 1);
		type = read_bits(&ctx, 2);

		/* uncompress block */
		switch (type) {
			case 0:
				ret = no_compression(&ctx);
				break;
			case 1:
			 	ret = fixed(&ctx);
				break;
			case 2:
			 	ret = dynamic(&ctx);
				break;
			default:
				ret = INFLATE_ERROR;
				break;
		}

		/* handle error */
		if (ret != INFLATE_OK)
			break;

		/* last block */
		if (last)
			break;
	}

	return ret;
}