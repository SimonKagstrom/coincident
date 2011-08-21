#pragma once

#include <sys/types.h>

class IDisassembly
{
public:
	class IInstructionListener
	{
	public:
		virtual void onMemoryReference(off_t offset, bool isLoad) = 0;

		virtual void onCall(off_t offset) = 0;

		virtual void onBranch(off_t offset) = 0;
	};

	static IDisassembly &getInstance();


	virtual bool execute(IInstructionListener *listener,
			uint8_t *data, size_t size) = 0;
};
