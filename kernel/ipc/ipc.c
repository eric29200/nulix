#include <stdio.h>
#include <ipc/ipc.h>
#include <ipc/shm.h>
#include <mm/mm.h>
#include <proc/sched.h>
#include <stderr.h>

#define SEQ_MULTIPLIER		(IPCMNI)

static struct ipc_ids_t *ids = NULL;

/*
 * Find an IPC element with a given key. Returns IPC element id.
 */
int ipc_findkey(int key)
{
	struct ipc_perm_t *p;
	int id;

	if (!ids)
		return -1;

	for (id = 0; id <= ids->max_id; id++) {
		p = ids->entries[id].p;
		if (p && p->key == key)
			return id;
	}

	return -1;
}

/*
 * Init IPC elements.
 */
static int ipc_init_ids(int size)
{
	int id;

	/* allocate ipc elements */
	ids = (struct ipc_ids_t *) kmalloc(sizeof(struct ipc_ids_t));
	if (!ids)
		return -ENOMEM;

	/* set ipc elements */
	ids->size = size;
	ids->in_use = 0;
	ids->max_id = -1;
	ids->seq = 0;
	ids->seq_max = 0xFFFF;

	/* allocate entries */
	ids->entries = (struct ipc_id_t *) kmalloc(sizeof(struct ipc_id_t *) * size);
	if (!ids->entries)
		goto err;

	/* reset entries */
	for (id = 0; id < size; id++)
		ids->entries[id].p = NULL;

	return 0;
err:
	kfree(ids);
	ids = NULL;
	return -ENOMEM;
}

/*
 * Add an IPC element.
 */
int ipc_add_id(struct ipc_perm_t *new)
{
	int ret, id;

	/* init IPC elements */
	if (!ids) {
		ret = ipc_init_ids(IPCMNI);
		if (ret)
			return ret;
	}

	/* find a free id */
	for (id = 0; id < ids->size; id++)
		if (ids->entries[id].p == NULL)
			goto found;

	return -1;
found:
	/* update elements array */
	ids->in_use++;
	if (id > ids->max_id)
		ids->max_id = id;

	/* set element */
	new->cuid = current_task->euid;
	new->uid = current_task->euid;
	new->cgid = current_task->egid;
	new->gid = current_task->egid;
	new->seq = ids->seq++;

	/* reset ipc sequence if needed */
	if (ids->seq > ids->seq_max)
		ids->seq = 0;

	/* set IPC entry */
	ids->entries[id].p = new;

	return id;
}

/*
 * Remove an IPC element.
 */
struct ipc_perm_t *ipc_rm_id(int id)
{
	struct ipc_perm_t *p;
	int lid;

	/* check id */
	lid = id % SEQ_MULTIPLIER;
	if (lid >= ids->size)
		return NULL;

	/* remove element */
	p = ids->entries[lid].p;
	ids->entries[lid].p = NULL;
	ids->in_use--;

	/* update max id */
	if (lid == ids->max_id) {
		do {
			lid--;
			if (lid == -1)
				break;
		} while (ids->entries[lid].p == NULL);

		ids->max_id = lid;
	}

	return p;
}

/*
 * Build an IPC id.
 */
int ipc_build_id(int id, int seq)
{
	return SEQ_MULTIPLIER * seq + id;
}

/*
 * Get an IPC element given an id.
 */
struct ipc_perm_t *ipc_get(int id)
{
	if (id % SEQ_MULTIPLIER >= ids->size)
		return NULL;

	return ids->entries[id % SEQ_MULTIPLIER].p;
}

/*
 * IPC dispatcher system call.
 */
int sys_ipc(uint32_t call, int first, int second, int third, void *ptr, int fifth)
{
	uint32_t addr_ret;
	int ret;

	/* unused fifth argument */
	UNUSED(fifth);

	switch (call) {
		case SHMGET:
			return do_shmget(first, second, third);
		case SHMAT:
			ret = do_shmat(first, (char *) ptr, second, &addr_ret);
			if (ret)
				return ret;

			/* set return address */
			*((uint32_t *) third) = addr_ret;

			return 0;
		case SHMCTL:
			return do_shmctl(first, second, (struct shmid_ds_t *) ptr);
		case SHMDT:
			return do_shmdt((char *) ptr);
		default:
			printf("IPC system call %d not implemented\n", call);
			return -ENOSYS;
	}
}
