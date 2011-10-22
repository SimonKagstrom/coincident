#include <controller.hh>
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
	IController &controller = IController::getInstance();

	controller.addThread(fn_a, NULL);
	controller.addThread(fn_b, NULL);

	controller.setRuns(1);
	controller.run();

	return 0;
}
