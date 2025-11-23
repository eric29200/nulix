#include <ipc/msg.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>

#define msg_buildid(id, seq)		ipc_buildid(&msg_ids, id, seq)
#define msg_checkid(msq, msgid)		ipc_checkid(&msg_ids, &msq->q_perm, msgid)
#define msg_rmid(id)			((struct msg_queue*) ipc_rmid(&msg_ids, id))

#define SEARCH_ANY			1
#define SEARCH_EQUAL			2
#define SEARCH_NOTEQUAL			3
#define SEARCH_LESSEQUAL		4

/*
 * Message.
 */
struct msg {
	int				m_type;			/* message type */
	size_t				m_ts;			/* message text size */
	char *				m_text;			/* message text */
	struct list_head		m_list;			/* next message on queue */
};

/*
 * Message queue.
 */
struct msg_queue {
	struct kern_ipc_perm		q_perm;
	time_t				q_stime;		/* last msgsnd time */
	time_t				q_rtime;		/* last msgrcv time */
	time_t				q_ctime;		/* last change time */
	size_t				q_cbytes;		/* current number of bytes on queue */
	size_t				q_qnum;			/* number of messages in queue */
	size_t				q_qbytes;		/* max number of bytes on queue */
	pid_t				q_lspid;		/* pid of last msgsnd */
	pid_t				q_lrpid;		/* last receive pid */
	struct wait_queue *		q_wait;			/* wait queue */
	struct list_head		q_messages;		/* messages */
};

/* global variables */
static struct ipc_ids msg_ids;
static int msg_bytes = 0;
static int msg_hdrs = 0;

/*
 * Create a new message queue.
 */
static int msg_newque(key_t key, int msgflg)
{
	struct msg_queue *msq;
	int id;

	/* allocate a new message queue */
	msq = (struct msg_queue *) kmalloc(sizeof(struct msg_queue));
	if (!msq)
		return -ENOMEM;

	/* register message queue */
	id = ipc_addid(&msg_ids, &msq->q_perm, MSGMNI);
	if (id == -1) {
		kfree(msq);
		return -ENOSPC;
	}

	/* set message queue */
	msq->q_perm.mode = (msgflg & S_IRWXUGO);
	msq->q_perm.key = key;
	msq->q_stime = 0;
	msq->q_rtime = 0;
	msq->q_ctime = CURRENT_TIME;
	msq->q_cbytes = 0;
	msq->q_qnum = 0;
	msq->q_qbytes = MSGMNB;
	msq->q_lspid = 0;
	msq->q_lrpid = 0;
	msq->q_wait = NULL;
	INIT_LIST_HEAD(&msq->q_messages);

	/* return message id */
	return msg_buildid(id, msq->q_perm.seq);
}

/*
 * Free a message queue.
 */
static void msg_freeque(int id)
{
	struct list_head *pos, *n;
	struct msg_queue *msq;
	struct msg *msg;

	/* remove message queue */
	msq = msg_rmid(id);

	/* free remaining messages */
	list_for_each_safe(pos, n, &msq->q_messages) {
		msg = list_entry(pos, struct msg, m_list);

		/* free msg */
		list_del(&msg->m_list);
		if (msg->m_text)
			kfree(msg->m_text);
		kfree(msg);

		/* update global number of messages */
		msg_hdrs--;
	}

	/* update global messages size */
	msg_bytes -= msq->q_cbytes;

	/* wakeup processes */
	wake_up(&msq->q_wait);

	/* free message queue */
	kfree(msq);
}

/*
 * Convert search mode.
 */
static int convert_mode(long *msgtyp, int msgflg)
{
	/*
	 *  find message of correct type.
	 *  msgtyp = 0 => get first.
	 *  msgtyp > 0 => get first message of matching type.
	 *  msgtyp < 0 => get message with least type must be < abs(msgtype).
	 */
	if (*msgtyp == 0)
		return SEARCH_ANY;

	if (*msgtyp < 0) {
		*msgtyp = -(*msgtyp);
		return SEARCH_LESSEQUAL;
	}

	if (msgflg & MSG_EXCEPT)
		return SEARCH_NOTEQUAL;

	return SEARCH_EQUAL;
}

/*
 * Test a message.
 */
static int testmsg(struct msg *msg, long type, int mode)
{
	switch(mode) {
		case SEARCH_ANY:
			return 1;
		case SEARCH_LESSEQUAL:
			if (msg->m_type <= type)
				return 1;
			break;
		case SEARCH_EQUAL:
			if (msg->m_type == type)
				return 1;
			break;
		case SEARCH_NOTEQUAL:
			if (msg->m_type != type)
				return 1;
			break;
	}

	return 0;
}

/*
 * Get a message queue.
 */
