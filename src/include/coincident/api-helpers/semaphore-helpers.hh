#pragma once

#include <map>

namespace coincident
{
	class Semaphore;

	class PthreadManager
	{
	public:
		static PthreadManager &getInstance();

		Semaphore *getSem(unsigned long addr);

	private:
		typedef std::map<unsigned long, Semaphore *> SemaphoreMap_t;

		SemaphoreMap_t m_semaphores;
	};

	class PthreadMutexBase
	{
	protected:
		Semaphore *lookupSemOnStop();
	};
}
