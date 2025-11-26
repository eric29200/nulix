#include <ipc/sem.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>

#define sem_buildid(id, seq)		ipc_buildid(&sem_ids, id, seq)
#define sem_checkid(sma, semid)		ipc_checkid(&sem_ids, &sma->sem_perm, semid)
#define sem_rmid(id)			((struct sem_array *) ipc_rmid(&sem_ids, id))

/*
 * Semaphore.
 */
struct sem {
	int			semval;		/* current value */
	pid_t			sempid;		/* pid of last operation */
};

/*
 * Semaphores array.
 */
struct sem_array {
	struct kern_ipc_perm	sem_perm;	/* permissions */
	time_t			sem_otime;	/* last semop time */
	time_t			sem_ctime;	/* last change time */
	struct sem *		sem_base;	/* ptr to first semaphore in array */
	struct wait_queue *	sem_wait;	/* processes waiting for semval */
	struct sem_undo	*	undo;		/* undo requests on this array */
	size_t			sem_nsems;	/* number of semaphores in array */
};

/* global variables */
static struct ipc_ids sem_ids;
static int used_sems = 0;

/*
 * Create a new semaphores array.
 */
static int sem_newary(key_t key, int nsems, int semflg)
{
	struct sem_array *sma;
	size_t size;
	int id;

	/* check number of semaphores */
	if (!nsems)
		return -EINVAL;
	if (used_sems + nsems > SEMMNS)
		return -ENOSPC;

	/* allocate semaphores array */
	size = sizeof (struct sem_array) + sizeof (struct sem) * nsems;
	sma = (struct sem_array *) kmalloc(size);
	if (!sma)
		return -ENOMEM;

	/* register semaphores array */
	id = ipc_addid(&sem_ids, &sma->sem_perm, SEMMNI);
	if (id == -1) {
		kfree(sma);
		return -ENOSPC;
	}

	/* set semaphires array */
	memset(sma, 0, size);
	sma->sem_perm.mode = (semflg & S_IRWXUGO);
	sma->sem_perm.key = key;
	sma->sem_base = (struct sem *) &sma[1];
	sma->sem_nsems = nsems;
	sma->sem_ctime = CURRENT_TIME;

	/* update global stats */
	used_sems += nsems;

	/* return semaphores array id */
	return sem_buildid(id, sma->sem_perm.seq);
}

/*
 * Free a semaphores array.
 */
static void sem_freeary(int id)
{
	struct sem_array *sma;
	struct sem_undo *un;

	/* remove semaphores array */
	sma = sem_rmid(id);

	/* clear undos */
	for (un = sma->undo; un; un = un->id_next)
	        un->semadj = 0;

	/* wake up processes */
	if (sma->sem_wait)
		wake_up(&sma->sem_wait);

	/* update global stats */
	used_sems -= sma->sem_nsems;

	/* free semaphores array */
	kfree(sma);
}

/*
 * Exit a process.
 */
void sem_exit(void)
{
	struct sem_undo *u, *un = NULL, **up, **unp;
	struct sem_array *sma;
	struct sem *sem;
	size_t i;

	/* for each undo request */
	for (up = &current_task->semundo; (u = *up); *up = u->proc_next, kfree(u)) {
		if (u->semid == -1)
			continue;

		/* get semaphores array */
		sma = (struct sem_array *) ipc_get(&sem_ids, u->semid);
		if (!sma)
			continue;

		/* check semaphores array */
		if (sem_checkid(sma, u->semid))
			continue;

		/* remove u from the sma->undo list */
		for (unp = &sma->undo; (un = *unp); unp = &un->id_next)
			if (u == un)
				goto found;

		printf("sem_exit: undo list error id=%d\n", u->semid);
		continue;
found:
		*unp = un->id_next;

		/* perform adjustments registered in u */
		for (i = 0; i < sma->sem_nsems; i++) {
			sem = &sma->sem_base[i];

			sem->sempid = current_task->pid;
			sem->semval += u->semadj[i];
			if (sem->semval < 0)
				sem->semval = 0;
		}

		/* update operation time */
		sma->sem_otime = CURRENT_TIME;

		/* wake up sleepers */
		if (sma->sem_wait)
			wake_up(&sma->sem_wait);
	}

	/* reset undo */
	current_task->semundo = NULL;
}