int sys_msgget(key_t key, int msgflg)
{
	struct msg_queue *msq;
	int id;

	/* private resource : create a new queue */
	if (key == IPC_PRIVATE)
		return msg_newque(key, msgflg);

	/* find message queue */
	id = ipc_findkey(&msg_ids, key);

	/* create message queue if needed */
	if (id == -1) {
		if (!(msgflg & IPC_CREAT))
			return -ENOENT;

		return msg_newque(key, msgflg);
	}

	/* exclusive access */
	if ((msgflg & IPC_CREAT) && (msgflg & IPC_EXCL))
		return -EEXIST;

	/* get message queue */
	msq = (struct msg_queue *) ipc_get(&msg_ids, id);

	/* check permissions */
	if (ipcperms(&msq->q_perm, msgflg))
		return -EACCES;

	/* build message id */
	return msg_buildid(id, msq->q_perm.seq);
}

/*
 * Send a message.
 */
int sys_msgsnd(int msqid, struct msgbuf *msgp, size_t msgsz, int msgflg)
{
	struct msg_queue *msq;
	struct msg *msg;

	/* check parameters */
	if (msgsz > MSGMAX || (long) msgsz < 0 || msqid < 0 || msgp->mtype < 1)
		return -EINVAL;

	/* get message queue */
	msq = (struct msg_queue *) ipc_get(&msg_ids, msqid);
	if (!msq)
		return -EINVAL;

retry:
	/* check message id */
	if (msg_checkid(msq, msqid))
		return -EIDRM;

	/* check permissions */
	if (ipcperms(&msq->q_perm, S_IWUGO))
		return -EACCES;

	/* message too big */
	if (msgsz + msq->q_cbytes > msq->q_qbytes || msq->q_qnum + 1 > msq->q_qbytes) {
		/* don't wait */
		if (msgflg & IPC_NOWAIT)
			return -EAGAIN;

		/* go to sleep */
		sleep_on(&msq->q_wait);

		/* reget message queue */
		msq = (struct msg_queue *) ipc_get(&msg_ids, msqid);
		if (!msq)
			return -EIDRM;

		/* handle signals */
		if (signal_pending(current_task))
			return -EINTR;

		goto retry;
	}

	/* allocate a new message */
	msg = (struct msg *) kmalloc(sizeof(struct msg));
	if (!msg)
		return -ENOMEM;

	/* allocate text */
	msg->m_text = (char *) kmalloc(msgsz);
	if (!msg->m_text) {
		kfree(msg);
		return -ENOMEM;
	}

	/* set message */
	INIT_LIST_HEAD(&msg->m_list);
	msg->m_type = msgp->mtype;
	msg->m_ts = msgsz;
	memcpy(msg->m_text, msgp->mtext, msgsz);

	/* insert message */
	list_add_tail(&msg->m_list, &msq->q_messages);

	/* update message queue */
	msq->q_lspid = current_task->pid;
	msq->q_stime = CURRENT_TIME;
	msq->q_cbytes += msgsz;
	msq->q_qnum++;

	/* update global stats */
	msg_bytes += msgsz;
	msg_hdrs++;

	/* wakeup eventual receivers */
	wake_up(&msq->q_wait);

	return 0;
}

/*
 * Receive a message.
 */
int sys_msgrcv(int msqid, struct msgbuf *msgp, size_t msgsz, long msgtyp, int msgflg)
{
	struct msg *msg, *found_msg;
	struct list_head *pos;
	struct msg_queue *msq;
	int mode;

	/* check parameters */
	if (msgsz > MSGMAX || (long) msgsz < 0)
		return -EINVAL;

	/* convert mode */
	mode = convert_mode(&msgtyp, msgflg);

	/* get message queue */
	msq = (struct msg_queue *) ipc_get(&msg_ids, msqid);
	if (!msq)
		return -EINVAL;

retry:
	/* check message id */
	if (msg_checkid(msq, msqid))
		return -EIDRM;

	/* check permissions */
	if (ipcperms(&msq->q_perm, S_IWUGO))
		return -EACCES;

	/* try to find a message */
	found_msg = NULL;
	list_for_each(pos, &msq->q_messages) {
		msg = list_entry(pos, struct msg, m_list);
		if (!testmsg(msg, msgtyp, mode))
			continue;

		if (mode == SEARCH_LESSEQUAL && msg->m_type != 1) {
			found_msg = msg;
			msgtyp = msg->m_type - 1;
		} else {
			found_msg = msg;
			goto found;
		}
	}

	/* no message found */
	if (msgflg & IPC_NOWAIT)
		return -ENOMSG;

	/* go to sleep */
	sleep_on(&msq->q_wait);

	/* reget message queue */
	msq = (struct msg_queue *) ipc_get(&msg_ids, msqid);
	if (!msq)
		return -EIDRM;

	/* handle signals */
	if (signal_pending(current_task))
		return -EINTR;

	goto retry;
found:
	msg = found_msg;

	/* message too big */
	if (msgsz < msg->m_ts && !(msgflg & MSG_NOERROR))
		return -E2BIG;

	/* copy message to user space */
	msgsz = msgsz > msg->m_ts ? msg->m_ts : msgsz;
	msgp->mtype = msg->m_type;
	memcpy(msgp->mtext, msg->m_text, msgsz);

	/* remove message from queue */
	list_del(&msg->m_list);

	/* update message queue */
	msq->q_qnum--;
	msq->q_rtime = CURRENT_TIME;
	msq->q_lrpid = current_task->pid;
	msq->q_cbytes -= msg->m_ts;

	/* update global stats */
	msg_bytes -= msg->m_ts;
	msg_hdrs--;

	/* wake up eventual senders */
	wake_up(&msq->q_wait);

	/* free message */
	kfree(msg->m_text);
	kfree(msg);

	return msgsz;
}

