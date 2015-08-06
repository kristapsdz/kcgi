#include <sys/tame.h>

#include <stdio.h>

int
main(void)
{
	if (-1 == tame(0)) {
		perror("tame");
		return(1);
	}
	return(0);
}
