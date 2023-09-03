#ifndef _IPC_H_
#define _IPC_H_

#include <stddef.h>

#define SEMOP		 1
#define SEMGET		 2
#define SEMCTL		 3
#define SEMTIMEDOP	 4
#define MSGSND		11
#define MSGRCV		12
#define MSGGET		13
#define MSGCTL		14
#define SHMAT		21
#define SHMDT		22
#define SHMGET		23
#define SHMCTL		24

#define IPC_PRIVATE	0
#define IPC_CREAT 	00001000
#define IPC_EXCL   	00002000

#define IPC_RMID 	0     		/* remove resource */
#define IPC_SET  	1     		/* set ipc_perm options */
#define IPC_STAT 	2     		/* get ipc_perm options */
#define IPC_INFO 	3     		/* see ipcs */

#define IPCMNI		256		/* maximum number of IPC elements */

/*
 * IPC permissions.
 */
struct ipc_perm {
	int			key;
	uid_t			uid;
	gid_t			gid;
	uid_t			cuid;
	gid_t			cgid;
	mode_t			mode;
	uint32_t		seq;
};

/*
 * IPC element.
 */
struct ipc_id {
	struct ipc_perm *	p;
};

/*
 * IPC elements.
 */
struct ipc_ids {
	int 			size;
	int 			in_use;
	int 			max_id;
	uint16_t 		seq;
	uint16_t 		seq_max;
	struct ipc_id *		entries;
};

int ipc_findkey(int key);
int ipc_add_id(struct ipc_perm *new);
struct ipc_perm *ipc_rm_id(int id);
int ipc_build_id(int id, int seq);
struct ipc_perm *ipc_get(int id);

int sys_ipc(uint32_t call, int first, int second, int third, void *ptr, int fifth);

#endif
