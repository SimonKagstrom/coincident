#include <controller.hh>
#include <ptrace.hh>
#include <utils.hh>
#include <thread.hh>

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


class Controller : public IController, IThread::IThreadExitListener
{
public:
	Controller()
	{
		m_threads = NULL;

		m_selector = new DefaultThreadSelector();
		m_startTimeStamp = getTimeStamp(0);
	}

	virtual ~Controller()
	{
		cleanup();
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

			// Select an initial thread and load its registers
			int thread = m_selector->selectThread(0, m_nThreads,
						getTimeStamp(m_startTimeStamp), NULL);

			void *regs = m_threads[thread]->getRegs();
			IPtrace::getInstance().loadRegisters(pid, regs);

			m_startTimeStamp = getTimeStamp(0);

			do {
				should_quit = !runChild(pid, thread);
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

	bool runChild(int pid, int thread)
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
				nextThread = m_selector->selectThread(thread, m_nThreads,
						getTimeStamp(m_startTimeStamp), &ev);

				// Perform the actual thread switch
				if (nextThread != thread) {
					IPtrace::getInstance().saveRegisters(pid,
							m_threads[thread]->getRegs());
					IPtrace::getInstance().loadRegisters(pid,
							m_threads[nextThread]->getRegs());

					thread = nextThread;
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

	int m_nThreads;
	IThread **m_threads;
	IThreadSelector *m_selector;

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