/*
 * Get a semaphores array.
 */
int sys_semget(key_t key, int nsems, int semflg)
{
	struct sem_array *sma;
	int id;

	/* check number of semaphores */
	if (nsems < 0  || nsems > SEMMSL)
		return -EINVAL;

	/* private resource : create a new semaphores array */
	if (key == IPC_PRIVATE)
		return sem_newary(key, nsems, semflg);

	/* find semaphores array */
	id = ipc_findkey(&sem_ids, key);

	/* create semaphores array if needed */
	if (id == -1) {
		if (!(semflg & IPC_CREAT))
			return -ENOENT;

		return sem_newary(key, nsems, semflg);
	}

	/* exclusive access */
	if ((semflg & IPC_CREAT) && (semflg & IPC_EXCL))
		return -EEXIST;

	/* get semaphores array queue */
	sma = (struct sem_array *) ipc_get(&sem_ids, id);

	/* check permissions */
	if (ipcperms(&sma->sem_perm, semflg))
		return -EACCES;
	if (nsems > (int) sma->sem_nsems)
		return -EINVAL;

	/* build message id */
	return sem_buildid(id, sma->sem_perm.seq);
}

/*
 * Free an undo requests structure.
 */
static struct sem_undo *sem_freeundos(struct sem_undo *un)
{
	struct sem_undo *u, **up;

	for (up = &current_task->semundo; (u = *up) ;up = &u->proc_next) {
		if (un != u)
			continue;

		/* remove request */
		un = u->proc_next;
		*up = un;

		/* free request */
		kfree(u);
		return un;
	}

	printf("freeundos: undo list error id=%d\n", un->semid);
	return un->proc_next;
}

/*
 * Allocate an undo requests structure.
 */
static int sem_allocundos(struct sem_array *sma, struct sem_undo **unp, int semid)
{
	struct sem_undo *un;
	size_t size;

	/* allocate undo requests structure */
	size = sizeof(struct sem_undo) + sizeof(short) * sma->sem_nsems;
	un = (struct sem_undo *) kmalloc(size);
	if (!un)
		return -ENOMEM;

	/* set undo requests structure */
	memset(un, 0, size);
	un->semadj = (short *) &un[1];
	un->semid = semid;
	un->proc_next = current_task->semundo;
	current_task->semundo = un;
	un->id_next = sma->undo;
	sma->undo = un;
	*unp = un;

	return 0;
}

/*
 * Try to execute operations.
 */
static int try_atomic_semop(struct sem_array *sma, struct sembuf *sops, size_t nsops, struct sem_undo *un, pid_t pid, int do_undo)
{
	struct sembuf *sop;
	struct sem *curr;
	int res, undo;

	/* for each operation */
	for (sop = sops; sop < sops + nsops; sop++) {
		/* get semaphore */
		curr = sma->sem_base + sop->sem_num;

		/* compute result */
		res = curr->semval + sop->sem_op;

		/* check if operation would block */
		if ((!sop->sem_op && curr->semval) || res < 0)
			goto would_block;

		/* exceeding the undo range is an error */
		if (sop->sem_flg & SEM_UNDO) {
			undo = un->semadj[sop->sem_num] - sop->sem_op;
			if (undo < (-SEMAEM - 1) || undo > SEMAEM)
				goto out_of_range;
		}

		/* ok : set semaphore */
		curr->semval = res;
	}

	/* undo ? */
	if (do_undo) {
		res = 0;
		goto undo;
	}

	/* update adjust values */
	sop--;
	for (; sop >= sops; sop--) {
		sma->sem_base[sop->sem_num].sempid = pid;
		if (sop->sem_flg & SEM_UNDO)
			un->semadj[sop->sem_num] -= sop->sem_op;
	}

	/* update operation time */
	sma->sem_otime = CURRENT_TIME;
	return 0;
out_of_range:
	res = -ERANGE;
	goto undo;
would_block:
	res = sop->sem_flg & IPC_NOWAIT ? -EAGAIN : 1;
undo:
	sop--;
	for (; sop >= sops; sop--)
		sma->sem_base[sop->sem_num].semval -= sop->sem_op;
	return res;
}

