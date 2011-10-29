#include <coincident/coincident.h>
#include <stdio.h>

static int fn_a(void *params)
{
	printf("fn_a!\n");
	return 1;
}

static int fn_b(void *params)
{
	printf("fn_b!\n");
	return 2;
}

int main(int argc, const char *argv[])
{
	coincident_add_thread(fn_a, NULL);
	coincident_add_thread(fn_b, NULL);

	coincident_set_run_limit(2);

	return coincident_run();
}
