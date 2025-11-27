#include <ipc/shm.h>
#include <proc/sched.h>
#include <fs/tmp_fs.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>

#define shm_buildid(id, seq)		ipc_buildid(&shm_ids, id, seq)
#define shm_checkid(shp, shmid)		ipc_checkid(&shm_ids, &shp->shm_perm, shmid)
#define shm_rmid(id)			((struct shmid_kernel *) ipc_rmid(&shm_ids, id))

/*
 * Shared memory segment.
 */
struct shmid_kernel {
	struct kern_ipc_perm		shm_perm;
	struct file *			shm_file;
	int				id;
	unsigned long			shm_nattch;
	unsigned long			shm_segsz;
	time_t				shm_atim;
	time_t				shm_dtim;
	time_t				shm_ctim;
	pid_t				shm_cprid;
	pid_t				shm_lprid;
};

/* global variables */
static struct ipc_ids shm_ids;
static int shm_tot = 0;

/*
 * Destroy a shared memory segment.
 */
static void shm_destroy(struct shmid_kernel *shp)
{
	/* remove segment queue */
	shm_rmid(shp->id);

	/* release file */
	fput(shp->shm_file);

	/* update global stats */
	shm_tot -= (shp->shm_segsz + PAGE_SIZE - 1) >> PAGE_SHIFT;

	/* free segment */
	kfree(shp);
}

/*
 * Increment a shared memory segment.
 */
static void shm_inc(int id)
{
	struct shmid_kernel *shp;

	/* get segment */
	shp = (struct shmid_kernel *) ipc_get(&shm_ids, id);
	if (!shp)
		return;

	/* update segment */
	shp->shm_atim = CURRENT_TIME;
	shp->shm_lprid = current_task->pid;
	shp->shm_nattch++;
}

/*
 * Open a memory region.
 */
void shm_open(struct vm_area *vma)
{
	shm_inc(vma->vm_file->f_dentry->d_inode->u.tmp_i.i_shmid);
}

/*
 * Close a memory region.
 */
void shm_close(struct vm_area *vma)
{
	struct file *filp = vma->vm_file;
	struct shmid_kernel *shp;

	/* get segment */
	shp = (struct shmid_kernel *) ipc_get(&shm_ids, filp->f_dentry->d_inode->u.tmp_i.i_shmid);
	if (!shp)
		return;

	/* update segment */
	shp->shm_dtim = CURRENT_TIME;
	shp->shm_lprid = current_task->pid;
	shp->shm_nattch--;

	/* destroy segment if needed */
	if (shp->shm_nattch == 0 && shp->shm_perm.mode & SHM_DEST)
		shm_destroy(shp);
}

/*
 * Handle a shared memory page fault.
 */
static struct page *shm_nopage(struct vm_area *vma, uint32_t address)
{
	struct inode *inode = vma->vm_file->f_dentry->d_inode;
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
 * Shared memory mmap.
 */
static int shm_mmap(struct file *filp, struct vm_area *vma)
{
	struct inode *inode = filp->f_dentry->d_inode;

	/* update inode */
	update_atime(inode);
	vma->vm_ops = &shm_vm_ops;

	/* update shared segment */
	shm_inc(inode->u.tmp_i.i_shmid);

	return 0;
}

/*
 * Shared memory file operations.
 */
static struct file_operations shm_fops = {
	.mmap		= shm_mmap,
};

/*
 * Get a new shared memory inode.
 */
static struct inode *shm_get_inode(struct super_block *sb, mode_t mode, dev_t dev, size_t size)
{
	struct inode *inode;

	/* create an inode */
	inode = tmpfs_new_inode(sb, mode, dev);
	if (!inode)
		return NULL;

	/* set inode */
	inode->i_shm = 1;
	inode->i_size = size;

	return inode;
}

/*
 * Set up a temporary file.
 */
static struct file *shm_file_setup(const char *name, size_t size)
{
	struct dentry *d_root, *dentry;
	struct inode *inode;
	struct file *filp;
	struct qstr this;
	int ret;

	/* get tmpfs root */
	d_root = namei(AT_FDCWD, "/tmp", 0);
	if (IS_ERR(d_root))
		return (void *) d_root;

	/* allocate a new dentry */
	ret = -ENOMEM;
	this.name = (char *) name;
	this.len = strlen(name);
	this.hash = 0;
	dentry = d_alloc(d_root, &this);
	if (!dentry)
		goto err;

	/* get an empty file */
	ret = -ENFILE;
	filp = get_empty_filp();
	if (!filp)
		goto err_dput;

