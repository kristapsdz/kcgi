#include <stdlib.h>

int
main(void)
{
	volatile int foo = arc4random() % 2;

	if (foo > 0)
		return(0);

	return(foo);
}
