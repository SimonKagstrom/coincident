#pragma once

#include <stdint.h>
#include <stdlib.h>

enum ptrace_event_type
{
	ptrace_error,
	ptrace_breakpoint,
	ptrace_syscall,
	ptrace_crash,
	ptrace_exit
};

class IPtrace
{
public:
	class PtraceEvent
	{
	public:
		ptrace_event_type type;

		int eventId; // Typically the breakpoint
		void *addr;
	};

	static IPtrace &getInstance();


	virtual bool readMemory(uint8_t *dst, void *start, size_t bytes) = 0;


	/**
	 * Set a breakpoint
	 *
	 * @param addr the address to set the breakpoint on
	 *
	 * @return the ID of the breakpoint, or -1 on failure
	 */
	virtual int setBreakpoint(void *addr) = 0;

	virtual bool clearBreakpoint(int id) = 0;

	/**
	 * For a new process and attach to it with ptrace
	 *
	 * @return 0 when in the child, -1 on error or the pid otherwise
	 */
	virtual int forkAndAttach() = 0;

	/**
	 * Step over to the next instruction.
	 *
	 * Does not invoke the breakpoint listener
	 */
	virtual void singleStep(int pid) = 0;

	/**
	 * Continue execution until next breakpoint.
	 *
	 * @param pid the process to continue
	 *
	 * @return the event the execution stopped at.
	 */
	virtual const PtraceEvent continueExecution(int pid) = 0;
};
