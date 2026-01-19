#ifndef _SOCK_H_
#define _SOCK_H_

#include <lib/list.h>
#include <proc/timer.h>
#include <net/socket.h>
#include <net/inet/in.h>
#include <net/unix/un.h>
#include <net/sk_buff.h>

#define MIN_WRITE_SPACE			2048

/*
 * Socket credentials.
 */
struct ucred {
	pid_t				pid;
	uid_t				uid;
	gid_t				gid;
};

/*
 * UNIX options.
 */
struct unix_opt {
	struct unix_address *		addr;
	struct dentry *			dentry;
};

/*
 * TCP options.
 */
struct tcp_opt {
	uint32_t			snd_nxt;		/* next sequence we send */
	uint32_t			rcv_nxt;		/* what we want to receive next */
};

/*
 * Internal socket.
 */
struct sock {
	uint16_t			family;
	uint16_t			type;
	uint16_t			protocol;
	struct proto *			prot;
	struct socket *			socket;
	uint8_t				state;
	uint8_t				dead;
	int				err;
	off_t				msg_position;
	uint16_t			shutdown;
	size_t				rcvbuf;
	size_t				sndbuf;
	size_t				wmem_alloc;
	int				sock_readers;
	struct ucred 			peercred;
	int				proc;
	struct sock *			pair;
	struct wait_queue **		sleep;
	union {
		struct unix_opt		af_unix;
		struct tcp_opt		af_tcp;
	} protinfo;
	uint16_t			num;
	uint32_t			saddr;
	uint32_t			rcv_saddr;
	uint16_t			sport;
	uint32_t			daddr;
	uint16_t			dport;
	struct list_head		list;
	struct sk_buff_head		receive_queue;
	int				timeout;
	struct timer_event		timer;
	void				(*state_change)(struct sock *);
	void				(*data_ready)(struct sock *, size_t);
	void				(*destruct)(struct sock *);
};

/*
 * Inet protocol.
 */
struct proto {
	int (*handle)(struct sock *, struct sk_buff *);
	int (*close)(struct sock *);
	int (*recvmsg)(struct sock *, struct msghdr *, size_t);
	int (*sendmsg)(struct sock *, const struct msghdr *, size_t);
	int (*connect)(struct sock *, const struct sockaddr *, size_t);
	int (*poll)(struct socket *, struct select_table *);
};

/* inet protocols */
extern struct proto udp_prot;
extern struct proto tcp_prot;
extern struct proto raw_prot;

struct sock *sk_alloc(int family, int zero_it);
void sk_free(struct sock *sk);
int sock_error(struct sock *sk);
void sock_init_data(struct socket *sock, struct sock *sk);
int sock_no_dup(struct socket *newsock, struct socket *oldsock);
int sock_no_listen(struct socket *sock);
int sock_no_accept(struct socket *sock, struct socket *sock_new, int flags);
int sock_no_getsockopt(struct socket *sock, int level, int optname, void *optval, size_t *optlen);
int sock_no_setsockopt(struct socket *sock, int level, int optname, void *optval, size_t optlen);

struct sk_buff *sock_wmalloc(struct sock *sk, size_t size, int force);
struct sk_buff *sock_alloc_send_skb(struct sock *sk, size_t len, int nonblock, int *err);
struct sk_buff *skb_recv_datagram(struct sock *sk, int flags, int noblock, int *err);
void skb_copy_datagram_iovec(struct sk_buff *skb, int offset, struct iovec *to, size_t size);
int datagram_poll(struct socket *sock, struct select_table *wait);

/*
 * Get write space of a socket.
 */
static inline size_t sock_wspace(struct sock *sk)
{
	int res = 0;

	if (!(sk->shutdown & SEND_SHUTDOWN)) {
		res = sk->sndbuf - sk->wmem_alloc;
		if (res < 0)
			res = 0;
	}

	return res;
}

#endif
