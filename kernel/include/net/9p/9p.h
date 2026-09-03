#ifndef _NET_9P_H_
#define _NET_9P_H_

#include <stdio.h>
#include <lib/parser.h>
#include <lib/list.h>
#include <proc/wait.h>
#include <stdio.h>
#include <config.h>

#define P9_PROTO_2000L			2
#define P9_PORT				564

#define P9_HDRSZ			7
#define P9_IOHDRSZ			24
#define P9_READDIRHDRSZ			24

#define P9_CLIENT_CONNECTED		1
#define P9_CLIENT_DISCONNECTED		2

#define P9_REQUEST_STATUS_IDLE		0
#define P9_REQUEST_STATUS_ALLOC		1
#define P9_REQUEST_STATUS_UNSENT	2
#define P9_REQUEST_STATUS_SENT		3
#define P9_REQUEST_STATUS_FLSH		4
#define P9_REQUEST_STATUS_RCVD		5
#define P9_REQUEST_STATUS_FLSHD		6
#define P9_REQUEST_STATUS_ERROR		7

#define P9_NOTAG			((uint16_t) (~0))
#define P9_NOFID			((uint32_t) (~0))
#define P9_MAXWELEM			16

#define P9_TLERROR			6
#define P9_RLERROR			7
#define P9_TSTATFS			8
#define P9_RSTATFS			9
#define P9_TLOPEN			12
#define P9_RLOPEN			13
#define P9_TSYMLINK			16
#define P9_RSYMLINK			17
#define P9_TMKNOD			18
#define P9_RMKNOD			19
#define P9_TRENAME			20
#define P9_RRENAME			21
#define P9_TREADLINK			22
#define P9_RREADLINK			23
#define P9_TGETATTR			24
#define P9_RGETATTR			25
#define P9_TREADDIR			40
#define P9_RREADDIR			41
#define P9_TLINK			70
#define P9_RLINK			71
#define P9_TMKDIR			72
#define P9_RMKDIR			73
#define P9_TVERSION			100
#define P9_RVERSION			101
#define P9_TATTACH			104
#define P9_RATTACH			105
#define P9_TERROR			106
#define P9_RERROR			107
#define P9_TFLUSH			108
#define P9_RFLUSH			109
#define P9_TWALK			110
#define P9_RWALK			111
#define P9_TREAD			116
#define P9_RREAD			117
#define P9_TWRITE			118
#define P9_RWRITE			119
#define P9_TCLUNK			120
#define P9_RCLUNK			121
#define P9_TREMOVE			122
#define P9_RREMOVE			123

#define P9_STATS_BASIC			0x000007FFULL
#define P9_STATS_ALL			0x00003FFFULL
#define P9_STATS_MODE			0x00000001ULL
#define P9_STATS_NLINK			0x00000002ULL
#define P9_STATS_UID			0x00000004ULL
#define P9_STATS_GID			0x00000008ULL
#define P9_STATS_RDEV			0x00000010ULL
#define P9_STATS_ATIME			0x00000020ULL
#define P9_STATS_MTIME			0x00000040ULL
#define P9_STATS_CTIME			0x00000080ULL
#define P9_STATS_INO			0x00000100ULL
#define P9_STATS_SIZE			0x00000200ULL
#define P9_STATS_BLOCKS			0x00000400ULL
#define P9_STATS_BTIME			0x00000800ULL
#define P9_STATS_GEN			0x00001000ULL
#define P9_STATS_DATA_VERSION		0x00002000ULL

#define P9_MODE_MASK			0x1FFF

/*
 * 9p client.
 */
struct p9_client {
	uint8_t				status;
	uint32_t			msize;
	uint8_t				proto_version;
	uint16_t			tag;
	uint32_t			fid;
	struct p9_trans_module *	trans_mod;
	void *				trans;
	struct list_head 		fid_list;
};

/*
 * 9p packet.
 */
struct p9_fcall {
	uint32_t			size;
	uint8_t				id;
	uint16_t			tag;
	size_t				offset;
	size_t				capacity;
	uint8_t *			sdata;
};

/*
 * 9p request.
 */
struct p9_request {
	struct p9_fcall			tc;
	struct p9_fcall 		rc;
	int				t_err;
	int				status;
	struct wait_queue_head		wait;
	struct list_head		list;
};

/*
 * 9p transport module.
 */
struct p9_trans_module {
	struct list_head		list;
	char *				name;
	uint8_t				def;
	uint32_t			maxsize;
	int 				(*create)(struct p9_client *, const char *, char *);
	void				(*close) (struct p9_client *);
	int				(*request) (struct p9_client *, struct p9_request *req);
	int				(*cancel)(struct p9_client *, struct p9_request *req);
};

/*
 * File system entity information (server side).
 */
struct p9_qid {
	uint8_t			type;
	uint32_t		version;
	uint64_t		path;
};

/*
 * File stats.
 */