/*
 * Semaphore operation.
 */
int sys_semop(int semid, struct sembuf *sops, size_t nsops)
{
	struct sem_undo *un = NULL;
	int undos = 0, alter = 0;
	struct sem_array *sma;
	struct sembuf *sop;
	int ret;

	/* check parameters */
	if (nsops < 1 || semid < 0)
		return -EINVAL;
	if (nsops > SEMOPM)
		return -E2BIG;
	if (!sops)
		return -EFAULT;

	/* get semaphores array */
	sma = (struct sem_array *) ipc_get(&sem_ids, semid);
	if (!sma)
		return -EINVAL;

	/* check semaphores array id */
	if (sem_checkid(sma, semid))
		return -EIDRM;

	/* check operations */
	for (sop = sops; sop < sops + nsops; sop++) {
		if (sop->sem_num >= sma->sem_nsems)
			return -EFBIG;
		if (sop->sem_flg & SEM_UNDO)
			undos++;
		if (sop->sem_op)
			alter++;
	}

	/* check permissions */
	if (ipcperms(&sma->sem_perm, alter ? S_IWUGO : S_IRUGO))
		return -EACCES;

	/* make sure we have an undo structure for this process and this semaphore set */
	if (undos) {
		un = current_task->semundo;
		while (un) {
			if (un->semid == semid)
				break;
			if (un->semid == -1)
				un = sem_freeundos(un);
			else
				un = un->proc_next;
		}

		if (!un) {
			ret = sem_allocundos(sma, &un, semid);
			if (ret)
				return ret;
		}
	}

	/* try to execute operations */
	ret = try_atomic_semop(sma, sops, nsops, un, current_task->pid, 0);
	if (ret <= 0)
		goto update;

	/* we need to wait */
	for (;;) {
		/* handle signal */
		if (signal_pending(current_task))
			return -EINTR;

		/* sleep on semaphore */
		sleep_on(&sma->sem_wait);

		/* get semaphores array */
		sma = (struct sem_array *) ipc_get(&sem_ids, semid);
		if (!sma)
			return -EIDRM;

		/* check semaphores array id */
		if (sem_checkid(sma, semid))
			return -EIDRM;

		/* try to execute operations */
		ret = try_atomic_semop(sma, sops, nsops, un, current_task->pid, 0);
		if (ret <= 0)
			break;
	}

update:
	/* wake up processes */
       	if (sma->sem_wait)
		wake_up(&sma->sem_wait);

	return 0;
}

/*
 * Control a semaphores array.
 */
