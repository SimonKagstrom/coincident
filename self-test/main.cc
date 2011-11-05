#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <crpcut.hpp>
#include <stdio.h>

#include <coincident/coincident.h>

using namespace testing;


/*
 * The code below is not stupid, but VERY stupid.
 *
 * However, it's just for testing coincident.
 */
int global;

int add_val(int v)
{
	global += v;

	return global;
}

static int test_race(void *params)
{
	global = 1;

	ASSERT_TRUE(global == 1);

	int v = add_val(1);
	ASSERT_TRUE(v == 2);

	ASSERT_TRUE(v == global);

	return 0;
}


// Same as above, but without races (stack allocated stuff)
int add_val_non_race(int *src, int v)
{
	*src += v;

	return *src;
}

static int test_non_race(void *params)
{
	int src = 1;

	ASSERT_TRUE(src == 1);

	int v = add_val_non_race(&src, 1);
	ASSERT_TRUE(v == 2);

	ASSERT_TRUE(v == src);

	return 0;
}


TESTSUITE(coincident)
{
	TEST(basic_race)
	{
		// We use random scheduling here, so this actually affects
		// the result
		srand(time(NULL));

		coincident_add_thread(test_race, NULL);
		coincident_add_thread(test_race, NULL);

		coincident_set_run_limit(10);

		int result = coincident_run();
		ASSERT_TRUE(result == 0);
	}

	TEST(basic_non_race)
	{
		srand(time(NULL));

		coincident_add_thread(test_race, NULL);
		coincident_add_thread(test_non_race, NULL);
		coincident_add_thread(test_non_race, NULL);

		// Stop after 500 ms
		coincident_set_time_limit(500);

		int result = coincident_run();
		ASSERT_TRUE(result == 0);
	}
}

int main(int argc, const char *argv[])
{
	coincident_init();

	return crpcut::run(argc, argv);
}
