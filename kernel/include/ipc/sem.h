#ifndef _SEM_H_
#define _SEM_H_

#include <ipc/ipc.h>

#define SEMMNI			128			/* max number of semaphore identifiers */
#define SEMMSL			250			/* max number of semaphores per id */
#define SEMMNS			(SEMMNI * SEMMSL)	/* max number of semaphores in system */
#define SEMOPM			32			/* max number of ops per semop call */
#define SEMVMX			32767			/* semaphore max value */
#define SEMAEM			SEMVMX			/* adjust on exit max value */

/* unused */
#define SEMUME			SEMOPM			/* max number of undo entries per process */
#define SEMMNU			SEMMNS			/* number of undo structures system wide */
#define SEMMAP			SEMMNS			/* number of entries in semaphore map */
#define SEMUSZ			20			/* sizeof struct sem_undo */

/* semop flags */
#define SEM_UNDO		0x1000			/* undo the operation on exit */

/* semctl command definitions */
#define GETPID			11			/* get sempid */
#define GETVAL			12			/* get semval */
#define GETALL			13			/* get all semval's */
#define GETNCNT			14			/* get semncnt */
#define GETZCNT			15			/* get semzcnt */
#define SETVAL			16			/* set semval */
#define SETALL			17			/* set all semval's */
#define SEM_STAT		18
#define SEM_INFO		19

/*
 * Undo requests.
 */
struct sem_undo {
	struct sem_undo *	proc_next;		/* next entry on this process */
	struct sem_undo *	id_next;		/* next entry on this semaphore set */
	int			semid;			/* semaphore set identifier */
	short *			semadj;			/* array of adjustments, one per semaphore */
};

/*
 * Semaphore operation buffer (used by sys_semop).
 */
struct sembuf {
	unsigned short 		sem_num;		/* semaphore index in array */
	short			sem_op;			/* semaphore operation */
	short			sem_flg;		/* operation flags */
};

/*
 * Semaphores array data structure.
 */
struct semid_ds {
	struct ipc_perm		sem_perm;		/* permissions */
	time_t			sem_otime;		/* last semop time */
	time_t			sem_ctime;		/* last change time */
	unsigned short		sem_nsems;		/* number of semaphores in array */
	uint8_t			__unused[24];
};

/*
 * Argument for sys_semctl.
 */
union semun {
	int			val;			/* value for SETVAL */
	struct semid_ds *	buf;			/* buffer for IPC_STAT & IPC_SET */
	unsigned short *	array;			/* array for GETALL & SETALL */
};

/*
 * Semaphores information.
 */
struct seminfo {
	int			semmap;
	int			semmni;
	int			semmns;
	int			semmnu;
	int			semmsl;
	int			semopm;
	int			semume;
	int			semusz;
	int			semvmx;
	int			semaem;
};

void init_sem();
void sem_exit();
int sys_semop(int semid, struct sembuf *sops, size_t nsops);
int sys_semget(key_t key, int nsems, int semflg);
int sys_semctl(int semid, int semnum, int cmd, void *buf);

#endif