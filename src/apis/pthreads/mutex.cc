#include <utils.hh>
#include <coincident/controller.hh>
#include <coincident/thread.hh>
#include <apis.hh>
#include <coincident/api-helpers/semaphore.hh>

#include <map>
#include <pthread.h>

using namespace coincident;

static void function_replacement(void)
{
}

class PthreadManager
{
public:
	static PthreadManager &getInstance();

	Semaphore *getSem(unsigned long addr)
	{
		if (m_semaphores.find(addr) == m_semaphores.end()) {
			Semaphore *p = new Semaphore(1);

			m_semaphores[addr] = p;
		}

		return m_semaphores[addr];
	}

private:
	typedef std::map<unsigned long, Semaphore *> SemaphoreMap_t;

	SemaphoreMap_t m_semaphores;
};

PthreadManager &PthreadManager::getInstance()
{
	static PthreadManager *instance;

	if (!instance)
		instance = new PthreadManager();

	return *instance;
}


class PthreadMutexBase
{
protected:
	Semaphore *lookupSemOnStop()
	{
		IController &controller = IController::getInstance();
		IThread *thread = controller.getCurrentThread();
		unsigned long mutex = thread->getArgument(0);

		Semaphore *sem = PthreadManager::getInstance().getSem(mutex);

		// Don't execute the real pthread stuff, instead just return
		thread->setPc((void *)function_replacement);

		return sem;
	}
};

class PthreadMutexLock : public IController::IFunctionHandler, public PthreadMutexBase
{
public:
	bool handle(IThread *curThread, void *addr, const PtraceEvent &ev)
	{
		Semaphore *sem = lookupSemOnStop();

		sem->wait();

		return true;
	}
};

class PthreadMutexUnlock : public IController::IFunctionHandler, public PthreadMutexBase
{
public:
	bool handle(IThread *curThread, void *addr, const PtraceEvent &ev)
	{
		Semaphore *sem = lookupSemOnStop();

		sem->signal();

		return true;
	}
};


void coincident_api_init_pthreads(void)
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
