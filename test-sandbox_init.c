#include <sandbox.h>

int
main(void)
{
	char	*er;

	sandbox_init("kSBXProfilePureComputation", SANDBOX_NAMED, &er);
	sandbox_free_error(er);
	return(0);
}
