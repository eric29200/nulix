#include <stdio.h>
#include <errno.h>

static char *__error_msgs[] = {
	[0]			= "No error information",
	[EILSEQ]		= "Illegal byte sequence",
	[EDOM]			= "Domain error",
	[ERANGE]		= "Result not representable",
	[ENOTTY]		= "Not a tty",
	[EACCES]		= "Permission denied",
	[EPERM]			= "Operation not permitted",
	[ENOENT]		= "No such file or directory",
	[ESRCH]			= "No such process",
	[EEXIST]		= "File exists",
	[EOVERFLOW]		= "Value too large for data type",
	[ENOSPC]		= "No space left on device",
	[ENOMEM]		= "Out of memory",
	[EBUSY]			= "Resource busy",
	[EINTR]			= "Interrupted system call",
	[EAGAIN]		= "Resource temporarily unavailable",
	[ESPIPE]		= "Invalid seek",
	[EXDEV]			= "Cross-device link",
	[EROFS]			= "Read-only file system",
	[ENOTEMPTY]		= "Directory not empty",
	[ECONNRESET]		= "Connection reset by peer",
	[ETIMEDOUT]		= "Operation timed out",
	[ECONNREFUSED]		= "Connection refused",
	[EHOSTDOWN]		= "Host is down",
	[EHOSTUNREACH]		= "Host is unreachable",
	[EADDRINUSE]		= "Address in use",
	[EPIPE]			= "Broken pipe",
	[EIO]			= "I/O error",
	[ENXIO]			= "No such device or address",
	[ENOTBLK]		= "Block device required",
	[ENODEV]		= "No such device",
	[ENOTDIR]		= "Not a directory",
	[EISDIR]		= "Is a directory",
	[ETXTBSY]		= "Text file busy",
	[ENOEXEC]		= "Exec format error",
	[EINVAL]		= "Invalid argument",
	[E2BIG]			= "Argument list too long",
	[ELOOP]			= "Symbolic link loop",
	[ENAMETOOLONG]		= "Filename too long",
	[ENFILE]		= "Too many open files in system",
	[EMFILE]		= "No file descriptors available",
	[EBADF]			= "Bad file descriptor",
	[ECHILD]		= "No child process",
	[EFAULT]		= "Bad address",
	[EFBIG]			= "File too large",
	[EMLINK]		= "Too many links",
	[ENOLCK]		= "No locks available",
	[EDEADLK]		= "Resource deadlock would occur",
	[ENOTRECOVERABLE]	= "State not recoverable",
	[EOWNERDEAD]		= "Previous owner died",
	[ECANCELED]		= "Operation canceled",
	[ENOSYS]		= "Function not implemented",
	[ENOMSG]		= "No message of desired type",
	[EIDRM]			= "Identifier removed",
	[ENOSTR]		= "Device not a stream",
	[ENODATA]		= "No data available",
	[ETIME]			= "Device timeout",
	[ENOSR]			= "Out of streams resources",
	[ENOLINK]		= "Link has been severed",
	[EPROTO]		= "Protocol error",
	[EBADMSG]		= "Bad message",
	[EBADFD]		= "File descriptor in bad state",
	[ENOTSOCK]		= "Not a socket",
	[EDESTADDRREQ]		= "Destination address required",
	[EMSGSIZE]		= "Message too large",
	[EPROTOTYPE]		= "Protocol wrong type for socket",
	[ENOPROTOOPT]		= "Protocol not available",
	[EPROTONOSUPPORT]	= "Protocol not supported",
	[ESOCKTNOSUPPORT]	= "Socket type not supported",
	[ENOTSUP]		= "Not supported",
	[EPFNOSUPPORT]		= "Protocol family not supported",
	[EAFNOSUPPORT]		= "Address family not supported by protocol",
	[EADDRNOTAVAIL]		= "Address not available",
	[ENETDOWN]		= "Network is down",
	[ENETUNREACH]		= "Network unreachable",
	[ENETRESET]		= "Connection reset by network",
	[ECONNABORTED]		= "Connection aborted",
	[ENOBUFS]		= "No buffer space available",
	[EISCONN]		= "Socket is connected",
	[ENOTCONN]		= "Socket not connected",
	[ESHUTDOWN]		= "Cannot send after socket shutdown",
	[EALREADY]		= "Operation already in progress",
	[EINPROGRESS]		= "Operation in progress",
	[ESTALE]		= "Stale file handle",
	[EREMOTEIO]		= "Remote I/O error",
	[EDQUOT]		= "Quota exceeded",
	[ENOMEDIUM]		= "No medium found",
	[EMEDIUMTYPE]		= "Wrong medium type",
	[EMULTIHOP]		= "Multihop attempted",
	[ENOKEY]		= "Required key not available",
	[EKEYEXPIRED]		= "Key has expired",
	[EKEYREVOKED]		= "Key has been revoked",
	[EKEYREJECTED]		= "Key was rejected by service",
};

char *strerror(int errnum)
{
	if (errnum < 0 || (size_t) errnum >= sizeof(__error_msgs) / sizeof(*__error_msgs))
		errnum = 0;

	return __error_msgs[errnum];
}