#pragma once

#include <map>

namespace coincident
{
	class Semaphore;

	class SemaphoreManager
	{
	public:
		static SemaphoreManager &getInstance();

		Semaphore *getSem(unsigned long addr);

		void clearSemaphores();

	private:
		typedef std::map<unsigned long, Semaphore *> SemaphoreMap_t;

		SemaphoreMap_t m_semaphores;
	};

	class SemaphoreBase
	{
	protected:
		Semaphore *lookupSemOnStop();
	};
}
