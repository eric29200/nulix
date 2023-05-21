#include <signal.h>
#include <unistd.h>

#include "../x86/__syscall.h"

static const unsigned long all_mask[] = {
	-1UL, -1UL, -1UL, -1UL
};

void __block_all_sigs(void *set)
{
	__syscall4(SYS_rt_sigprocmask, SIG_BLOCK, (long) &all_mask, (long) set, NSIG / 8);
}