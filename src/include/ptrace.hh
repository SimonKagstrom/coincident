#pragma once

#include <stdint.h>

class IPtrace
{
public:
	class IBreakpointListener
	{
	public:
		virtual void onBreak(void *instructionAddr) = 0;
	};

	static IPtrace &getInstance();


	virtual bool readMemory(uint8_t *dst, void *start, size_t bytes) = 0;


	virtual bool setBreakpoint(void *addr) = 0;

	virtual bool clearBreakpoint(void *addr) = 0;


	/**
	 * Step over to the next instruction.
	 *
	 * Does not invoke the breakpoint listener
	 */
	virtual void singleStep() = 0;

	/**
	 * Continue execution until next breakpoint.
	 */
	virtual void continueExecution() = 0;
};