static int semctl_main(int semid, int cmd, union semun arg)
{
	struct sem_array *sma;
	struct sem_undo *un;
	size_t i;

	/* get semaphores array */
	sma = (struct sem_array *) ipc_get(&sem_ids, semid);
	if (!sma)
		return -EINVAL;

	/* check semaphores array */
	if (sem_checkid(sma, semid))
		return -EIDRM;

	/* check permissions */
	if (ipcperms(&sma->sem_perm, (cmd == SETVAL || cmd == SETALL) ? S_IWUGO:S_IRUGO))
		return -EACCES;

	switch (cmd) {
		case GETALL:
			/* get values */
		 	for (i = 0; i < sma->sem_nsems; i++)
				arg.array[i] = sma->sem_base[i].semval;

			break;
		case SETALL:
			/* check values */
		 	for (i = 0; i < sma->sem_nsems; i++)
			 	if (arg.array[i] > SEMVMX)
					return -ERANGE;

			/* set values */
		 	for (i = 0; i < sma->sem_nsems; i++)
				sma->sem_base[i].semval = arg.array[i];

			/* reset undos */
			for (un = sma->undo; un; un = un->id_next)
				un->semadj = 0;

			/* wake up processes */
			if (sma->sem_wait)
				wake_up(&sma->sem_wait);

			/* update change time */
			sma->sem_ctime = CURRENT_TIME;
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

/*
 * Get status of a semaphores array.
 */
static int semctl_stat(int semid, int cmd, struct semid_ds *out)
{
	struct sem_array *sma;

	/* clear output */
	memset(out, 0, sizeof(struct semid_ds));

	/* get semaphores array */
	sma = (struct sem_array *) ipc_get(&sem_ids, semid);
	if (!sma)
		return -EINVAL;

	/* check semaphores array id */
	if (cmd != SEM_STAT && sem_checkid(sma, semid))
		return -EIDRM;

	/* get status */
	kernel_to_ipc_perm(&sma->sem_perm, &out->sem_perm);
	out->sem_otime = sma->sem_otime;
	out->sem_ctime = sma->sem_ctime;
	out->sem_nsems = sma->sem_nsems;

	return cmd == IPC_STAT ? 0 : sem_buildid(semid, sma->sem_perm.seq);
}

/*
 * Remove a semaphores array.
 */
static int semctl_rmid(int semid)
{
	struct sem_array *sma;

	/* get semaphores array */
	sma = (struct sem_array *) ipc_get(&sem_ids, semid);
	if (!sma)
		return -EINVAL;

	/* check semaphores id */
	if (sem_checkid(sma, semid))
		return -EIDRM;

	/* check permissions */
	if (current_task->euid != sma->sem_perm.cuid &&  current_task->euid != sma->sem_perm.uid)
	    return -EPERM;

	/* free semaphores array */
	sem_freeary(semid);

	return 0;
}

/*
 * Get informations of a semaphores array.
 */
static int semctl_info(int cmd, struct seminfo *p)
{
	/* clear output */
	memset(p, 0, sizeof(struct seminfo));

	p->semmni = SEMMNI;
	p->semmns = SEMMNS;
	p->semmsl = SEMMSL;
	p->semopm = SEMOPM;
	p->semvmx = SEMVMX;
	p->semmnu = SEMMNU;
	p->semmap = SEMMAP;
	p->semume = SEMUME;

	if (cmd == SEM_INFO) {
		p->semusz = sem_ids.in_use;
		p->semaem = used_sems;
	} else {
		p->semusz = SEMUSZ;
		p->semaem = SEMAEM;
	}

	return sem_ids.max_id < 0 ? 0: sem_ids.max_id;
}

/*
 * Control a semaphores array.
 */
int sys_semctl(int semid, int semnum, int cmd, void *buf)
{
	union semun semun_arg;

	UNUSED(semnum);

	/* parse version */
	ipc_parse_version(&cmd);

	/* handle command */
	switch (cmd) {
		case IPC_INFO:
		case SEM_INFO:
			return semctl_info(cmd, (struct seminfo *) buf);
		case IPC_STAT:
		case SEM_STAT:
			return semctl_stat(semid, cmd, *((struct semid_ds **) buf));
		case GETALL:
		case SETALL:
			memcpy(&semun_arg, buf, sizeof(union semun));
			return semctl_main(semid, cmd, semun_arg);
		case IPC_RMID:
			return semctl_rmid(semid);
		default:
			printf("sys_semctl: unknown command %d\n", cmd);
			return -EINVAL;
	}

	return 0;
}

/*
 * Init IPC semaphores.
 */
void init_sem()
{
	ipc_init_ids(&sem_ids, SEMMNI);
}