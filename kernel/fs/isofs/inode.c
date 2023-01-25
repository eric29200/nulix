#include <fs/iso_fs.h>
#include <proc/sched.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * ISOFS file operations.
 */
struct file_operations_t isofs_file_fops = {
	.read		= isofs_file_read,
	.mmap		= generic_file_mmap,
};

/*
 * ISOFS directory operations.
 */
struct file_operations_t isofs_dir_fops = {
	.getdents64	= isofs_getdents64,
};

/*
 * ISOFS file inode operations.
 */
struct inode_operations_t isofs_file_iops = {
	.fops		= &isofs_file_fops,
	.bmap		= isofs_bmap,
	.readpage	= generic_readpage,
};

/*
 * ISOFS directory inode operations.
 */
struct inode_operations_t isofs_dir_iops = {
	.fops		= &isofs_dir_fops,
	.lookup		= isofs_lookup,
};

/*
 * Read a ISOFS inode.
 */
int isofs_read_inode(struct inode_t *inode)
{
	struct iso_directory_record_t *raw_inode;
	struct super_block_t *sb = inode->i_sb;
	int offset, frag1, err = 0;
	struct buffer_head_t *bh;
	void *cpnt = NULL;
	uint32_t block;
	uint8_t *pnt;

	/* read inode block */
	block = inode->i_ino >> sb->s_blocksize_bits;
	bh = bread(sb->s_dev, block, sb->s_blocksize);
	if (!bh)
		return -EIO;

	/* get raw inode (= directory record) */
	pnt = ((uint8_t *) bh->b_data + (inode->i_ino & (sb->s_blocksize - 1)));
	raw_inode = (struct iso_directory_record_t *) pnt;

	/* if raw inode is on 2 blocks, copy it in memory */
	if ((inode->i_ino & (sb->s_blocksize - 1)) + *pnt > sb->s_blocksize) {
		/* copy first fragment */
		offset = (inode->i_ino & (sb->s_blocksize - 1));
		frag1 = sb->s_blocksize - offset;

		/* allocate a buffer */
		cpnt = kmalloc(*pnt);
		if (!cpnt) {
			brelse(bh);
			return -ENOMEM;
		}

		/* copy 1st fragment */	
		memcpy(cpnt, bh->b_data + offset, frag1);

		/* read next block */
		brelse(bh);
		bh = bread(sb->s_dev, ++block, sb->s_blocksize);
		if (!bh) {
			kfree(cpnt);
			return -EIO;
		}

		/* copy 2nd fragment */
		offset += *pnt - sb->s_blocksize;
		memcpy((char *) cpnt + frag1, bh->b_data, offset);

		/* get full raw inode */
		pnt = (uint8_t *) cpnt;
		raw_inode = (struct iso_directory_record_t *) pnt;
	}

	/* set inode */
	inode->i_mode = S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH;
	inode->i_mode |= (raw_inode->flags[0] & 2) ? S_IFDIR : S_IFREG;
	inode->i_nlinks = 1;
	inode->i_uid = current_task->uid;
	inode->i_gid = current_task->gid;
	inode->i_size = isofs_num733(raw_inode->size);
	inode->i_blocks = 0;
	inode->i_atime = inode->i_mtime = inode->i_ctime = isofs_date(raw_inode->date);
	inode->u.iso_i.i_first_extent = (isofs_num733(raw_inode->extent) + isofs_num711(raw_inode->ext_attr_length)) << isofs_sb(sb)->s_log_zone_size;
	inode->u.iso_i.i_backlink = 0xFFFFFFFF;

	/* set operations */
	if (S_ISREG(inode->i_mode))
		inode->i_op = &isofs_file_iops;
	else if (S_ISDIR(inode->i_mode))
		inode->i_op = &isofs_dir_iops;
	else
		err = -EINVAL;

	/* release block buffer */
	brelse(bh);

	/* free buffer */
	if (cpnt)
		kfree(cpnt);

	return err;
}

/*
 * Get or create a buffer.
 */
struct buffer_head_t *isofs_getblk(struct inode_t *inode, int block, int create)
{
	struct super_block_t *sb = inode->i_sb;

	/* isofs is read only */
	if (create)
		return NULL;

	/* get block number */
	block = isofs_bmap(inode, block);
	if (block <= 0)
		return NULL;

	/* read block */
	return bread(sb->s_dev, block, sb->s_blocksize);
}

/*
 * Get a block number.
 */
int isofs_bmap(struct inode_t *inode, int block)
{
	if (block < 0)
		return 0;

	return (inode->u.iso_i.i_first_extent >> inode->i_sb->s_blocksize_bits) + block;
}
