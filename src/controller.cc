#include <controller.hh>
#include <ptrace.hh>
#include <utils.hh>
#include <elf.hh>
#include <thread.hh>
#include <function.hh>

#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <map>

class DefaultThreadSelector : public IController::IThreadSelector
{
public:
	int selectThread(int curThread,
			int nThreads,
			uint64_t timeUs,
			const IPtrace::PtraceEvent *ev)
	{
		return rand() % nThreads;
	}
};


class Controller : public IController, IThread::IThreadExitListener, IElf::IFunctionListener
{
public:
	Controller()
	{
		m_threads = NULL;
		m_nThreads = 0;
		m_curThread = 0;

		m_selector = new DefaultThreadSelector();
		m_startTimeStamp = getTimeStamp(0);

		IElf &elf = IElf::getInstance();

		elf.setFile(this, "/proc/self/exe");
	}

	virtual ~Controller()
	{
		cleanup();
	}


	void onFunction(IFunction &fn)
	{
		m_functions[fn.getEntry()] = &fn;
	}

	// Thread exit handler from the IThreadExitListener class
	void threadExit(IThread &thread)
	{
		// Do something
		printf("Thread exited\n");
		IThread::releaseThread(thread);
	}


	void setThreadSelector(IThreadSelector *selector)
	{
		if (m_selector)
			delete m_selector;

		m_selector = selector;
	}


	bool addThread(int (*fn)(void *), void *priv)
	{
		int cur = m_nThreads;

		m_nThreads++;
		m_threads = (IThread **)realloc(m_threads,
				m_nThreads * sizeof(IThread *));

		// Add the thread to the list
		m_threads[cur] = &IThread::createThread(*this, fn, priv);

		return true;
	}

	bool run()
	{
		bool should_quit = false;

		IPtrace &ptrace = IPtrace::getInstance();
		/* Fork the parent process of all test threads. All
		 * other threads will be threads running in this
		 * context.
		 *
		 * For each round in the test, this is forked again
		 * to retain the original memory state.
		 */
		int pid = ptrace.forkAndAttach();

		if (pid < 0) {
			error("fork failed\n");
			return false;
		} else if (pid == 0) {
			// Hmm... Not sure what to do here
			exit(0);
		}
		else {
			// Parent
			bool should_quit;

			// Setup function breakpoints
			for (functionMap_t::iterator it = m_functions.begin();
					it != m_functions.end(); it++) {
				IFunction *cur = it->second;

				if (cur->getSize() == 0)
					continue;

				int id = cur->setupEntryBreakpoint();

				if (id < 0)
					continue;

				m_functionBreakpoints[id] = cur;
			}

			// Select an initial thread and load its registers
			m_curThread = m_selector->selectThread(0, m_nThreads,
						getTimeStamp(m_startTimeStamp), NULL);

			void *regs = m_threads[m_curThread]->getRegs();
			IPtrace::getInstance().loadRegisters(pid, regs);

			m_startTimeStamp = getTimeStamp(0);

			do {
				should_quit = !runChild(pid);
			} while (!should_quit);
		}

		return true;
	}

	void setRuns(int nRuns)
	{
	}

	void setTimeLimit(int ms)
	{
	}

private:

	void cleanup()
	{
		for (int i = 0; i < m_nThreads; i++)
			IThread::releaseThread(*m_threads[i]);

		if (m_selector)
			delete m_selector;

		free(m_threads);
	}

	bool runChild(int pid)
	{
		IPtrace &ptrace = IPtrace::getInstance();

		while (1) {
			const IPtrace::PtraceEvent ev = ptrace.continueExecution(pid);
			int nextThread;

			switch (ev.type) {
			case ptrace_error:
			case ptrace_crash:
			case ptrace_exit:
				return false;

			case ptrace_breakpoint:
			case ptrace_syscall:
				// Step to next instruction
				ptrace.singleStep(pid);

				nextThread = m_selector->selectThread(m_curThread, m_nThreads,
						getTimeStamp(m_startTimeStamp), &ev);

				// Perform the actual thread switch
				if (nextThread != m_curThread) {
					ptrace.saveRegisters(pid, m_threads[m_curThread]->getRegs());
					ptrace.loadRegisters(pid, m_threads[nextThread]->getRegs());

					m_curThread = nextThread;
				}

				return true;

			default:
				return false;
			}
		}

		/* Unreachable */
		return false;
	}

	uint64_t getTimeStamp(uint64_t start)
	{
		struct timeval tv;

		gettimeofday(&tv, NULL);

		return (tv.tv_usec + tv.tv_sec * 1000 * 1000) - start;
	}

	typedef std::map<void *, IFunction *> functionMap_t;
	typedef std::map<int, IFunction *> functionBreakpointMap_t;


	int m_nThreads;
	IThread **m_threads;
	IThreadSelector *m_selector;
	int m_curThread;

	functionMap_t m_functions;
	functionBreakpointMap_t m_functionBreakpoints;

	uint64_t m_startTimeStamp;

	int m_runLimit;
};

IController &IController::getInstance()
{
	static Controller *instance;

	if (!instance)
		instance = new Controller();

	return *instance;
}
