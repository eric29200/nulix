#ifndef _IOCTL_H_
#define _IOCTL_H_

#define _IOC_NRBITS		8
#define _IOC_TYPEBITS		8
#define _IOC_SIZEBITS		14
#define _IOC_DIRBITS		2

#define _IOC_NRSHIFT		0
#define _IOC_TYPESHIFT		(_IOC_NRSHIFT + _IOC_NRBITS)
#define _IOC_SIZESHIFT		(_IOC_TYPESHIFT + _IOC_TYPEBITS)
#define _IOC_DIRSHIFT		(_IOC_SIZESHIFT + _IOC_SIZEBITS)

#define _IOC_NONE		0U
#define _IOC_WRITE		1U
#define _IOC_READ		2U	 

#define _IOC(dir,type,nr,size) (((dir) << _IOC_DIRSHIFT) | ((type) << _IOC_TYPESHIFT) | ((nr) << _IOC_NRSHIFT) | ((size) << _IOC_SIZESHIFT))

#define _IO(type,nr)		_IOC(_IOC_NONE, (type), (nr), 0)
#define _IOR(type,nr,size)	_IOC(_IOC_READ, (type), (nr), sizeof(size))
#define _IOW(type,nr,size)	_IOC(_IOC_WRITE, (type), (nr), sizeof(size))
#define _IOWR(type,nr,size)	_IOC(_IOC_READ | _IOC_WRITE, (type), (nr), sizeof(size))

#endif