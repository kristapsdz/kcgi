#include <stdlib.h>

int
main(void)
{
	const char *ep;
	int a = strtonum("20", 0, 30, &ep);
	return(a != 20);
}
