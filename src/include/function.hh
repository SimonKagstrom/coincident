#pragma once

#include <stdint.h>
#include <list>

namespace coincident
{
	class IFunction
	{
	public:
		typedef std::list<void *> ReferenceList_t;

		virtual const char *getName() = 0;

		virtual void *getEntry() = 0;

		virtual size_t getSize() = 0;

		virtual ReferenceList_t &getMemoryLoads() = 0;

		virtual ReferenceList_t &getMemoryStores() = 0;
	};
}
