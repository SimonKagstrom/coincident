int vobb(int a)
{
	return a + 1;
}

int mibb(int b)
{
	return b + 2;
}


int main(int argc, const char *argv[])
{
	return vobb(mibb(5));
}
