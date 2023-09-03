#ifndef _UTSNAME_H_
#define _UTSNAME_H_

#define UTSNAME_LEN	 65

/*
 * Uname structure.
 */
struct utsname {
	char sysname[UTSNAME_LEN];
	char nodename[UTSNAME_LEN];
	char release[UTSNAME_LEN];
	char version[UTSNAME_LEN];
	char machine[UTSNAME_LEN];
};

#endif
