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
		p[1] = arg;
		m_regs.esp = (long)m_stack;
		m_regs.eip = (long)fn;
		m_regs.ebp = 0;

		m_blocked = false;
	}

	virtual ~Thread()
	{
		free(m_stackStart);
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

	unsigned long getReturnValue()
	{
		return m_regs.eax;
	}

	void setReturnValue(unsigned long value)
	{
		m_regs.eax = value;
	}

	void saveRegisters()
	{
		IPtrace &ptrace = IPtrace::getInstance();

		ptrace.saveRegisters(&m_regs);
		// The breakpoint points to the instruction AFTER the breakpoint
		m_regs.eip--;

		ptrace.saveFpRegisters(&m_fpregs);
	}

	void loadRegisters()
	{
		IPtrace &ptrace = IPtrace::getInstance();

		ptrace.loadFpRegisters(&m_fpregs);
		ptrace.loadRegisters(&m_regs);
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

	void *getPc()
	{
		return (void *)m_regs.eip;
	}

	int backtrace(unsigned long *buf, int maxValues)
	{
		unsigned long fp = m_regs.ebp;
		int n = 0;

		do {
			if (n >= maxValues)
				break;

			if (fp < (unsigned long)m_stackStart || fp > (unsigned long)m_stack)
				break;

			buf[n] = readProcessLong(fp + 4);

			fp = readProcessLong(fp);
			n++;
		} while (fp != 0);

		return n;
	}

	void dumpRegs(char *buf)
	{
		IPtrace::getInstance().saveRegisters(&m_regs);
		sprintf(buf,
				"eax 0x%08lx  ebx 0x%08lx  ecx 0x%08lx  edx 0x%08lx\n"
				"esp 0x%08lx  ebp 0x%08lx  esi 0x%08lx  edi 0x%08lx\n"
				"eip 0x%08lx  eflags 0x%04lx\n"
				"cs 0x%04lx  ss 0x%04lx  ds 0x%04lx  es 0x%04lx  fs 0x%04lx  gs 0x%04lx\n",
				m_regs.eax, m_regs.ebx, m_regs.ecx, m_regs.edx,
				m_regs.esp, m_regs.ebp, m_regs.esi, m_regs.edi,
				m_regs.eip, m_regs.eflags,
				m_regs.xcs, m_regs.xss, m_regs.xds, m_regs.xes, m_regs.xfs, m_regs.xgs
				);
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
	unsigned long readProcessLong(unsigned long addr)
	{
		IPtrace &ptrace = IPtrace::getInstance();
		union
		{
			uint8_t c[sizeof(unsigned long)];
			unsigned long v;
		} buf;

		ptrace.readProcessMemory(buf.c, (void *)addr, sizeof(unsigned long));

		return buf.v;
	}

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

		memset(&m_fpregs, 0, sizeof(m_fpregs));
	}


	uint8_t *m_stack;
	uint8_t *m_stackStart;
	struct user_regs_struct m_regs;
	struct user_fpxregs_struct m_fpregs;

	bool m_blocked;
};

IThread *IThread::createThread(void (*exitHook)(),
				int (*fn)(void *), void *arg)
{
	return new Thread(exitHook, fn, arg);
}

void IThread::releaseThread(IThread *thread)
{
	delete (Thread *)thread;
}

