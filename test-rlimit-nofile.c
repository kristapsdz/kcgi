#include <sys/resource.h>

int
main(void)
{
	struct rlimit rl_zero;

	rl_zero.rlim_cur = rl_zero.rlim_max = 0;
	return(-1 != setrlimit(RLIMIT_NOFILE, &rl_zero));
}
