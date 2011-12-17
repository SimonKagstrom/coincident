#include <utils.hh>
#include <coincident/controller.hh>
#include <coincident/thread.hh>
#include <apis.hh>
#include <coincident/api-helpers/semaphore.hh>
#include <coincident/api-helpers/semaphore-helpers.hh>

#include <map>
#include <pthread.h>

#include "common.h"

using namespace coincident;


class PthreadMutexLock : public IController::IFunctionHandler, public SemaphoreBase
{
public:
	bool handle(IThread *curThread, void *addr, const PtraceEvent &ev)
	{
		Semaphore *sem = lookupSemOnStop();

		sem->wait();

		return true;
	}
};

class PthreadMutexUnlock : public IController::IFunctionHandler, public SemaphoreBase
{
public:
	bool handle(IThread *curThread, void *addr, const PtraceEvent &ev)
	{
		Semaphore *sem = lookupSemOnStop();

		sem->signal();

		return true;
	}
};

void local_coincident_api_init_pthreads_mutex(void)
{
	IController &controller = IController::getInstance();
	bool res;

	res = controller.registerFunctionHandler((void *)pthread_mutex_lock,
			new PthreadMutexLock());
	panic_if(!res, "Can't register ptrace_mutex_lock handler");
	res = controller.registerFunctionHandler((void *)pthread_mutex_unlock,
			new PthreadMutexUnlock());
	panic_if(!res, "Can't register ptrace_mutex_unlock handler");
}
