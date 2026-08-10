#ifndef _NET_9P_H_
#define _NET_9P_H_

#include <stdio.h>
#include <lib/parser.h>
#include <lib/list.h>

#define P9_PROTO_2000L			2
#define P9_PORT				564

#define P9_IOHDRSZ			24

#define P9_CLIENT_CONNECTED		1
#define P9_CLIENT_DISCONNECTED		2

#define P9_NOTAG			((uint16_t) (~0))
#define P9_NOFID			((uint32_t) (~0))

#define P9_TLERROR			6
#define P9_RLERROR			7
#define P9_TVERSION			100
#define P9_RVERSION			101
#define P9_TATTACH			104
#define P9_RATTACH			105
#define P9_TERROR			106
#define P9_RERROR			107

/*
 * 9p client.
 */
struct p9_client {
	uint8_t				status;
	int				msize;
	uint8_t				proto_version;
	uint16_t			tag;
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
	struct p9_fcall  		tc;
	struct p9_fcall 		rc;
};

/*
 * 9p transport module.
 */
struct p9_trans_module {
	struct list_head		list;
	char *				name;
	int				maxsize;
	int 				(*create)(struct p9_client *, const char *, char *);
	void				(*close) (struct p9_client *);
	int				(*request) (struct p9_client *, struct p9_request *req);
};

/* init functions */
int init_p9();
int p9_trans_fd_init();

/* client functions */
struct p9_client *p9_client_create(const char *dev_name, char *options);
int p9_parse_header(struct p9_fcall *fc, int32_t *size, int8_t *type, int16_t *tag);
int p9_client_version(struct p9_client *client);

/* transport functions */
void v9fs_register_trans(struct p9_trans_module *trans);
struct p9_trans_module *v9fs_get_trans_by_name(const struct substring *name);
struct p9_trans_module *v9fs_get_default_trans();

/* packet functions */
int p9_msg_buf_size(int8_t type, const char *fmt, va_list ap);
int p9pdu_writef(struct p9_fcall *pdu, const char *fmt, ...);
int p9pdu_vwritef(struct p9_fcall *pdu, const char *fmt, va_list ap);
int p9pdu_vreadf(struct p9_fcall *pdu, const char *fmt, va_list ap);
int p9pdu_readf(struct p9_fcall *pdu, const char *fmt, ...);
int p9pdu_prepare(struct p9_fcall *pdu, int16_t tag, int8_t type);
int p9pdu_finalize(struct p9_fcall *pdu);
void p9pdu_reset(struct p9_fcall *pdu);

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
	va_list ap;

	/* print panic */
	printf("[9p DEBUG] ");

	/* print in tmp buf */
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

#endif