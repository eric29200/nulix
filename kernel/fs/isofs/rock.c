#include <fs/iso_fs.h>
#include <stderr.h>
#include <stdio.h>
#include <string.h>

struct SU_SP {
	unsigned char		magic[2];
	unsigned char		skip;
};

struct SU_CE {
	char			extent[8];
	char			offset[8];
	char			size[8];
};

struct SU_ER {
	unsigned char		len_id;
	unsigned char		len_des;
	unsigned char		len_src;
	unsigned char		ext_ver;
	char			data[0];
};

struct RR_RR {
	char			flags[1];
};

struct RR_PX {
	char			mode[8];
	char			n_links[8];
	char			uid[8];
	char			gid[8];
};

struct RR_PN {
	char			dev_high[8];
	char			dev_low[8];
};


struct SL_component {
	unsigned char		flags;
	unsigned char		len;
	char			text[0];
};

struct RR_SL {
	unsigned char		flags;
	struct SL_component	link;
};

struct RR_NM {
	unsigned char		flags;
	char			name[0];
};

struct RR_CL {
	char			location[8];
};

struct RR_PL {
	char			location[8];
};

struct stamp {
	char			time[7];
};

struct RR_TF {
	char			flags;
	struct stamp		times[0];
};

struct rock_ridge {
	char			signature[2];
	unsigned char		len;
	unsigned char		version;
	union {
		struct SU_SP SP;
		struct SU_CE CE;
		struct SU_ER ER;
		struct RR_RR RR;
		struct RR_PX PX;
		struct RR_PN PN;
		struct RR_SL SL;
		struct RR_NM NM;
		struct RR_CL CL;
		struct RR_PL PL;
		struct RR_TF TF;
	} u;
};

#define SIG(a, b)		((a << 8) | b)

#define RR_PX_F			1   /* POSIX attributes */
#define RR_PN_F			2   /* POSIX devices */
#define RR_SL_F			4   /* Symbolic link */
#define RR_NM_F			8   /* Alternate Name */
#define RR_CL_F			16  /* Child link */
#define RR_PL_F			32  /* Parent link */
#define RR_RE_F			64  /* Relocation directory */
#define RR_TF_F			128 /* Timestamps */

#define TF_CREATE		1
#define TF_MODIFY		2
#define TF_ACCESS		4
#define TF_ATTRIBUTES		8
#define TF_BACKUP		16
#define TF_EXPIRATION		32
#define TF_EFFECTIVE		64
#define TF_LONG_FORM		128

/* global variables */
static int cont_extent;
static int cont_offset;
static int cont_size;
static void *buf;
static unsigned char *chr;
static int len;

/*
 * Setup rock ridge.
 */
static void __setup_rock_ridge(struct iso_directory_record *de)
{
	len = sizeof(struct iso_directory_record) + de->name_len[0];
	if (len & 1)
		len += 1;
	chr = ((unsigned char *) de) + len;
	len = *((unsigned char *) de) - len;

	cont_extent = 0;
	cont_offset = 0;
	cont_size = 0;
	buf = NULL;
}

/*
 * Check rock ridge signature.
 */
static int __check_sp(struct rock_ridge *rr)
{
	return rr->u.SP.magic[0] == 0xBE && rr->u.SP.magic[1] == 0xEF;
}

/*
 * Check extent.
 */
static void __check_ce(struct rock_ridge *rr)
{
	cont_extent = isofs_num733(rr->u.CE.extent);
	cont_offset = isofs_num733(rr->u.CE.offset);
	cont_size = isofs_num733(rr->u.CE.size);
}

/*
 * Continue rock ridge definition ?
 */
static int __maybe_continue(struct inode *inode)
{
	int block, offset, offset1;
	struct buffer_head *bh;

	/* free global buffer */
	if (buf)
		kfree(buf);

	/* done */
	if (!cont_extent)
		return 0;

	/* allocate new buffer */
	buf = kmalloc(cont_size);
	if (!buf)
		return 0;

	/* read next block */
	block = cont_extent;
	offset = cont_offset;
	offset1 = 0;

	bh = bread(inode->i_dev, block, inode->i_sb->s_blocksize);
	if (!bh)
		goto err;

	/* get block content */
	memcpy(buf + offset1, bh->b_data + offset, cont_size - offset1);
	brelse(bh);
	chr = (unsigned char *) buf;
	len = cont_size;
	cont_extent = 0;
	cont_size = 0;
	cont_offset = 0;

	return 1;
err:
	printf("Unable to read rock-ridge attributes\n");
	return 0;
}

