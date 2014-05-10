#include <stdlib.h>

int
main(void)
{
	char *a = reallocarray(NULL, 0, 0);
	return(NULL == a);
}
