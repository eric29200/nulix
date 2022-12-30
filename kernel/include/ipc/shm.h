#ifndef _SHM_H_
#define _SHM_H_

#include <ipc/ipc.h>
#include <stddef.h>

#define SHMMIN 		1			/* min shared seg size (bytes) */
#define SHMMAX 		0x2000000		/* max shared seg size (bytes) */

#define	SHMLBA		PAGE_SIZE		/* attach addr a multiple of this */
#define	SHM_RDONLY	010000			/* read-only access */
#define	SHM_RND		020000			/* round attach address to SHMLBA boundary */
#define	SHM_REMAP	040000			/* take-over region on attach */

#define	SHM_DEST	01000			/* segment will be destroyed on last detach */

/*
 * Shared memory segment structure.
 */
struct shmid_t {
	struct ipc_perm_t	shm_perm;
	int			shm_id;
	struct file_t *		shm_filp;
	int			shm_size;
	int			shm_nattch;
	time_t			shm_atime;
	time_t			shm_dtime;
	time_t			shm_ctime;
	pid_t			shm_cprid;
	pid_t			shm_lprid;
};

/*
 * Shared memory user data structure.
 */
struct shmid_ds_t {
	struct ipc_perm_t 	shm_perm;
	size_t 			shm_segsz;
	uint32_t 		__shm_atime_lo;
	uint32_t		__shm_atime_hi;
	uint32_t 		__shm_dtime_lo;
	uint32_t		__shm_dtime_hi;
	uint32_t 		__shm_ctime_lo;
	uint32_t 		__shm_ctime_hi;
	pid_t 			shm_cpid;
	pid_t 			shm_lpid;
	uint32_t 		shm_nattch;
	uint32_t 		__pad1;
	uint32_t 		__pad2;
	uint32_t 		__pad3;
	int64_t 		shm_atime;
	int64_t 		shm_dtime;
	int64_t 		shm_ctime;
};

int do_shmget(int key, int size, int shmflg);
int do_shmat(int shmid, char *shmaddr, int shmflg, uint32_t *addr_ret);
int do_shmctl(int shmid, int cmd, struct shmid_ds_t *buf);
int do_shmdt(char *shmaddr);

#endif
