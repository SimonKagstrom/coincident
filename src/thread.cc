// Included from thread-ia32.cc etc (yes, ugly)

#include <utils.hh>

#define N_THREADS 16
static IThread *threads[N_THREADS];

IThread &IThread::createThread(void (*exitHook)(),
		int (*fn)(void *), void *arg)
{
	int i;

	for (i = 0; i < N_THREADS; i++) {
		if (!threads[i])
			break;
	}
	if (i == N_THREADS)
		panic("No free threads!");

	threads[i] = new Thread(exitHook, fn, arg);

	return *threads[i];
}

void IThread::releaseThread(IThread &thread)
{
	int i;

	for (i = 0; i < N_THREADS; i++) {
		if (threads[i] == &thread) {
			delete threads[i];
			threads[i] = NULL;
			return;
		}
	}

	panic("No such thread???");
}