	/* get an inode */
	ret = -ENOSPC;
	inode = shm_get_inode(d_root->d_sb, S_IFREG | S_IRWXUGO, 0, size);
	if (!inode)
		goto err_fput;

	/* instantiate dentry */
	d_instantiate(dentry, inode);

	/* set file */
	filp->f_dentry = dentry;
	filp->f_mode = FMODE_READ | FMODE_WRITE;
	filp->f_op = &shm_fops;

	return filp;
err_fput:
	fput(filp);
err_dput:
	dput(dentry);
err:
	dput(d_root);
	return ERR_PTR(ret);
}

/*
 * Create a new shared memory segment.
 */
static int shm_newseg(key_t key, int shmflg, size_t size)
{
	size_t npages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;
	struct shmid_kernel *shp;
	struct file *filp;
	char name[32];
	int id;

	/* check size */
	if (size < SHMMIN || size > SHMMAX)
		return -EINVAL;
	if (shm_tot + npages >= SHMALL)
		return -ENOSPC;

	/* allocate segment */
	shp = (struct shmid_kernel *) kmalloc(sizeof(struct shmid_kernel));
	if (!shp)
		return -ENOMEM;

	/* setup segment */
	sprintf(name, "SYSV%08x", key);
	filp = shm_file_setup(name, size);
	if (IS_ERR(filp)) {
		kfree(shp);
		return PTR_ERR(filp);
	}

	/* register segment */
	id = ipc_addid(&shm_ids, &shp->shm_perm, SHMMNI);
	if (id == -1) {
		fput(filp);
		kfree(shp);
		return -ENOSPC;
	}

	/* set segment */
	shp->shm_perm.key = key;
	shp->shm_perm.mode = (shmflg & S_IRWXUGO);
	shp->shm_file = filp;
	shp->id = shm_buildid(id, shp->shm_perm.seq);
	shp->shm_nattch = 0;
	shp->shm_segsz = size;
	shp->shm_atim = 0;
	shp->shm_dtim = 0;
	shp->shm_ctim = CURRENT_TIME;
	shp->shm_cprid = current_task->pid;
	shp->shm_lprid = 0;

	/* attach segment id to inode */
	filp->f_dentry->d_inode->u.tmp_i.i_shmid = shp->id;

	/* update global stats */
	shm_tot += npages;

	return shp->id;
}

/*
 * Get a shared memory segment.
 */
int sys_shmget(key_t key, size_t size, int shmflg)
{
	struct shmid_kernel *shp;
	int id;

	/* private resource : create a new semaphores array */
	if (key == IPC_PRIVATE)
	 	return shm_newseg(key, shmflg, size);

	/* find shared memory segment */
	id = ipc_findkey(&shm_ids, key);

	/* create shared memory segment if needed */
	if (id == -1) {
		if (!(shmflg & IPC_CREAT))
			return -ENOENT;

	 	return shm_newseg(key, shmflg, size);
	}

	/* exclusive access */
	if ((shmflg & IPC_CREAT) && (shmflg & IPC_EXCL))
		return -EEXIST;

	/* get shared memory segment */
	shp = (struct shmid_kernel *) ipc_get(&shm_ids, id);

	/* check permissions */
	if (shp->shm_segsz < size)
		return -EINVAL;
	if (ipcperms(&shp->shm_perm, shmflg))
		return -EACCES;

	/* build segment id */
	return shm_buildid(id, shp->shm_perm.seq);
}

/*
 * Attach a shared memory segment to current process.
 */
int sys_shmat(int shmid, char *shmaddr, int shmflg, uint32_t *addr_ret)
{
	uint32_t addr, flags, prot, buf;
	struct shmid_kernel *shp;
	struct file *filp;
	size_t size;

	/* check shared memory id */
	if (shmid < 0)
		return -EINVAL;

	/* get shared segment */
	shp = (struct shmid_kernel *) ipc_get(&shm_ids, shmid);
	if (!shp)
		return -EINVAL;

	/* check segment id */
	if (shm_checkid(shp, shmid))
		return -EIDRM;

	/* check permissions */
	if (ipcperms(&shp->shm_perm, S_IWUGO))
		return -EACCES;

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

	/* get file */
	filp = shp->shm_file;
	size = filp->f_dentry->d_inode->i_size;

	/* check memory region intersection */
	if (addr && !(shmflg & SHM_REMAP))
		if (find_vma_intersection(current_task, addr, addr + size))
			return -EINVAL;

	/* do mmap */
	buf = do_mmap(shp->shm_file, addr, size, prot, flags, 0);
	if (!buf)
		goto out;

	/* set return address */
	*addr_ret = (uint32_t) buf;
out:
	/* destroy memory segment ? */
	if (shp->shm_nattch == 0 && shp->shm_perm.mode & SHM_DEST)
		shm_destroy(shp);

	return 0;
}

/*
 * Detach a shared memory segment from a process.
 */
int sys_shmdt(char *shmaddr)
{
	struct vm_area *vma;

	vma = find_vma(current_task, (uint32_t) shmaddr);
	if (vma && vma->vm_ops == &shm_vm_ops)
		do_munmap(vma->vm_start, vma->vm_end - vma->vm_start);

	return 0;
}

/*
 * Get informations of shared memory segments.
 */
static int shmctl_ipcinfo(struct shminfo *p)
{
	/* clear output */
	memset(p, 0, sizeof(struct shminfo));

	p->shmmax = SHMMAX;
	p->shmmin = SHMMIN;
	p->shmmni = SHMMNI;
	p->shmseg = SHMSEG;
	p->shmall = SHMALL;

	return shm_ids.max_id < 0 ? 0: shm_ids.max_id;
}

/*
 * Get informations of shared memory segments.
 */
static int shmctl_shminfo(struct shm_info *p)
{
	/* clear output */
	memset(p, 0, sizeof(struct shm_info));

	p->used_ids = shm_ids.in_use;
	p->shm_tot = shm_tot;

	return shm_ids.max_id < 0 ? 0: shm_ids.max_id;
}

/*
 * Get status of a shared memory segment.
 */
static int shmctl_stat(int shmid, int cmd, struct shmid_ds *out)
{
	struct shmid_kernel *shp;

	/* clear output */
	memset(out, 0, sizeof(struct shmid_ds));

	/* get shared memory segment */
	shp = (struct shmid_kernel *) ipc_get(&shm_ids, shmid);
	if (!shp)
		return -EINVAL;

	/* check segment id */
	if (cmd != SHM_STAT && shm_checkid(shp, shmid))
		return -EIDRM;

	/* get status */
	kernel_to_ipc_perm(&shp->shm_perm, &out->shm_perm);
	out->shm_segsz = shp->shm_segsz;
	out->shm_atime = shp->shm_atim;
	out->shm_dtime = shp->shm_dtim;
	out->shm_ctime = shp->shm_ctim;
	out->shm_cpid = shp->shm_cprid;
	out->shm_lpid = shp->shm_lprid;
	out->shm_nattch = shp->shm_nattch;

	return cmd == IPC_STAT ? 0 : shm_buildid(shmid, shp->shm_perm.seq);
}

/*
 * Remove a shared memory segment queue.
 */
static int shmctl_rmid(int shmid)
{
	struct shmid_kernel *shp;

	/* get shared memory segment */
	shp = (struct shmid_kernel *) ipc_get(&shm_ids, shmid);
	if (!shp)
		return -EINVAL;

	/* check segment id */
	if (shm_checkid(shp, shmid))
		return -EIDRM;

	/* check permissions */
	if (current_task->euid != shp->shm_perm.cuid &&  current_task->euid != shp->shm_perm.uid)
	    return -EPERM;

	/* free segment */
	if (shp->shm_nattch) {
		shp->shm_perm.mode |= SHM_DEST;
		shp->shm_perm.key = IPC_PRIVATE;
	} else {
		shm_destroy(shp);
	}

	return 0;
}

/*
 * Control a shared memory segment.
 */
int sys_shmctl(int shmid, int cmd, struct shmid_ds *buf)
{
	/* check parameters */
	if (cmd < 0 || shmid < 0)
		return -EINVAL;

	/* parse version */
	ipc_parse_version(&cmd);

	/* handle command */
	switch (cmd) {
		case IPC_INFO:
			return shmctl_ipcinfo((struct shminfo *) buf);
		case SHM_INFO:
			return shmctl_shminfo((struct shm_info *) buf);
		case IPC_STAT:
		case SHM_STAT:
		 	return shmctl_stat(shmid, cmd, (struct shmid_ds *) buf);
		case IPC_RMID:
			return shmctl_rmid(shmid);
		default:
			printf("sys_shmctl: unknown command %d\n", cmd);
			return -ENOSYS;
	}
}

/*
 * Init IPC shared memory segments.
 */
void init_shm()
{
	ipc_init_ids(&shm_ids, SHMMNI);
}