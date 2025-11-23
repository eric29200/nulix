#ifndef _IPC_H_
#define _IPC_H_

#include <stddef.h>

#define IPC_PRIVATE		((key_t) 0)

/* resource get request flags */
#define IPC_CREAT		00001000	/* create if key is nonexistent */
#define IPC_EXCL		00002000	/* fail if key exists */
#define IPC_NOWAIT		00004000	/* return error on wait */

#define IPCMNI			32768		/* maximum size for IPC arrays */
#define SEQ_MULTIPLIER		(IPCMNI)

#define IPC_OLD			0		/* old version (no 32-bit UID support on many architectures) */
#define IPC_64			0x0100  	/* new version (support 32-bit UIDs, bigger message sizes, etc. */

/*  Control commands */
#define IPC_RMID		0
#define IPC_SET			1
#define IPC_STAT		2
#define IPC_INFO		3

/* IPC commands */
#define SEMOP			1
#define SEMGET			2
#define SEMCTL			3
#define SEMTIMEDOP		4
#define MSGSND			11
#define MSGRCV			12
#define MSGGET			13
#define MSGCTL			14
#define SHMAT			21
#define SHMDT			22
#define SHMGET			23
#define SHMCTL			24

/*
 * IPC permissions.
 */
struct ipc_perm {
	key_t			key;
	uid_t			uid;
	gid_t			gid;
	uid_t			cuid;
	gid_t			cgid;
	mode_t			mode;
	int			seq;
	long			__pad1;
	long			__pad2;
};

/*
 * IPC permissions.
 */
struct kern_ipc_perm {
	key_t			key;
	uid_t			uid;
	gid_t			gid;
	uid_t			cuid;
	gid_t			cgid;
	mode_t			mode;
	uint32_t		seq;
};

/*
 * IPC identifier.
 */
struct ipc_id {
	struct kern_ipc_perm *	p;
};

/*
 * IPC identifiers set.
 */
struct ipc_ids {
	int			size;
	int			in_use;
	int			max_id;
	uint16_t		seq;
	uint16_t		seq_max;
	struct ipc_id *		entries;
};

/*
 * Wrap system call.
 */
struct ipc_kludge {
	struct msgbuf *		msgp;
	long			msgtyp;
};

void init_ipc();
void kernel_to_ipc_perm(struct kern_ipc_perm *in, struct ipc_perm *out);
int ipc_parse_version(int *cmd);
int ipc_buildid(struct ipc_ids *ids, int id, int seq);
int ipc_addid(struct ipc_ids *ids, struct kern_ipc_perm *new, int size);
struct kern_ipc_perm *ipc_rmid(struct ipc_ids *ids, int id);
void ipc_init_ids(struct ipc_ids *ids, int size);
int ipcperms(struct kern_ipc_perm *ipcp, short flag);
int ipc_checkid(struct ipc_ids *ids, struct kern_ipc_perm * ipcp, int uid);
struct kern_ipc_perm *ipc_get(struct ipc_ids *ids, int id);
int ipc_findkey(struct ipc_ids *ids, key_t key);
int sys_ipc(uint32_t call, int first, int second, int third, void *ptr, int fifth);

#endif