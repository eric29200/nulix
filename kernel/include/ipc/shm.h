#ifndef _SHM_H_
#define _SHM_H_

#include <ipc/ipc.h>

#define SHMMAX		0x2000000				/* max shared seg size (bytes) */
#define SHMMIN		1					/* min shared seg size (bytes) */
#define SHMMNI		4096					/* max number of segs system wide */
#define SHMALL		(SHMMAX / PAGE_SIZE * (SHMMNI / 16))	/* max shm system wide (pages) */
#define SHMSEG		SHMMNI					/* max shared segs per process */

#define	SHMLBA		PAGE_SIZE				/* attach addr a multiple of this */
#define	SHM_RDONLY	010000					/* read-only access */
#define	SHM_RND		020000					/* round attach address to SHMLBA boundary */
#define	SHM_REMAP	040000					/* take-over region on attach */

#define	SHM_DEST	01000					/* segment will be destroyed on last detach */

/* ipcs ctl commands */
#define SHM_STAT 	13
#define SHM_INFO 	14

/*
 * Shared memory user data structure.
 */
struct shmid_ds {
	struct ipc_perm 	shm_perm;
	size_t 			shm_segsz;
	time_t 			shm_atime;
	time_t	 		shm_dtime;
	time_t	 		shm_ctime;
	pid_t 			shm_cpid;
	pid_t 			shm_lpid;
	uint32_t 		shm_nattch;
	uint8_t			__unused[36];
};

/*
 * Buffer for shmctl call IPC_INFO.
 */
struct shminfo {
	unsigned long		shmmax;
	unsigned long		shmmin;
	unsigned long		shmmni;
	unsigned long		shmseg;
	unsigned long		shmall;
	unsigned long		__unused[4];
};

/*
 * Buffer for shmctl call SHM_INFO.
 */
struct shm_info {
	int			used_ids;
	unsigned long		shm_tot;	/* total allocated shm */
	unsigned long		shm_rss;	/* total resident shm */
	unsigned long		shm_swp;	/* total swapped shm */
	unsigned long		swap_attempts;
	unsigned long		swap_successes;
};

void init_shm();
int sys_shmget(key_t key, size_t size, int shmflg);
int sys_shmat(int shmid, char *shmaddr, int shmflg, uint32_t *addr_ret);
int sys_shmdt(char *shmaddr);
int sys_shmctl(int shmid, int cmd, struct shmid_ds *buf);

#endif