#ifndef _LIBC_UTSNAME_H_
#define _LIBC_UTSNAME_H_

struct utsname {
	char	sysname[65];
	char	nodename[65];
	char	release[65];
	char	version[65];
	char	machine[65];
	char	domainename[65];
};

int uname(struct utsname *buf);

#endif