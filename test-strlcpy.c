#include <string.h>

int
main(void)
{
	char a[] = "foo";
	char b[1024];
	b[0] = '\0';
	strlcpy(b, a, sizeof(b));
	return(0);
}
