#pragma once

#include <sys/types.h>

namespace coincident
{
	class IFunction;

	class IElf
	{
	public:
		class IFunctionListener
		{
		public:
			virtual void onFunction(IFunction &fn) = 0;
		};

		static IElf *open(const char *filename);


		virtual bool setFile(IFunctionListener *listener) = 0;

		virtual IFunction *functionByName(const char *name) = 0;

		virtual IFunction *functionByAddress(void *addr) = 0;
	};
}
