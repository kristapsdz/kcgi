#include <sys/prctl.h>
#include <linux/seccomp.h>
#include <errno.h>

int
main(void)
{

	prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, 0);
	return(EFAULT == errno ? 0 : 1);
}
