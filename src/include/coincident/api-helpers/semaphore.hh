#include <list>

namespace coincident
{
	class IThread;

	class Semaphore
	{
	public:
		Semaphore(int maxValue, int startValue = 1);

		void signal();

		void wait();

		bool tryWait();

	private:
		typedef std::list<IThread *> WaitList_t;

		WaitList_t m_waitList;
		int m_value;
		int m_maxValue;
	};
}
