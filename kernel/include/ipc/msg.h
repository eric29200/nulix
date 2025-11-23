#ifndef _MSG_H_
#define _MSG_H_

#include <ipc/ipc.h>

#define MSGMNI			16				/* max number of msg queue identifiers */
#define MSGMAX			8192				/* max size of message (bytes) */
#define MSGMNB			16384				/* max size of a message queue */
#define MSGSSZ			16				/* message segment size */
#define MSGPOOL			(MSGMNI * MSGMNB / 1024)	/* size in kbytes of message pool */
#define MSGTQL			MSGMNB				/* number of system message headers */
#define MSGMAP			MSGMNB				/* number of entries in message map */
#define __MSGSEG		((MSGPOOL * 1024) / MSGSSZ)	/* max number of segments */
#define MSGSEG			(__MSGSEG <= 0xFFFF ? __MSGSEG : 0xFFFF)


/* msgrcv options */
#define MSG_NOERROR    		010000		/* no error if message is too big */
#define MSG_EXCEPT      	020000		/* recv any msg except of specified type.*/

/* ipcs ctl commands */
#define MSG_STAT		11
#define MSG_INFO		12

/*
 * Message buffer (for msgsnd and msgrcv calls).
 */
struct msgbuf {
	long			mtype;         /* type of message */
	char			mtext[1];      /* message text */
};

/*
 * Message queue data structure.
 */
struct msqid64_ds {
	struct ipc64_perm	msg_perm;
	time_t			msg_stime;	/* last msgsnd time */
	time_t			msg_rtime;	/* last msgrcv time */
	time_t			msg_ctime;	/* last change time */
	uint64_t		msg_cbytes;	/* current number of bytes on queue */
	uint64_t		msg_qnum;	/* number of messages in queue */
	uint64_t		msg_qbytes;	/* max number of bytes on queue */
	pid_t			msg_lspid;	/* pid of last msgsnd */
	pid_t			msg_lrpid;	/* last receive pid */
	uint64_t		__unused4;
	uint64_t		__unused5;
};

/*
 * Buffer for msgctl calls IPC_INFO and MSG_INFO.
 */
struct msginfo {
	int			msgpool;
	int			msgmap;
	int			msgmax;
	int			msgmnb;
	int			msgmni;
	int			msgssz;
	int			msgtql;
	unsigned short		msgseg;
};

void init_msg();
int sys_msgget(key_t key, int msgflg);
int sys_msgsnd(int msqid, struct msgbuf *msgp, size_t msgsz, int msgflg);
int sys_msgrcv(int msqid, struct msgbuf *msgp, size_t msgsz, long msgtyp, int msgflg);
int sys_msgctl(int msqid, int cmd, void *buf);

#endif
