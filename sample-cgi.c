#include <stdlib.h>
#include <stdio.h>

int
main(void)
{

	printf("Status: 200 OK\r\n");
	printf("Content-Type: text/html\r\n");
	printf("\r\n");
	printf("Hello, world.\n");
	return(EXIT_SUCCESS);
}
