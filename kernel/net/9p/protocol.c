#include <net/9p/9p.h>
#include <stdio.h>
#include <stderr.h>
#include <string.h>
#include <mm/mm.h>
#include <x86/endian.h>

#define min(x, y)		((x) <= (y) ? (x) : (y))
#define max(x, y)		((x) >= (y) ? (x) : (y))
#define P9_STRLEN(s)		(2 + min(s ? strlen(s) : 0, USHRT_MAX))

/*
 * Get needed size packet.
 */
size_t p9_msg_buf_size(int8_t type, const char *fmt, va_list ap)
{
	const int hdr = 4 + 1 + 2;
	const int err_size = hdr + 4;

	switch (type) {
		case P9_TVERSION:
		case P9_RVERSION:
		case P9_RATTACH:
		case P9_TCLUNK:
		case P9_RCLUNK:
		case P9_TGETATTR:
		case P9_RGETATTR:
		case P9_TLOPEN:
		case P9_RLOPEN:
		case P9_TREADDIR:
		case P9_TREAD:
		case P9_TSTATFS:
		case P9_RSTATFS:
		case P9_TREADLINK:
		case P9_RMKDIR:
		case P9_RMKNOD:
		case P9_RLINK:
		case P9_TSYMLINK:
		case P9_RSYMLINK:
		case P9_TREMOVE:
		case P9_RREMOVE:
		case P9_RRENAME:
		case P9_RWRITE:
			return 4096;
		case P9_RREADLINK:
		case P9_TMKDIR:
		case P9_TMKNOD:
		case P9_TLINK:
		case P9_TRENAME:
			return 8192;
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
		case P9_TWALK:
			if (strcmp("ddT", fmt))
				p9_fatal("p9_msg_buf_size: wrong format for message %d\n", type);
			va_arg(ap, int32_t);
			va_arg(ap, int32_t);
			{
				uint32_t i, nwname = va_arg(ap, int);
				size_t wname_all;
				const char **wnames = va_arg(ap, const char **);
				for (i = 0, wname_all = 0; i < nwname; i++)
					wname_all += P9_STRLEN(wnames[i]);
				/* fid[4] newfid[4] nwname[2] nwname*(wname[s]) */
				return hdr + 4 + 4 + 2 + wname_all;
			}
		case P9_RWALK:
			if (strcmp("ddT", fmt))
				p9_fatal("p9_msg_buf_size: wrong format for message %d\n", type);
			va_arg(ap, int32_t);
			va_arg(ap, int32_t);
			{
				uint32_t nwname = va_arg(ap, int);
				/* nwqid[2] nwqid*(wqid[13]) */
				return max(hdr + 6 + nwname * 13, err_size);
			}
		case P9_RREAD:
		case P9_RREADDIR:
			if (strcmp("dqd", fmt))
				p9_fatal("p9_msg_buf_size: wrong format for message %d\n", type);
			va_arg(ap, int32_t);
			va_arg(ap, int64_t);
			{
				const int32_t count = va_arg(ap, int32_t);
				/* count[4] data[count] */
				return max(hdr + 4 + count, err_size);
			}
		case P9_TWRITE:
			if (strcmp("dqD", fmt))
				p9_fatal("p9_msg_buf_size: wrong format for message %d\n", type);
			va_arg(ap, int32_t);
			va_arg(ap, int64_t);
			{
				const int32_t count = va_arg(ap, int32_t);
				/* fid[4] offset[8] count[4] data[count] */
				return hdr + 4 + 8 + 4 + count;
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
			case 'q':
				int64_t val64 = htole64(va_arg(ap, int64_t));
				if (pdu_write(pdu, &val64, sizeof(val64)))
					ret = -EFAULT;
				break;
			case 'u':
				uid_t uid = htole32(va_arg(ap, uid_t));
				if (pdu_write(pdu, &uid, sizeof(uid)))
					ret = -EFAULT;
				break;
			case 'g':
				gid_t gid = htole32(va_arg(ap, gid_t));
				if (pdu_write(pdu, &gid, sizeof(gid)))
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
			case 'T':
				uint16_t nwname = va_arg(ap, int);
				const char **wnames = va_arg(ap, const char **);
				ret = p9pdu_writef(pdu, "w", nwname);
				if (ret == 0) {
					int i;

					for (i = 0; i < nwname; i++) {
						ret = p9pdu_writef(pdu, "s", wnames[i]);
						if (ret)
							break;
					}
				}
				break;
			case 'D':
				uint32_t count = va_arg(ap, uint32_t);
				const void *data = va_arg(ap, const void *);
				ret = p9pdu_writef(pdu, "d", count);
				if (ret == 0 && pdu_write(pdu, data, count))
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
	int ret = 0, i;

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
			case 'q':
				int64_t *val64 = va_arg(ap, int64_t *);
				if (pdu_read(pdu, val64, sizeof(*val64))) {
					ret = -EFAULT;
					break;
				}
				*val64 = le64toh(*val64);
				break;
			case 'u':
				uid_t *uid = va_arg(ap, uid_t *);
				if (pdu_read(pdu, uid, sizeof(*uid))) {
					ret = -EFAULT;
					break;
				}
				*uid = le32toh(*uid);
				break;
			case 'g':
				gid_t *gid = va_arg(ap, gid_t *);
				if (pdu_read(pdu, gid, sizeof(*gid))) {
					ret = -EFAULT;
					break;
				}
				*gid = le32toh(*gid);
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
			case 'A':
				struct p9_stat *st = va_arg(ap, struct p9_stat *);
				memset(st, 0, sizeof(struct p9_stat));
				ret = p9pdu_readf(pdu, "qQdugqqqqqqqqqqqqqqq",
					&st->st_result_mask,
					&st->qid,
					&st->st_mode,
					&st->st_uid,
					&st->st_gid,
					&st->st_nlink,
					&st->st_rdev,
					&st->st_size,
					&st->st_blksize,
					&st->st_blocks,
					&st->st_atime_sec,
					&st->st_atime_nsec,
					&st->st_mtime_sec,
					&st->st_mtime_nsec,
					&st->st_ctime_sec,
					&st->st_ctime_nsec,
					&st->st_btime_sec,
					&st->st_btime_nsec,
					&st->st_gen,
					&st->st_data_version);
				break;
			case 'R':
				uint16_t *nwqid = va_arg(ap, uint16_t *);
				struct p9_qid **wqids = va_arg(ap, struct p9_qid **);
				*wqids = NULL;

				ret = p9pdu_readf(pdu, "w", nwqid);
				if (ret == 0) {
					*wqids = (struct p9_qid *) kmalloc(sizeof(struct p9_qid) * *nwqid);
					if (!*wqids)
						ret = -ENOMEM;
				}

				if (ret == 0) {
					for (i = 0; i < *nwqid; i++) {
						ret = p9pdu_readf(pdu, "Q", &(*wqids)[i]);
						if (ret)
							break;
					}
				}

				if (ret) {
					kfree(*wqids);
					*wqids = NULL;
				}

				break;
			case 'D':
				uint32_t *count = va_arg(ap, uint32_t *);
				void **data = va_arg(ap, void **);

				ret = p9pdu_readf(pdu, "d", count);
				if (ret == 0) {
					*count = min(*count, pdu->size - pdu->offset);
					*data = &pdu->sdata[pdu->offset];
				}
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

/*
 * Read a directory entry.
 */
int p9dirent_read(char *buf, int len, struct p9_dirent *dirent)
{
	struct p9_fcall fake_pdu;
	char *nameptr;
	int ret;

	/* create a fake packet */
	fake_pdu.size = len;
	fake_pdu.capacity = len;
	fake_pdu.sdata = (uint8_t *) buf;
	fake_pdu.offset = 0;

	/* read directory entry */
	ret = p9pdu_readf(&fake_pdu, "Qqbs", &dirent->qid, &dirent->d_off, &dirent->d_type, &nameptr);
	if (ret)
		return ret;

	/* copy name */
	strncpy(dirent->d_name, nameptr, sizeof(dirent->d_name));
	kfree(nameptr);

	return fake_pdu.offset;
}