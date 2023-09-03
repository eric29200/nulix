#include <fs/iso_fs.h>

int isofs_num711(char *p)
{
	return *p & 0xFF;
}

int isofs_num721(char *p)
{
	return ((p[0] & 0xFF) | ((p[1] & 0xFF) << 8));
}

int isofs_num723(char *p)
{
	return isofs_num721(p);
}

int isofs_num731(char *p)
{
	return ((p[0] & 0xFF) | ((p[1] & 0xFF) << 8) | ((p[2] & 0xFF) << 16) | ((p[3] & 0xFF) << 24));
}

int isofs_num733(char *p)
{
	return isofs_num731(p);
}

int isofs_date(char *p)
{
	int monlen[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	int year, month, day, hour ,minute, second, tz;
	int crtime, days, i;

	year = p[0] - 70;
	month = p[1];
	day = p[2];
	hour = p[3];
	minute = p[4];
	second = p[5];
	tz = p[6];

	if (year < 0) {
		crtime = 0;
	} else {
		days = year * 365;
		if (year > 2)
			days += (year + 1) / 4;

		for (i = 1; i < month; i++)
			days += monlen[i-1];

		if (((year + 2) % 4) == 0 && month > 2)
			days++;

		days += day - 1;
		crtime = ((((days * 24) + hour) * 60 + minute) * 60) + second;

		/* timezone offset is unreliable on some disks */
		if (-48 <= tz && tz <= 52)
			crtime += tz * 15 * 60;
	}

	return crtime;
}

/*
 * Get parent inode number.
 */
ino_t isofs_parent_ino(struct inode *inode)
{
	if (isofs_sb(inode->i_sb)->s_firstdatazone != inode->i_ino)
		return inode->u.iso_i.i_backlink;

	return inode->i_ino;
}

/*
 * Translate a ISOFS name.
 */
int isofs_name_translate(char *old, size_t old_len, char *new)
{
	size_t i;
	int c;

	for (i = 0; i < old_len; i++) {
		c = old[i];

		/* end of name */
		if (!c)
			break;

		/* lower case */
		if (c >= 'A' && c <= 'Z')
			c |= 0x20;

		/* remove trailing ".;1" */
		if (c == '.' && i == old_len - 3 && old[i + 1] == ';' && old[i + 2] == '1')
			break;

		/* remove trailing ";1" */
		if (c == ';' && i == old_len - 2 && old[i + 1] == '1')
			break;

		/* convert remaining ';' to '.' */
		if (c == ';')
			c = '.';

		/* convert */
		new[i] = c;
	}

	return i;
}
