#include <sys/syscall.h>
#include <ipc/ipc.h>
#include <ipc/shm.h>
#include <stdio.h>
#include <stderr.h>

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
			return do_shmctl(first, second);
		case SHMDT:
			return do_shmdt((char *) ptr);
		default:
			printf("IPC system call %d not implemented\n", call);
			return -ENOSYS;
	}
}
