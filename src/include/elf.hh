#pragma once

#include <sys/types.h>

class IFunction;

class IElf
{
public:
	class IFunctionListener
	{
	public:
		virtual void onFunction(IFunction &fn) = 0;
	};

	static IElf &getInstance();


	virtual bool setFile(IFunctionListener *listener,
			const char *filename) = 0;

	virtual IFunction *functionByName(const char *name) = 0;

	virtual IFunction *functionByAddress(void *addr) = 0;
};
