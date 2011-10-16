#include <stdint.h>

#include <thread.hh>
#include <utils.hh>

#include <sys/user.h>

extern "C" void cleanupAsm(void);

class Thread : public IThread
{
public:
	Thread(IThreadExitListener &listener,
			int (*fn)(void *), void *arg) : m_listener(listener)
	{
		/* Plenty of stack */
		size_t stack_sz = 8 * 1024 * 1024;
		m_stackStart = (uint8_t *)xmalloc(stack_sz);

		m_stack = m_stackStart + stack_sz - 16;

		void **p = (void **)m_stack;

		setupRegs();

		p[3] = (void *)this;
		p[2] = (void *)this;
		p[1] = (void *)cleanup;
		p[0] = (void *)cleanupAsm; // Return address
		m_regs.esp = (long)m_stack;
		m_regs.eip = (long)fn;
		m_regs.ebp = 0;
	}

	virtual ~Thread()
	{
		free(m_stackStart);
	}

	void *getRegs()
	{
		return (void *)&m_regs;
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

	static void cleanup(void *p)
	{
		Thread *pThis = (Thread *)p;

		pThis->m_listener.threadExit(*pThis);
	}

	uint8_t *m_stack;
	uint8_t *m_stackStart;
	IThreadExitListener &m_listener;
	struct user_regs_struct m_regs;
};

asm(
		".pushsection .text \n"
		"cleanupAsm:        \n"
		"   popl    %eax    \n"
		"   jmp    *%eax    \n"
		".popsection        \n"
);


// Yes, this is ugly.
#include "thread.cc"
