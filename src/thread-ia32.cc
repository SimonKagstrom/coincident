#include <stdint.h>

#include <coincident/thread.hh>
#include <utils.hh>
#include <ptrace.hh>

#include <sys/user.h>

using namespace coincident;

extern "C" void cleanupAsm(void);

class Thread : public IThread
{
public:
	Thread(void (*exitHook)(),
			int (*fn)(void *), void *arg)
	{
		/* Plenty of stack */
		size_t stack_sz = 8 * 1024 * 1024;
		m_stackStart = (uint8_t *)xmalloc(stack_sz);

		m_stack = m_stackStart + stack_sz - 8;

		void **p = (void **)m_stack;

		setupRegs();

		p[0] = (void *)exitHook; // Return address
		m_regs.esp = (long)m_stack;
		m_regs.eip = (long)fn;
		m_regs.ebp = 0;

		m_blocked = false;
	}

	virtual ~Thread()
	{
		free(m_stackStart);
	}

	void *getRegs()
	{
		return (void *)&m_regs;
	}

	unsigned long getArgument(int n)
	{
		uint8_t *sp = (uint8_t *)m_regs.esp;
		unsigned long out;

		// The return address is at the top of the stack
		IPtrace::getInstance().readProcessMemory((uint8_t *)&out,
				sp + (1 + n) * sizeof(unsigned long), sizeof(unsigned long));

		return out;
	}

	void saveRegisters()
	{
		IPtrace &ptrace = IPtrace::getInstance();

		ptrace.saveRegisters(&m_regs);
		// The breakpoint points to the instruction AFTER the breakpoint
		m_regs.eip--;
	}

	void stepOverBreakpoint()
	{
		IPtrace &ptrace = IPtrace::getInstance();

		ptrace.singleStep();
		ptrace.saveRegisters(&m_regs);
	}

	void setPc(void *addr)
	{
		m_regs.eip = (unsigned long)addr;
	}

	void block()
	{
		m_blocked = true;
	}

	bool isBlocked()
	{
		return m_blocked;
	}

	void unBlock()
	{
		m_blocked = false;
	}

private:
	void setupRegs()
	{
		memset(&m_regs, 0, sizeof(m_regs));
		asm volatile(
				"pushf\n"
				"popl 0(%[reg])\n"
				: : [reg]"r"(&m_regs.eflags) : "memory" );
		asm volatile("mov    %%cs, 0(%[reg])\n"
				: : [reg]"r"(&m_regs.xcs) : "memory" );
		asm volatile("mov    %%ds, 0(%[reg])\n"
				: : [reg]"r"(&m_regs.xds) : "memory" );
		asm volatile("mov    %%ss, 0(%[reg])\n"
				: : [reg]"r"(&m_regs.xss) : "memory" );
		asm volatile("mov    %%es, 0(%[reg])\n"
				: : [reg]"r"(&m_regs.xes) : "memory" );
		asm volatile("mov    %%fs, 0(%[reg])\n"
				: : [reg]"r"(&m_regs.xfs) : "memory" );
		asm volatile("mov    %%gs, 0(%[reg])\n"
				: : [reg]"r"(&m_regs.xgs) : "memory" );
	}


	uint8_t *m_stack;
	uint8_t *m_stackStart;
	struct user_regs_struct m_regs;

	bool m_blocked;
};

// Yes, this is ugly.
#include "thread.cc"
