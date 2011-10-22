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

#define N_THREADS 16

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


class Controller : public IController, IElf::IFunctionListener
{
public:
	Controller()
	{
		memset(m_threads, 0, sizeof(m_threads));
		m_nActiveThreads = 0;
		m_nThreads = 0;
		m_curThread = 0;

		m_schedulerLock = 0;

		m_runLimit = 0;
		m_timeLimit = 0;

		m_selector = new DefaultThreadSelector();
		m_startTimeStamp = getTimeStamp(0);

		IElf &elf = IElf::getInstance();

		elf.setFile(this, "/proc/self/exe");


		// Setup function breakpoints
		for (functionMap_t::iterator it = m_functions.begin();
				it != m_functions.end(); it++) {
			IFunction *cur = it->second;

			if (cur->getSize() == 0)
				continue;

			m_breakpoints[cur->getEntry()] = 1;
		}
	}

	virtual ~Controller()
	{
		cleanup();
	}


	void onFunction(IFunction &fn)
	{
		m_functions[fn.getEntry()] = &fn;
	}

	// Thread exit handler (just a marker)
	static void threadExit()
	{
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
		m_nActiveThreads++;

		// Add the thread to the list
		m_threads[cur] = &IThread::createThread(threadExit, fn, priv);

		return true;
	}


	int lockScheduler()
	{
		int out = m_schedulerLock;

		m_schedulerLock++;

		return out;
	}

	void unlockScheduler(int level)
	{
		m_schedulerLock = level;
	}



	bool forkAndRunOneRound()
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
		m_curPid = ptrace.forkAndAttach();

		if (m_curPid < 0) {
			error("fork failed\n");
			return false;
		} else if (m_curPid == 0) {
			// Hmm... Not sure what to do here
			exit(0);
		}
		else {
			// Parent
			bool should_quit;

			m_nActiveThreads = m_nThreads;

			for (breakpointMap_t::iterator it = m_breakpoints.begin();
					it != m_breakpoints.end(); it++) {
				void *p = it->first;

				ptrace.setBreakpoint(p);
			}

			// Select an initial thread and load its registers
			m_curThread = m_selector->selectThread(0, m_nActiveThreads,
						getTimeStamp(m_startTimeStamp), NULL);

			void *regs = m_threads[m_curThread]->getRegs();
			IPtrace::getInstance().loadRegisters(m_curPid, regs);

			do {
				should_quit = !continueExecution();

				// Quit if all threads have exited cleanly
				if (m_nActiveThreads == 0)
					break;
			} while (!should_quit);

			ptrace.kill(m_curPid);
			m_curPid = -1;
		}

		return true;
	}

	bool run()
	{
		int runsLeft = -1;
		bool out;

		if (m_runLimit)
			runsLeft = m_runLimit;

		m_startTimeStamp = getTimeStamp(0);

		while (1) {
			out = forkAndRunOneRound();
			if (!out)
				break;

			if (runsLeft > 0) {
				runsLeft--;

				if (runsLeft == 0)
					break;
			}

			if (m_timeLimit &&
					getTimeStamp(m_startTimeStamp) > m_timeLimit)
				break;
		}

		return out;
	}

	void setRuns(int nRuns)
	{
		m_runLimit = nRuns;
	}

	void setTimeLimit(int ms)
	{
		m_timeLimit = ms * 1000;
	}

	void removeThread(int pid, int which)
	{
		if (m_nActiveThreads < 1)
			return;

		IThread::releaseThread(*m_threads[which]);

		// Swap threads
		if (which != m_nActiveThreads)
			m_threads[which] = m_threads[m_nActiveThreads - 1];

		m_nActiveThreads--;

		if (which == m_curThread || m_curThread >= m_nActiveThreads)
			m_curThread = 0;

		if (m_nActiveThreads > 0)
			IPtrace::getInstance().loadRegisters(pid, m_threads[0]->getRegs());
	}


	void cleanup()
	{
		for (int i = 0; i < m_nActiveThreads; i++)
			IThread::releaseThread(*m_threads[i]);

		if (m_selector)
			delete m_selector;
	}

	bool handleBreakpoint(const IPtrace::PtraceEvent &ev)
	{
		IFunction *function = m_functions[ev.addr];
		IPtrace &ptrace = IPtrace::getInstance();

		// Step to next instruction
		ptrace.singleStep(m_curPid);

		if (function && function->getEntry() == (void *)threadExit) {
			removeThread(m_curPid, m_curThread);

			if (m_nActiveThreads == 0)
				return true;
			// Re-select the thread
		} else if (function) {
			// Visited a function for the first time, setup breakpoints
			if (ptrace.clearBreakpoint(ev.eventId) == false)
				error("Can't clear function breakpoint???");

			function->setupMemoryBreakpoints();

			return true;
		}

		// No reschedules if this is set
		if (m_schedulerLock)
			return true;

		int nextThread;

		nextThread = m_selector->selectThread(m_curThread, m_nActiveThreads,
				getTimeStamp(m_startTimeStamp), &ev);

		// Perform the actual thread switch
		if (nextThread != m_curThread) {
			ptrace.saveRegisters(m_curPid, m_threads[m_curThread]->getRegs());
			ptrace.loadRegisters(m_curPid, m_threads[nextThread]->getRegs());

			m_curThread = nextThread;
		}

		return true;
	}

	bool continueExecution()
	{
		const IPtrace::PtraceEvent ev = IPtrace::getInstance().continueExecution(m_curPid);

		switch (ev.type) {
		case ptrace_error:
		case ptrace_crash:
		case ptrace_exit:
			return false;

		case ptrace_syscall:
			return false;

		case ptrace_breakpoint:
			return handleBreakpoint(ev);
		}

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
	typedef std::map<void *, int> breakpointMap_t;


	int m_nActiveThreads;
	int m_nThreads;
	IThread *m_threads[N_THREADS];
	IThreadSelector *m_selector;
	int m_curThread;

	int m_curPid;

	functionMap_t m_functions;
	breakpointMap_t m_breakpoints;

	uint64_t m_startTimeStamp;

	int m_schedulerLock;
	int m_runLimit;
	uint64_t m_timeLimit;
};

IController &IController::getInstance()
{
	static Controller *instance;

	if (!instance)
		instance = new Controller();

	return *instance;
}
