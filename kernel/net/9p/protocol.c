#include <net/9p/9p.h>
#include <stdio.h>
#include <stderr.h>
#include <string.h>
#include <mm/mm.h>
#include <x86/endian.h>

#define min(x, y)		((x) <= (y) ? (x) : (y))
#define P9_STRLEN(s)		(2 + min(s ? strlen(s) : 0, USHRT_MAX))

/*
 * Get needed size packet.
 */
int p9_msg_buf_size(int8_t type, const char *fmt, va_list ap)
{
	const int hdr = 4 + 1 + 2;

	switch (type) {
		case P9_TVERSION:
		case P9_RVERSION:
		case P9_RATTACH:
		case P9_TCLUNK:
		case P9_RCLUNK:
			return 4096;
		case P9_TATTACH:
			if (strcmp("ddssu", fmt))
				p9_fatal("p9_msg_buf_size: wrong format for message %d\n", type);
			va_arg(ap, int32_t);
			va_arg(ap, int32_t);
			{
				const char *uname = va_arg(ap, const char *);
				const char *aname = va_arg(ap, const char *);
				/* fid[4] afid[4] uname[s] aname[s] n_uname[4] */
				return hdr + 4 + 4 + P9_STRLEN(uname) + P9_STRLEN(aname) + 4;
			}
		default:
			p9_fatal("p9_msg_buf_size: unknown message %d\n", type);
			return -EINVAL;
	}
}

/*
 * Write to packet.
 */
static size_t pdu_write(struct p9_fcall *pdu, const void *data, size_t size)
{
	size_t len = min(pdu->capacity - pdu->size, size);
	memcpy(&pdu->sdata[pdu->size], data, len);
	pdu->size += len;
	return size - len;
}

/*
 * Write from a packet.
 */
size_t pdu_read(struct p9_fcall *pdu, void *data, size_t size)
{
	size_t len = min(pdu->size - pdu->offset, size);
	memcpy(data, &pdu->sdata[pdu->offset], len);
	pdu->offset += len;
	return size - len;
}

/*
 * Write to packet.
 */
int p9pdu_vwritef(struct p9_fcall *pdu, const char *fmt, va_list ap)
{
	const char *ptr;
	int ret = 0;

	for (ptr = fmt; *ptr; ptr++) {
		switch (*ptr) {
			case 'b':
				int8_t val8 = va_arg(ap, int);
				if (pdu_write(pdu, &val8, sizeof(val8)))
					ret = -EFAULT;
				break;
			case 'w':
				int16_t val16 = htole16(va_arg(ap, int));
				if (pdu_write(pdu, &val16, sizeof(val16)))
					ret = -EFAULT;
				break;
			case 'd':
				int32_t val32 = htole32(va_arg(ap, int32_t));
				if (pdu_write(pdu, &val32, sizeof(val32)))
					ret = -EFAULT;
				break;
			case 'u':
				uid_t uid = htole32(va_arg(ap, uid_t));
				if (pdu_write(pdu, &uid, sizeof(uid)))
					ret = -EFAULT;
				break;
			case 's':
				const char *sptr = va_arg(ap, const char *);
				uint16_t len = 0;
				if (sptr)
					len = min(strlen(sptr), USHRT_MAX);

				ret = p9pdu_writef(pdu, "w", len);
				if (ret == 0 && pdu_write(pdu, sptr, len))
					ret = -EFAULT;
				break;
			default:
				p9_fatal("p9pdu_vwritef: unknwon format %c\n", *ptr);
				break;
		}

		if (ret)
			break;
	}

	return ret;
}

/*
 * Read from a packet.
 */
int p9pdu_vreadf(struct p9_fcall *pdu, const char *fmt, va_list ap)
{
	const char *ptr;
	int ret = 0;

	for (ptr = fmt; *ptr; ptr++) {
		switch (*ptr) {
			case 'b':
				int8_t *val8 = va_arg(ap, int8_t *);
				if (pdu_read(pdu, val8, sizeof(*val8))) {
					ret = -EFAULT;
					break;
				}
				break;
			case 'w':
				int16_t *val16 = va_arg(ap, int16_t *);
				if (pdu_read(pdu, val16, sizeof(*val16))) {
					ret = -EFAULT;
					break;
				}
				*val16 = le16toh(*val16);
				break;
			case 'd':
				int32_t *val32 = va_arg(ap, int32_t *);
				if (pdu_read(pdu, val32, sizeof(*val32))) {
					ret = -EFAULT;
					break;
				}
				*val32 = le32toh(*val32);
				break;
			case 'q':{
				int64_t *val64 = va_arg(ap, int64_t *);
				if (pdu_read(pdu, val64, sizeof(*val64))) {
					ret = -EFAULT;
					break;
				}
				*val64 = le64toh(*val64);
			}
			break;
			case 's':
				char **sptr = va_arg(ap, char **);
				uint16_t len;

				ret = p9pdu_readf(pdu, "w", &len);
				if (ret)
					break;

				*sptr = kmalloc(len + 1);
				if (*sptr == NULL) {
					ret = -ENOMEM;
					break;
				}

				if (pdu_read(pdu, *sptr, len)) {
					ret = -EFAULT;
					kfree(*sptr);
					*sptr = NULL;
				} else {
					(*sptr)[len] = 0;
				}

				break;
			case 'Q':
				struct p9_qid *qid = va_arg(ap, struct p9_qid *);
				ret = p9pdu_readf(pdu, "bdq", &qid->type, &qid->version, &qid->path);
				break;
			default:
				p9_fatal("p9pdu_vreadf: unknwon format %c\n", *ptr);
				break;
		}

		if (ret)
			break;
	}

	return ret;
}

/*
 * Write to packet.
 */
int p9pdu_writef(struct p9_fcall *pdu, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = p9pdu_vwritef(pdu, fmt, ap);
	va_end(ap);

	return ret;
}

/*
 * Read from a packet.
 */
int p9pdu_readf(struct p9_fcall *pdu, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = p9pdu_vreadf(pdu, fmt, ap);
	va_end(ap);

	return ret;
}

/*
 * Prepare a packet.
 */
int p9pdu_prepare(struct p9_fcall *pdu, int16_t tag, int8_t type)
{
	/* write packet size (0 for now), type and tag  */
	pdu->id = type;
	return p9pdu_writef(pdu, "dbw", 0, type, tag);
}

/*
 * Finalize a packet.
 */
int p9pdu_finalize(struct p9_fcall *pdu)
{
	int size = pdu->size, ret;

	/* write final size */
	pdu->size = 0;
	ret = p9pdu_writef(pdu, "d", size);
	pdu->size = size;

	return ret;
}

/*
 * Reset a packet.
 */
void p9pdu_reset(struct p9_fcall *pdu)
{
	pdu->offset = 0;
	pdu->size = 0;
}