#include <unistd.h>

#include <stdio.h>

int
main(void)
{
	if (-1 == pledge("stdio", NULL)) {
		perror("pledge");
		return(1);
	}
	return(0);
}
