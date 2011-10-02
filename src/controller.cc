#include <controller.hh>
#include <ptrace.hh>
#include <utils.hh>

#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <map>

class DefaultProcessSelector : public IController::IProcessSelector
{
public:
	int selectProcess(int curProcess,
			int nProcesses,
			uint64_t timeUs,
			const IPtrace::PtraceEvent *ev)
	{
		return rand() % nProcesses;
	}
};


class Controller : public IController
{
public:
	Controller()
	{
		m_processes = NULL;

		m_selector = new DefaultProcessSelector();
		m_startTimeStamp = getTimeStamp(0);
	}

	~Controller()
	{
		cleanup();
	}

	void setProcessSelector(IProcessSelector *selector)
	{
		if (m_selector)
			delete m_selector;

		m_selector = selector;
	}


	bool addThread(int (*fn)(void *), void *priv)
	{
		int cur = m_nProcesses;

		m_nProcesses++;
		m_processes = (Process **)realloc(m_processes,
				m_nProcesses * sizeof(Process*));

		// Add the process to the list
		m_processes[cur] = new Process(fn, priv);

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
			// Child
			for (int i = 0; i < m_nProcesses; i++) {
				Process *cur = m_processes[i];
				int id = ptrace.cloneAndAttach(cur->m_fn, cur->m_priv, cur->m_stack);

				cur->setPid(id);
			}

			int exitCode = 0;

			do {
				pid_t which;
				int status;
				int i;

				which = waitpid(-1, &status, WEXITED);

				for (i = 0; i < m_nProcesses; i++) {
					if (m_processes[i]->getPid() == which)
						m_processes[i]->exit(status);
				}

				if (status > exitCode)
					exitCode = status;

				// Done if there are no processes left running
				for (i = 0; i < m_nProcesses; i++) {
					if (!m_processes[i]->hasExited())
						break;
				}
				if (i == m_nProcesses)
					break;
			} while (1);

			exit(exitCode);
		}
		else {
			bool should_quit;

			// Parent
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
	class Process
	{
	public:
		Process(int (*fn)(void *), void *priv) :
			m_fn(fn), m_priv(priv), m_pid(0), m_exitCode(-1)
		{
			/* Plenty of stack */
			size_t stack_sz = 8 * 1024 * 1024;
			m_stackStart = (uint8_t *)xmalloc(stack_sz);

			m_stack = (void *)(m_stackStart + stack_sz - 8);
		}

		~Process()
		{
			free(m_stackStart);
		}

		void exit(int status)
		{
			m_exitCode = status;
		}

		bool hasExited()
		{
			return m_exitCode != -1;
		}

		void setPid(int pid)
		{
			m_pid = pid;
		}

		pid_t getPid()
		{
			return m_pid;
		}

		int (*m_fn)(void *);
		void *m_priv;

		void *m_stack;
	private:
		pid_t m_pid;
		int m_exitCode;

		uint8_t *m_stackStart;
	};


	void cleanup()
	{
		for (int i = 0; i < m_nProcesses; i++)
			delete m_processes[i];

		if (m_selector)
			delete m_selector;

		free(m_processes);
	}

	bool runChild(int pid)
	{
		IPtrace &ptrace = IPtrace::getInstance();

		while (1) {
			const IPtrace::PtraceEvent ev = ptrace.continueExecution(pid);

			switch (ev.type) {
			case ptrace_error:
			case ptrace_crash:
			case ptrace_exit:
				return false;

			case ptrace_breakpoint:
			case ptrace_syscall:
				pid = m_selector->selectProcess(pid, m_nProcesses,
						getTimeStamp(m_startTimeStamp), &ev);
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

	int m_nProcesses;
	Process **m_processes;
	IProcessSelector *m_selector;

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