/*
 * Parse rock ridge inode.
 */
int parse_rock_ridge_inode(struct iso_directory_record *de, struct inode *inode)
{
	struct rock_ridge *rr;
	int cnt, sig;

	/* setup rock ridge */
	__setup_rock_ridge(de);

repeat:
	/* parse rock ridge inode */
	while (len > 1) {
		/* get rock ridge struct */
		rr = (struct rock_ridge *) chr;
		if (rr->len == 0)
			goto out;
		sig = (chr[0] << 8) + chr[1];
		chr += rr->len; 
		len -= rr->len;
      
		/* handle command */
		switch(sig) {
			case SIG('R', 'R'):
				if ((rr->u.RR.flags[0] & (RR_PX_F | RR_TF_F | RR_SL_F | RR_CL_F)) == 0)
					goto out;
				break;
			case SIG('S', 'P'):
				if (!__check_sp(rr))
					goto out;
				break;
			case SIG('C', 'E'):
				__check_ce(rr);
				break;
			case SIG('E', 'R'):
				break;
			case SIG('P', 'X'):
				inode->i_mode = isofs_num733(rr->u.PX.mode);
				inode->i_nlinks = isofs_num733(rr->u.PX.n_links);
				inode->i_uid = isofs_num733(rr->u.PX.uid);
				inode->i_gid = isofs_num733(rr->u.PX.gid);
				break;
			case SIG('P', 'N'):
				printf("parse_rock_ridge_inode: SIG('P', 'N) not implemented\n");
				break;
			case SIG('T', 'F'):
				cnt = 0;
				if (rr->u.TF.flags & TF_CREATE) 
					inode->i_ctime = isofs_date(rr->u.TF.times[cnt++].time);
				if (rr->u.TF.flags & TF_MODIFY) 
					inode->i_mtime = isofs_date(rr->u.TF.times[cnt++].time);
				if (rr->u.TF.flags & TF_ACCESS) 
					inode->i_atime = isofs_date(rr->u.TF.times[cnt++].time);
				if (rr->u.TF.flags & TF_ATTRIBUTES) 
					inode->i_ctime = isofs_date(rr->u.TF.times[cnt++].time);
				break;
			case SIG('S', 'L'):
				printf("parse_rock_ridge_inode: SIG('S', 'L) not implemented\n");
				break;
			case SIG('R', 'E'):
				printf("parse_rock_ridge_inode: attempt to read inode for relocated directory\n");
				goto out;
			case SIG('C', 'L'):
				printf("parse_rock_ridge_inode: SIG('C', 'L) not implemented\n");
				break;
			default:
				break;
		}
	}

	/* continue rock ridge definition ? */
	if (__maybe_continue(inode))
		goto repeat;

	return 0;
out:
	if (buf)
		kfree(buf);
	return 0;
}

/*
 * Get rock ridge file name.
 */
int get_rock_ridge_filename(struct iso_directory_record *de, char *retname, struct inode *inode)
{
	int retname_len = 0, sig, truncate = 0;
	struct rock_ridge * rr;

	/* reset return name */
	*retname = 0;

	/* setup rock ridge */
	__setup_rock_ridge(de);

repeat:
	/* parse rock ridge inode */
	while (len > 1) {
		/* get rock ridge struct */
		rr = (struct rock_ridge *) chr;
		if (rr->len == 0)
			goto out;
		sig = (chr[0] << 8) + chr[1];
		chr += rr->len; 
		len -= rr->len;

		/* handle command */
		switch(sig) {
      			case SIG('R', 'R'):
				if ((rr->u.RR.flags[0] & RR_NM_F) == 0)
					goto out;
				break;
			case SIG('S', 'P'):
				if (!__check_sp(rr))
					goto out;
				break;
			case SIG('C', 'E'):
				__check_ce(rr);
				break;
			case SIG('N', 'M'):
				if (truncate)
					break;
				if (rr->u.NM.flags & ~1) {
					printf("Unsupported NM flag settings (%d)\n", rr->u.NM.flags);
					break;
				};
				if ((strlen(retname) + rr->len - 5) >= 254) {
					truncate = 1;
					break;
				};
				strncat(retname, rr->u.NM.name, rr->len - 5);
				retname_len += rr->len - 5;
				break;
			case SIG('R', 'E'):
				if (buf)
					kfree(buf);
				return -1;
			default:
				break;
		}
	}

	/* continue rock ridge definition ? */
	if (__maybe_continue(inode))
		goto repeat;

	return retname_len;
out:
	if (buf)
		kfree(buf);

	return 0;
}
