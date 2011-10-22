#pragma once

#include <stdint.h>
#include <list>

class IFunction
{
public:
	virtual const char *getName() = 0;

	virtual void *getEntry() = 0;

	virtual size_t getSize() = 0;

	virtual std::list<void *> &getMemoryRefs() = 0;
};
