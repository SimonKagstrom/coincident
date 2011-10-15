#include <stdint.h>

#include <thread.hh>
#include <utils.hh>

#include <sys/user.h>

class Thread : public IThread
{
public:
	Thread(IThreadExitListener &listener,
			int (*fn)(void *), void *arg) : m_listener(listener)
	{
		/* Plenty of stack */
		size_t stack_sz = 8 * 1024 * 1024;
		m_stackStart = (uint8_t *)xmalloc(stack_sz);

		m_stack = m_stackStart + stack_sz - 8;

		void **p = ((void **)m_stack) - 2;

		memset(&m_regs, 0, sizeof(m_regs));

		p[0] = (void *)cleanup; // Return address
		p[1] = arg;
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


// Yes, this is ugly.
#include "thread.cc"
