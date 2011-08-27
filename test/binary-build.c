#include <stdio.h>

int vobb(int a)
{
	printf("In vobb: %d\n", a);
	return a + 1;
}

int mibb(int b)
{
	printf("In mibb: %d\n", b);
	return b + 2;
}


int main(int argc, const char *argv[])
{
	return vobb(mibb(5));
}
