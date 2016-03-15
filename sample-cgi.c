#include <stdlib.h>
#include <stdio.h>

int
main(void)
{

	puts("Status: 200 OK\r");
	puts("Content-Type: text/html\r");
	puts("\r");
	puts("Hello, world.");
	return(EXIT_SUCCESS);
}