struct p9_stat {
	uint64_t		st_result_mask;
	struct p9_qid		qid;
	uint32_t 		st_mode;
	uid_t 			st_uid;
	gid_t 			st_gid;
	uint64_t 		st_nlink;
	uint64_t 		st_rdev;
	uint64_t 		st_size;
	uint64_t 		st_blksize;
	uint64_t 		st_blocks;
	uint64_t 		st_atime_sec;
	uint64_t 		st_atime_nsec;
	uint64_t 		st_mtime_sec;
	uint64_t 		st_mtime_nsec;
	uint64_t 		st_ctime_sec;
	uint64_t 		st_ctime_nsec;
	uint64_t 		st_btime_sec;
	uint64_t 		st_btime_nsec;
	uint64_t 		st_gen;
	uint64_t 		st_data_version;
};

/*
 * File system entity handle (client side).
 */
struct p9_fid {
	struct p9_client *	client;
	uint32_t		fid;
	int			mode;
	struct p9_qid		qid;
	uint32_t		iounit;
	uid_t			uid;
	void *			rdir;
	struct list_head	flist;
	struct list_head	dlist;
};

/*
 * Directory entry.
 */
struct p9_dirent {
	struct p9_qid		qid;
	uint64_t		d_off;
	uint8_t			d_type;
	char			d_name[256];
};

/*
 * File system statistics.
 */
struct p9_rstatfs {
	uint32_t 		type;
	uint32_t 		bsize;
	uint64_t 		blocks;
	uint64_t 		bfree;
	uint64_t 		bavail;
	uint64_t 		files;
	uint64_t 		ffree;
	uint64_t 		fsid;
	uint32_t 		namelen;
};

/* init functions */
int init_p9();
int p9_trans_fd_init();
int p9_trans_virtio_init();

/* client functions */
struct p9_client *p9_client_create(const char *dev_name, char *options);
void p9_client_destroy(struct p9_client *client);
void p9_client_cb(struct p9_request *req);
int p9_client_version(struct p9_client *client);
struct p9_fid *p9_client_attach(struct p9_client *client, struct p9_fid *afid, const char *uname, uid_t n_uname, const char *aname);
int p9_client_clunk(struct p9_fid *fid);
struct p9_stat *p9_client_getattr(struct p9_fid *fid, uint64_t request_mask);
int p9_client_open(struct p9_fid *fid, int mode);
struct p9_fid *p9_client_walk(struct p9_fid *oldfid, uint16_t nwname, char **wnames, int clone);
int p9_client_readdir(struct p9_fid *fid, char *buf, uint32_t count, uint64_t offset);
int p9_client_read(struct p9_fid *fid, char *buf, uint64_t offset, uint32_t count);
int p9_client_write(struct p9_fid *fid, const char *buf, uint64_t offset, uint32_t count);
int p9_client_statfs(struct p9_fid *fid, struct p9_rstatfs *st);
int p9_client_readlink(struct p9_fid *fid, char **target);
int p9_client_mkdir(struct p9_fid *fid, const char *name, int mode, gid_t gid, struct p9_qid *qid);
int p9_client_mknod(struct p9_fid *fid, const char *name, int mode, dev_t rdev, gid_t gid, struct p9_qid *qid);
int p9_client_link(struct p9_fid *dfid, struct p9_fid *oldfid, const char *newname);
int p9_client_symlink(struct p9_fid *dfid, const char *name, const char *symtgt, gid_t gid, struct p9_qid *qid);
int p9_client_remove(struct p9_fid *fid);
int p9_client_rename(struct p9_fid *fid, struct p9_fid *newdirfid, const char *name);
int p9_client_flush(struct p9_client *client, struct p9_request *oldreq);

/* transport functions */
void v9fs_register_trans(struct p9_trans_module *trans);
struct p9_trans_module *v9fs_get_trans_by_name(const struct substring *name);
struct p9_trans_module *v9fs_get_default_trans();

/* packet functions */
size_t p9_msg_buf_size(int8_t type, const char *fmt, va_list ap);
int p9pdu_writef(struct p9_fcall *pdu, const char *fmt, ...);
int p9pdu_vwritef(struct p9_fcall *pdu, const char *fmt, va_list ap);
int p9pdu_vreadf(struct p9_fcall *pdu, const char *fmt, va_list ap);
int p9pdu_readf(struct p9_fcall *pdu, const char *fmt, ...);
int p9pdu_prepare(struct p9_fcall *pdu, int16_t tag, int8_t type);
int p9pdu_finalize(struct p9_fcall *pdu);
void p9pdu_reset(struct p9_fcall *pdu);
int p9dirent_read(char *buf, int len, struct p9_dirent *dirent);

/*
 * Print a fatal message.
 */
static inline void p9_fatal(const char *fmt, ...)
{
	va_list ap;

	/* print panic */
	printf("[9p FATAL] ");

	/* print in tmp buf */
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);

	for (;;);
}

/*
 * Print an error message.
 */
static inline void p9_error(const char *fmt, ...)
{
	va_list ap;

	/* print panic */
	printf("[9p ERROR] ");

	/* print in tmp buf */
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

/*
 * Print a debug message.
 */
static inline void p9_debug(const char *fmt, ...)
{
#if P9_DEBUG >= 1
	va_list ap;

	/* print panic */
	printf("[9p DEBUG] ");

	/* print in tmp buf */
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
#else
	UNUSED(fmt);
#endif
}

#endif
