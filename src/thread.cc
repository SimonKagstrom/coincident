// Included from thread-ia32.cc etc (yes, ugly)

#include <utils.hh>
#include <coincident/thread.hh>

using namespace coincident;

#define N_THREADS 16
static IThread *threads[N_THREADS];

IThread &ThreadFactory::createThread(void (*exitHook)(),
		int (*fn)(void *), void *arg)
{
	int i;

	for (i = 0; i < N_THREADS; i++) {
		if (!threads[i])
			break;
	}
	if (i == N_THREADS)
		panic("No free threads!");

	threads[i] = IThread::createThread(exitHook, fn, arg);

	return *threads[i];
}

void ThreadFactory::releaseThread(IThread &thread)
{
	int i;

	for (i = 0; i < N_THREADS; i++) {
		if (threads[i] == &thread) {
			IThread::releaseThread(threads[i]);
			threads[i] = NULL;
			return;
		}
	}

	panic("No such thread???");
}
