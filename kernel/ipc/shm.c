#include <ipc/shm.h>
#include <fs/tmp_fs.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>

#define shm_get(id)		((struct shmid *) ipc_get(id))

static struct file_operations shm_file_operations;

/*
 * Get a new shared memory inode.
 */
static struct inode *shm_get_inode(mode_t mode, dev_t dev, int size)
{
	struct inode *tmpfs_root_inode, *inode;
	int ret;

	/* get tmpfs root inode */
	ret = namei(AT_FDCWD, NULL, "/tmp", 0, &tmpfs_root_inode);
	if (ret)
		return NULL;

	/* get a new tmpfs inode */
	inode = tmpfs_new_inode(tmpfs_root_inode->i_sb, mode, dev);
	if (inode) {
		inode->i_shm = 1;
		inode->i_size = size;
	}

	/* release tmpfs root inode */
	iput(tmpfs_root_inode);

	return inode;
}

/*
 * Set up a temporary file.
 */
static struct file *shmem_file_setup(int size)
{
	struct inode *inode;
	struct file *filp;

	/* get an empty file */
	filp = get_empty_filp();
	if (!filp)
		return NULL;

	/* get an inode */
	inode = shm_get_inode(S_IFREG | S_IRWXUGO, 0, size);
	if (!inode) {
		do_close(filp);
		return NULL;
	}

	/* set file */
	filp->f_inode = inode;
	filp->f_mode = FMODE_READ | FMODE_WRITE;
	filp->f_op = &shm_file_operations;

	return filp;
}

/*
 * Create a new shared memory segment.
 */
static int shm_newseg(int key, int size, int shmflg)
{
	int id, ret = -EINVAL;
	struct file *filp;
	struct shmid *shm;

	/* check size */
	if (size < SHMMIN || size > SHMMAX)
		return -EINVAL;

	/* allocate a new shared memory segment */
	shm = (struct shmid *) kmalloc(sizeof(struct shmid));
	if (!shm)
		return -ENOMEM;

	/* get a new inode */
	filp = shmem_file_setup(size);
	if (!filp)
		goto no_file;

	/* add ipc element */
	id = ipc_add_id(&shm->shm_perm);
	if (id == -1) {
		ret = -ENOSPC;
		goto no_id;
	}

	/* set shared memory segment */
	shm->shm_perm.key = key;
	shm->shm_perm.mode = (shmflg & S_IRWXUGO);
	shm->shm_id = ipc_build_id(id, shm->shm_perm.seq);
	shm->shm_filp = filp;
	shm->shm_size = size;
	shm->shm_nattch = 0;
	shm->shm_atime = 0;
	shm->shm_dtime = 0;
	shm->shm_ctime = CURRENT_TIME;
	shm->shm_cprid = current_task->pid;
	shm->shm_lprid = 0;

	/* attach segment id to inode */
	shm->shm_filp->f_inode->u.tmp_i.i_shmid = shm->shm_id;

	return shm->shm_id;
no_id:
	filp->f_inode->i_nlinks = 0;
	do_close(filp);
no_file:
	kfree(shm);
	return ret;
}

/*
 * Destroy a memory segment.
 */
int shm_destroy(struct shmid *shm)
{
	/* remove ipc resource */
	ipc_rm_id(shm->shm_id);

	/* remove file */
	shm->shm_filp->f_inode->i_nlinks = 0;
	do_close(shm->shm_filp);

	/* free segment */
	kfree(shm);
	return 0;
}

/*
 * Get a shared memory segment.
 */
int do_shmget(int key, int size, int shmflg)
{
	struct shmid *shm;
	int id;

	/* private shm : create a new segment */
	if (key == IPC_PRIVATE)
		return shm_newseg(key, size, shmflg);

	/* else try to find key */
	id = ipc_findkey(key);
	if (id == -1) {
		if (!(shmflg & IPC_CREAT))
			return -ENOENT;

		return shm_newseg(key, size, shmflg);
	}

	/* exclusive shm */
	if ((shmflg & IPC_CREAT) && (shmflg & IPC_EXCL))
		return -EEXIST;

	/* get segment */
	shm = shm_get(id);
	if (!shm)
		return -ENOSPC;

	/* check size */
	if (shm->shm_size < size)
		return -EINVAL;

	return ipc_build_id(id, shm->shm_perm.seq);
}

/*
 * Attach a shared memory segment to current process.
 */
