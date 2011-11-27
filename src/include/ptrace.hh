#pragma once

#include <stdint.h>
#include <stdlib.h>

namespace coincident
{
	enum ptrace_event_type
	{
		ptrace_error       = -1,
		ptrace_breakpoint  =  1,
		ptrace_syscall     =  2,
		ptrace_crash       =  3,
		ptrace_exit        =  4,
	};

	class PtraceEvent
	{
	public:
		enum ptrace_event_type type;

		int eventId; // Typically the breakpoint
		void *addr;
	};

	class IPtrace
	{
	public:
		static IPtrace &getInstance();


		virtual bool readMemory(uint8_t *dst, void *start, size_t bytes) = 0;

		virtual bool readProcessMemory(uint8_t *dst, void *start, size_t bytes) = 0;

		/**
		 * Set a breakpoint
		 *
		 * @param addr the address to set the breakpoint on
		 *
		 * @return the ID of the breakpoint, or -1 on failure
		 */
		virtual int setBreakpoint(void *addr) = 0;

		virtual bool clearBreakpoint(int id) = 0;


		virtual void clearAllBreakpoints() = 0;


		/**
		 * For a new process and attach to it with ptrace
		 *
		 * @return 0 when in the child, -1 on error or the pid otherwise
		 */
		virtual int forkAndAttach() = 0;


		virtual void loadRegisters(void *regs) = 0;

		virtual void saveRegisters(void *regs) = 0;

		/**
		 * Step over to the next instruction.
		 *
		 * Does not invoke the breakpoint listener
		 */
		virtual void singleStep() = 0;

		/**
		 * Continue execution until next breakpoint.
		 *
		 * @return the event the execution stopped at.
		 */
		virtual const PtraceEvent continueExecution() = 0;

		virtual void kill() = 0;
	};
}
