#include <string.h>

int
main(void)
{
	char a[] = "foo";
	char b[1024];
	strlcpy(b, a, sizeof(b));
	return(0);
}