int do_shmat(int shmid, char *shmaddr, int shmflg, uint32_t *addr_ret)
{
	uint32_t addr, flags, prot;
	struct shmid *shm;
	void *buf;

	/* check shared memory id */
	if (shmid < 0)
		return -EINVAL;

	/* get shared memory segment */
	shm = shm_get(shmid);
	if (!shm)
		return -EINVAL;

	/* compute mapping address */
	addr = (uint32_t) shmaddr;
	if (addr) {
		if (addr & (SHMLBA - 1)) {
			if (shmflg & SHM_RND)
				addr &= ~(SHMLBA - 1);
			else
				return -EINVAL;
		}

		flags = MAP_SHARED | MAP_FIXED;
	} else {
		if ((shmflg & SHM_REMAP))
			return -EINVAL;

		flags = MAP_SHARED;
	}

	/* compute flags */
	if (shmflg & SHM_RDONLY)
		prot = PROT_READ;
	else
		prot = PROT_READ | PROT_WRITE;

	/* check memory region intersection */
	if (addr && !(shmflg & SHM_REMAP))
		if (find_vma_intersection(current_task, addr, addr + shm->shm_size))
			return -EINVAL;

	/* do mmap */
	buf = do_mmap(addr, shm->shm_size, prot, flags, shm->shm_filp, 0);
	if (!buf)
		goto out;

	/* set return address */
	*addr_ret = (uint32_t) buf;
out:
	/* destroy memory segment ? */
	if (shm->shm_nattch == 0 && shm->shm_perm.mode & SHM_DEST)
		shm_destroy(shm);

	return 0;
}

/*
 * Control a shared memory segment.
 */
int do_shmctl(int shmid, int cmd, struct shmid_ds *buf)
{
	struct shmid *shm;

	if (cmd < 0 || shmid < 0)
		return -EINVAL;

	/* get segment */
	shm = shm_get(shmid);
	if (!shm)
		return -EINVAL;

	/* 64 bits command */
	cmd ^= 0x0100;
	switch (cmd) {
		case IPC_STAT:
			/* set output buffer */
			memset(&buf, 0, sizeof(buf));
			buf->shm_segsz = shm->shm_size;
			buf->shm_cpid = shm->shm_cprid;
			buf->shm_lpid = shm->shm_lprid;
			buf->shm_nattch = shm->shm_nattch;
			buf->shm_atime = shm->shm_atime;
			buf->shm_dtime = shm->shm_dtime;
			buf->shm_ctime = shm->shm_ctime;
			return 0;
		case IPC_RMID:
		  	if (shm->shm_nattch) {
				shm->shm_perm.mode |= SHM_DEST;
				shm->shm_perm.key = IPC_PRIVATE;
			} else {
				shm_destroy(shm);
			}

			return 0;
		default:
			printf("shmctl : unknown command %d\n", cmd);
			return -ENOSYS;
	}
}

/*
 * Open a memory region.
 */
void shm_open(struct vm_area *vma)
{
	struct shmid *shm = shm_get(vma->vm_inode->u.tmp_i.i_shmid);

	shm->shm_atime = CURRENT_TIME;
	shm->shm_lprid = current_task->pid;
	shm->shm_nattch++;
}

/*
 * Close a memory region.
 */
void shm_close(struct vm_area *vma)
{
	struct shmid *shm = shm_get(vma->vm_inode->u.tmp_i.i_shmid);

	shm->shm_dtime = CURRENT_TIME;
	shm->shm_lprid = current_task->pid;
	shm->shm_nattch--;

	if (shm->shm_nattch == 0 && shm->shm_perm.mode & SHM_DEST)
		shm_destroy(shm);
}

/*
 * Handle a shared memory page fault.
 */
static struct page *shm_nopage(struct vm_area *vma, uint32_t address)
{
	struct inode *inode = vma->vm_inode;
	uint32_t new_page, offset;
	struct page *page;

	/* page align address */
	address = PAGE_ALIGN_DOWN(address);

	/* compute offset */
	offset = address - vma->vm_start + vma->vm_offset;
	if (offset >= inode->i_size)
		return NULL;

	/* try to get page from cache */
	page = find_page(inode, offset);
	if (page)
		return page;

	/* else get a new page */
	new_page = (uint32_t) get_free_page();
	if (!new_page)
		return NULL;

	/* memzero page */
	memset((void *) new_page, 0, PAGE_SIZE);

	/* get page and add it to cache */
	page = &page_array[MAP_NR(new_page)];
	add_to_page_cache(page, inode, offset);

	return page;
}

/*
 * Shared memory page fault handler.
 */
static struct vm_operations shm_vm_ops = {
	.open		= shm_open,
	.close		= shm_close,
	.nopage		= shm_nopage,
}; 

/*
 * Detach a shared memory segment from a process.
 */
int do_shmdt(char *shmaddr)
{
	struct vm_area *vma;

	vma = find_vma(current_task, (uint32_t) shmaddr);
	if (vma && vma->vm_ops == &shm_vm_ops)
		do_munmap(vma->vm_start, vma->vm_end - vma->vm_start);

	return 0;
}

/*
 * Shared memory mmap.
 */
static int shm_mmap(struct inode *inode, struct vm_area *vma)
{
	struct shmid *shm = shm_get(inode->u.tmp_i.i_shmid);

	if (!S_ISREG(inode->i_mode))
		return -EACCES;

	/* increment number of attaches */
	shm->shm_nattch++;

	/* update inode */
	inode->i_atime = CURRENT_TIME;
	inode->i_dirt = 1;
	inode->i_count++;

	/* set memory region */
	vma->vm_inode = inode;
	vma->vm_ops = &shm_vm_ops;

	return 0;
}

/*
 * Shared memory file operations.
 */
static struct file_operations shm_file_operations = {
	.mmap		= shm_mmap,
};
