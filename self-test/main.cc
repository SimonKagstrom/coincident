#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <crpcut.hpp>
#include <stdio.h>
#include <pthread.h>

#include <coincident/coincident.h>

using namespace testing;

#include <sys/time.h>
uint64_t getTimeStamp(uint64_t start)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return (tv.tv_usec + tv.tv_sec * 1000 * 1000) - start;
}

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

		coincident_set_run_limit(100);

		uint64_t ts = getTimeStamp(0);
		int result = coincident_run();
		uint64_t nach = getTimeStamp(ts);
		printf("%Ld us\n", nach);
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

	TEST(test_time)
	{
		pthread_mutex_t mutex;

		pthread_mutex_init(&mutex, NULL);

		uint64_t ts = getTimeStamp(0);
		for (int i = 0; i < 100; i++)
		{
			int pid = fork();

			if (pid == 0)
			{
				wait(NULL);
				continue;
			}
			test_pthreads_non_race((void *)&mutex);
			test_pthreads_non_race((void *)&mutex);
			exit(0);
		}
		uint64_t nach = getTimeStamp(ts);
		printf("%Ld us\n", nach);
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
