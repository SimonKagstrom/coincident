#pragma once

#include <stdint.h>
#include <list>

namespace coincident
{
	class IFunction
	{
	public:
		virtual const char *getName() = 0;

		virtual void *getEntry() = 0;

		virtual size_t getSize() = 0;

		virtual std::list<void *> &getMemoryLoads() = 0;

		virtual std::list<void *> &getMemoryStores() = 0;
	};
}
