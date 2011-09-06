#include <controller.hh>
#include <ptrace.hh>
#include <utils.hh>

#include <stdlib.h>
#include <map>

class Controller : public IController
{
public:
	Controller()
	{
		m_processes = NULL;
	}

	~Controller()
	{
		cleanup();
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

		while (!should_quit) {
			/* Fork the parent process of all test threads. All
			 * other threads will be threads running in this
			 * context.
			 *
			 * For each round in the test, this is forked again
			 * to retain the original memory state.
			 */
			int pid = IPtrace::getInstance().forkAndAttach();

			if (pid < 0) {
				error("fork failed\n");
				return false;
			} else if (pid == 0) {
				// Child
			}
			else {
				// Parent
				runChild(pid);
			}

			// FIXME: Set should_quit...
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
			m_fn(fn), m_priv(priv)
		{
		}

		void setPid(int pid)
		{
			m_pid = pid;
		}

		int (*m_fn)(void *);
		void *m_priv;

		int m_pid;
	};


	void cleanup()
	{
		for (int i = 0; i < m_nProcesses; i++)
			delete m_processes[i];

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
				break;
			}
		}

		return true;
	}

	int m_nProcesses;
	Process **m_processes;

	int m_runLimit;

};

IController &IController::getInstance()
{
	static Controller *instance;

	if (!instance)
		instance = new Controller();

	return *instance;
}
