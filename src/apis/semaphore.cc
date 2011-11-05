#include "semaphore.hh"
#include <utils.hh>
#include <coincident/controller.hh>
#include <coincident/thread.hh>

Semaphore::Semaphore(int maxValue, int startValue) :
	m_maxValue(maxValue), m_value(startValue)
{
	panic_if (m_value > m_maxValue,
			"Illegal start values: %d and max %d\n",
			m_value, m_maxValue);
}


void Semaphore::signal()
{
	if (m_value == m_maxValue)
		return;

	if (m_value == 0 && !m_waitList.empty()) {
		IThread *waiter = m_waitList.front();

		waiter->unBlock();
		m_waitList.pop_front();
		IController::getInstance().forceReschedule();
	}

	m_value++;
}

void Semaphore::wait()
{
	if (m_value > 0) {
		m_value--;
		return;
	}

	// m_value is 0 - block this thread!
	IController &controller = IController::getInstance();
	IThread *cur = controller.getCurrentThread();

	cur->block();
	m_waitList.push_back(cur);
	controller.forceReschedule();
}

bool Semaphore::tryWait()
{
	if (m_value == 0)
		return false;

	wait();

	return true;
}
