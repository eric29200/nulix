#include <ipc/ipc.h>
#include <ipc/msg.h>
#include <ipc/sem.h>
#include <ipc/shm.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Convert IPC permissions.
 */
void kernel_to_ipc_perm(struct kern_ipc_perm *in, struct ipc_perm *out)
{
	out->key = in->key;
	out->uid = in->uid;
	out->gid = in->gid;
	out->cuid = in->cuid;
	out->cgid = in->cgid;
	out->mode = in->mode;
	out->seq = in->seq;
}

/*
 * Parse version.
 */
int ipc_parse_version(int *cmd)
{
	if (*cmd & IPC_64) {
		*cmd ^= IPC_64;
		return IPC_64;
	}

	return IPC_OLD;
}

/*
 * Grow an IPC identifiers set.
 */
static int grow_ary(struct ipc_ids *ids, int newsize)
{
	struct ipc_id *new;
	int i;

	/* limit to max size */
	if (newsize > IPCMNI)
		newsize = IPCMNI;

	/* no need to grow */
	if (newsize <= ids->size)
		return newsize;

	/* allocate a new array */
	new = kmalloc(sizeof(struct ipc_id) * newsize);
	if (!new)
		return ids->size;

	/* init array */
	memcpy(new, ids->entries, sizeof(struct ipc_id) * ids->size);
	for (i = ids->size; i < newsize; i++)
		new[i].p = NULL;

	/* free old array */
	kfree(ids->entries);

	ids->entries = new;
	ids->size = newsize;

	return ids->size;
}

/*
 * Add an IPC identifier.
 */
int ipc_addid(struct ipc_ids *ids, struct kern_ipc_perm *new, int size)
{
	int id;

	/* grow identifiers set if needed */
	size = grow_ary(ids,size);

	/* find a free entry */
	for (id = 0; id < size; id++)
		if (ids->entries[id].p == NULL)
			goto found;

	return -1;
found:
	/* update max id */
	ids->in_use++;
	if (id > ids->max_id)
		ids->max_id = id;

	/* set new identifier */
	new->cuid = current_task->euid;
	new->uid = current_task->euid;
	new->gid = current_task->egid;
	new->cgid = current_task->egid;

	/* update sequence */
	new->seq = ids->seq++;
	if (ids->seq > ids->seq_max)
		ids->seq = 0;

	ids->entries[id].p = new;
	return id;
}

/*
 * Remove an IPC identifier.
 */
struct kern_ipc_perm *ipc_rmid(struct ipc_ids *ids, int id)
{
	int lid = id % SEQ_MULTIPLIER;
	struct kern_ipc_perm *p;

	/* check id */
	if (lid >= ids->size) {
		printf("ipc_rmid: wrong id %d\n", lid);
		return NULL;
	}

	/* remove resource */
	p = ids->entries[lid].p;
	ids->entries[lid].p = NULL;
	if (!p) {
		printf("ipc_rmid: null entry %d\n", lid);
		return NULL;
	}

	/* update identifier set size */
	ids->in_use--;

	/* update max id */
	if (lid == ids->max_id) {
		do {
			lid--;
			if (lid == -1)
				break;
		} while (ids->entries[lid].p);

		ids->max_id = lid;
	}

	return p;
}

/*
 * Build an IPC id.
 */
int ipc_buildid(struct ipc_ids *ids, int id, int seq)
{
	UNUSED(ids);
	return SEQ_MULTIPLIER * seq + id;
}

/*
 * Init an IPC identifiers set.
 */
void ipc_init_ids(struct ipc_ids *ids, int size)
{
	int i, seq_limit = INT_MAX / SEQ_MULTIPLIER;

	/* limit to max size */
	if (size > IPCMNI)
		size = IPCMNI;

	ids->size = size;
	ids->in_use = 0;
	ids->max_id = -1;
	ids->seq = 0;
	if (seq_limit > USHRT_MAX)
		ids->seq_max = USHRT_MAX;
	else
		ids->seq_max = seq_limit;

	/* allocate array */
	ids->entries = kmalloc(sizeof(struct ipc_id) * size);
	if (!ids->entries) {
		printf("ipc_init_ids: failed, ipc service disabled\n");
		ids->size = 0;
	}

	/* clear array */
	for (i = 0; i < ids->size; i++)
		ids->entries[i].p = NULL;
}

/*
 * Check IPC permissions.
 */
int ipcperms(struct kern_ipc_perm *ipcp, short flag)
{
	int requested_mode, granted_mode;

	requested_mode = (flag >> 6) | (flag >> 3) | flag;
	granted_mode = ipcp->mode;

	if (current_task->euid == ipcp->cuid || current_task->euid == ipcp->uid)
		granted_mode >>= 6;
	else if (task_in_group(current_task, ipcp->cgid) || task_in_group(current_task, ipcp->gid))
		granted_mode >>= 3;

	if (requested_mode & ~granted_mode & 0007)
		return -1;

	return 0;
}

/*
 * Get an IPC resource.
 */
struct kern_ipc_perm *ipc_get(struct ipc_ids *ids, int id)
{
	int lid = id % SEQ_MULTIPLIER;

	if (lid >= ids->size)
		return NULL;

	return ids->entries[lid].p;
}

/*
 * Check IPC identifier.
 */
int ipc_checkid(struct ipc_ids *ids, struct kern_ipc_perm *ipcp, int uid)
{
	UNUSED(ids);

	if (uid / SEQ_MULTIPLIER != (int) ipcp->seq)
		return 1;

	return 0;
}

/*
 * Find an IPC resource.
 */
int ipc_findkey(struct ipc_ids *ids, key_t key)
{
	struct kern_ipc_perm *p;
	int id;

	for (id = 0; id <= ids->max_id; id++) {
		p = ids->entries[id].p;

		if (p && key == p->key)
			return id;
	}

	return -1;
}

/*
 * IPC dispatcher system call.
 */
int sys_ipc(uint32_t call, int first, int second, int third, void *ptr, int fifth)
{
	int version = call >> 16, ret;
	struct ipc_kludge *tmp;
	uint32_t addr_ret;

	/* get call */
	call &= 0xFFFF;

	/* unused fifth argument */
	UNUSED(fifth);

	switch (call) {
		case MSGGET:
			return sys_msgget((key_t) first, second);
		case MSGSND:
			return sys_msgsnd(first, (struct msgbuf *) ptr, second, third);
		case MSGRCV:
			switch (version) {
				case 0:
					tmp = (struct ipc_kludge *) ptr;
					return sys_msgrcv(first, tmp->msgp, second, tmp->msgtyp, third);
				default:
					return sys_msgrcv(first, (struct msgbuf *) ptr, second, fifth, third);
			}
		case MSGCTL:
			return sys_msgctl(first, second, ptr);
		case SEMGET:
			return sys_semget(first, second, third);
		case SEMOP:
			return sys_semop(first, (struct sembuf *) ptr, second);
		case SEMCTL:
			return sys_semctl(first, second, third, ptr);
		case SHMGET:
			return sys_shmget(first, second, third);
		case SHMAT:
			ret = sys_shmat(first, ptr, second, &addr_ret);
			if (ret)
				return ret;

			*((uint32_t *) third) = addr_ret;
			return 0;
		case SHMDT:
			return sys_shmdt((char *) ptr);
		case SHMCTL:
			return sys_shmctl(first, second, (struct shmid_ds *) ptr);
		default:
			printf("IPC system call %d not implemented\n", call);
			return -ENOSYS;
	}
}

/*
 * Init IPC resources.
 */
void init_ipc()
{
	init_msg();
	init_sem();
	init_shm();
}