/*
 * Remove a message queue.
 */
static int msgctl_rmid(int msqid)
{
	struct msg_queue *msq;

	/* get message queue */
	msq = (struct msg_queue *) ipc_get(&msg_ids, msqid);
	if (!msq)
		return -EINVAL;

	/* check message id */
	if (msg_checkid(msq, msqid))
		return -EIDRM;

	/* check permissions */
	if (current_task->euid != msq->q_perm.cuid &&  current_task->euid != msq->q_perm.uid)
	    return -EPERM;

	/* free message queue */
	msg_freeque(msqid);

	return 0;
}

/*
 * Get status of a message queue.
 */
static int msgctl_stat(int msqid, int cmd, struct msqid_ds *p)
{
	struct msg_queue *msq;

	/* clear output */
	memset(p, 0, sizeof(*p));

	/* get message queue */
	msq = (struct msg_queue *) ipc_get(&msg_ids, msqid);
	if (!msq)
		return -EINVAL;

	/* check message id */
	if (cmd != MSG_STAT && msg_checkid(msq, msqid))
		return -EIDRM;

	/* get status */
	kernel_to_ipc_perm(&msq->q_perm, &p->msg_perm);
	p->msg_stime = msq->q_stime;
	p->msg_rtime = msq->q_rtime;
	p->msg_ctime = msq->q_ctime;
	p->msg_cbytes = msq->q_cbytes;
	p->msg_qnum = msq->q_qnum;
	p->msg_qbytes = msq->q_qbytes;
	p->msg_lspid = msq->q_lspid;
	p->msg_lrpid = msq->q_lrpid;

	return cmd == IPC_STAT ? 0 : msg_buildid(msqid, msq->q_perm.seq);
}

/*
 * Get informations of a message queue.
 */
static int msgctl_info(int cmd, struct msginfo *p)
{
	/* clear output */
	memset(p, 0, sizeof(struct msginfo));

	p->msgmni = MSGMNI;
	p->msgmax = MSGMAX;
	p->msgmnb = MSGMNB;
	p->msgssz = MSGSSZ;
	p->msgseg = MSGSEG;

	if (cmd == MSG_INFO) {
		p->msgpool = msg_ids.in_use;
		p->msgmap = msg_hdrs;
		p->msgtql = msg_bytes;
	} else {
		p->msgpool = MSGPOOL;
		p->msgmap = MSGMAP;
		p->msgtql = MSGTQL;
	}

	return msg_ids.max_id < 0 ? 0: msg_ids.max_id;
}

/*
 * Control a message queue.
 */
int sys_msgctl(int msqid, int cmd, void *buf)
{
	struct msqid_ds msqid_info;
	struct msginfo msginfo;
	int ret;

	/* check parameters */
	if (msqid < 0 || cmd < 0)
		return -EINVAL;

	/* parse version */
	ipc_parse_version(&cmd);

	/* handle command */
	switch (cmd) {
		case IPC_INFO:
		case MSG_INFO:
			/* get message queue informations */
			ret = msgctl_info(cmd, &msginfo);
			if (ret < 0)
				return ret;

			/* copy to user space */
			memcpy(buf, &msginfo, sizeof(struct msginfo));
			return ret;
		case IPC_STAT:
		case MSG_STAT:
			/* stat message queue */
			ret = msgctl_stat(msqid, cmd, &msqid_info);
			if (ret < 0)
				return ret;

			/* copy to user space */
			memcpy(buf, &msqid_info, sizeof(struct msqid_ds));
			return ret;
		case IPC_RMID:
			return msgctl_rmid(msqid);
		default:
			printf("sys_msgctl: unknown command %d\n", cmd);
			return -EINVAL;
	}

	return 0;
}

/*
 * Init IPC messages.
 */
void init_msg()
{
	ipc_init_ids(&msg_ids, MSGMNI);
}