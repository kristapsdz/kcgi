#include <stddef.h>
#include <zlib.h>

int
main(void)
{
	gzFile		 gz;

	if (NULL == (gz = gzopen("/dev/null", "w")))
		return(1);
	gzputs(gz, "foo");
	gzclose(gz);
	return(0);
}
