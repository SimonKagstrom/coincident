#include <stdio.h>
#include <pthread.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <crpcut.hpp>

#include <coincident/coincident.h>
#include <coincident/controller.hh>

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

static int test_pthreads_non_race(void *params)
{
	pthread_mutex_t *mutex = (pthread_mutex_t *)params;

	pthread_mutex_lock(mutex);
	global = 1;

	ASSERT_TRUE(global == 1);

	int v = add_val(1);
	ASSERT_TRUE(v == 2);

	ASSERT_TRUE(v == global);
	pthread_mutex_unlock(mutex);

	return 0;
}

static int test_pthreads_race(void *params)
{
	pthread_mutex_t *mutex = (pthread_mutex_t *)params;

	pthread_mutex_lock(mutex);
	global = 1;

	ASSERT_TRUE(global == 1);
	pthread_mutex_unlock(mutex);

	pthread_mutex_lock(mutex);
	int v = add_val(1);
	ASSERT_TRUE(v == 2);

	ASSERT_TRUE(v == global);
	pthread_mutex_unlock(mutex);

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

class GMockTest
{
public:
	MOCK_METHOD1(test1, void(int a));
	MOCK_METHOD0(test2, int());
};

static int test_gmock_mock_in_function(void *p)
{
	GMockTest mock;

	EXPECT_CALL(mock, test1(_))
	.Times(AtLeast(1));
	EXPECT_CALL(mock, test2())
	.Times(AtLeast(1));

	mock.test1(5);
	mock.test2();

	return 0;
}

static int test_gmock_expect_in_function(void *p)
{
	GMockTest *mock = (GMockTest *)p;

	EXPECT_CALL(*mock, test1(_))
	.Times(AtLeast(1));

	mock->test1(5);

	return 0;
}

static int test_crash(void *p)
{
	int (*v)() = (int (*)())p;

	v();

	return 0;
}

TESTSUITE(coincident)
{
	TEST(basic_race)
	{
		coincident_add_thread(test_race, NULL);
		coincident_add_thread(test_race, NULL);

		coincident_set_run_limit(10);

		int result = coincident_run();
		ASSERT_TRUE(result == 0);
	}

	TEST(basic_non_race)
	{
		coincident_add_thread(test_race, NULL);
		coincident_add_thread(test_non_race, NULL);
		coincident_add_thread(test_non_race, NULL);

		// Stop after 500 ms
		coincident_set_time_limit(500);

		int result = coincident_run();
		ASSERT_TRUE(result == 0);
	}

	TEST(pthreads_non_race)
	{
		pthread_mutex_t mutex;

		pthread_mutex_init(&mutex, NULL);

		coincident_add_thread(test_pthreads_non_race, &mutex);
		coincident_add_thread(test_pthreads_non_race, &mutex);

		coincident_set_time_limit(1000);

		int result = coincident_run();
		ASSERT_TRUE(result == 0);
	}

	TEST(pthreads_race)
	{
		pthread_mutex_t mutex;

		pthread_mutex_init(&mutex, NULL);

		coincident_add_thread(test_pthreads_race, &mutex);
		coincident_add_thread(test_pthreads_race, &mutex);

		coincident_set_time_limit(1000);

		int result = coincident_run();
		ASSERT_TRUE(result == 0);
	}

	TEST(gmock_expectations_in_test_function)
	{
		GMockTest mock;

		test_gmock_expect_in_function((void *)&mock);

		coincident_add_thread(test_gmock_expect_in_function, (void *)&mock);
		coincident_set_run_limit(1);

		int result = coincident_run();
		if (result != 0)
			printf("ERROR: %s\n", coincident::IController::getInstance().getError());
		ASSERT_TRUE(result == 0);
	}

	TEST(gmock_mock_in_function)
	{
		coincident_add_thread(test_gmock_mock_in_function, NULL);
		coincident_set_run_limit(1);

		int result = coincident_run();
		if (result != 0)
			printf("ERROR: %s\n", coincident::IController::getInstance().getError());
		ASSERT_TRUE(result == 0);
	}

	TEST(crash)
	{
		// Tests backtrace
		coincident_add_thread(test_crash, NULL);
		coincident_set_run_limit(1);

		int result = coincident_run();
		if (result != 0)
			printf("ERROR: %s\n", coincident::IController::getInstance().getError());
		ASSERT_TRUE(result == 0);
	}
}

int main(int argc, const char *argv[])
{
	// We use random scheduling here, so this actually affects
	// the result
	srand(time(NULL));

	coincident_init();

	return crpcut::run(argc, argv);
}
