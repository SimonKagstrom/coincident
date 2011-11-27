#include <coincident/api-helpers/semaphore-helpers.hh>
#include <coincident/api-helpers/semaphore.hh>
#include <coincident/controller.hh>
#include <coincident/thread.hh>

using namespace coincident;

static void function_replacement(void)
{
}

Semaphore *SemaphoreManager::getSem(unsigned long addr)
{
	if (m_semaphores.find(addr) == m_semaphores.end()) {
		Semaphore *p = new Semaphore(1);

		m_semaphores[addr] = p;
	}

	return m_semaphores[addr];
}

void SemaphoreManager::clearSemaphores()
{
	for (SemaphoreManager::SemaphoreMap_t::iterator it = m_semaphores.begin();
			it != m_semaphores.end(); it++)
		delete it->second;

	m_semaphores.clear();
}

SemaphoreManager &SemaphoreManager::getInstance()
{
	static SemaphoreManager *instance;

	if (!instance)
		instance = new SemaphoreManager();

	return *instance;
}



Semaphore *SemaphoreBase::lookupSemOnStop()
{
	IController &controller = IController::getInstance();
	IThread *thread = controller.getCurrentThread();
	unsigned long mutex = thread->getArgument(0);

	Semaphore *sem = SemaphoreManager::getInstance().getSem(mutex);

	// Don't execute the real pthread stuff, instead just return
	thread->setPc((void *)function_replacement);

	return sem;
}
