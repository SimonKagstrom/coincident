#include "test.hh"
#include <coincident/coincident.h>

int main(int argc, const char *argv[])
{
	// We want to load the ELF symbols once only
	coincident_init();

	return crpcut::run(argc, argv);
}
