#include <coincident/controller.hh>
#include <coincident/coincident.h>
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
			const PtraceEvent *ev)
	{
		return rand() % nThreads;
	}
};

class Controller : public IController, IElf::IFunctionListener
{
public:
	friend class Session;

	class ThreadData
	{
	public:
		ThreadData(int (*fn)(void *), void *priv) : m_fn(fn), m_priv(priv)
		{
		}

		int (*m_fn)(void *);
		void *m_priv;
	};

	Controller();

	virtual ~Controller();


	void onFunction(IFunction &fn);

	void setThreadSelector(IThreadSelector *selector);

	bool addThread(int (*fn)(void *), void *priv);

	int lockScheduler();

	void unlockScheduler(int level);

	bool run();

	void setRuns(int nRuns);

	void setTimeLimit(int ms);

	void cleanup();

	uint64_t getTimeStamp(uint64_t start);

	typedef std::map<void *, IFunction *> FunctionMap_t;
	typedef std::map<int, IFunction *> FunctionBreakpointMap_t;
	typedef std::map<void *, int> BreakpointMap_t;


	int m_nThreads;
	ThreadData *m_threads[N_THREADS];
	IThreadSelector *m_selector;

	FunctionMap_t m_functions;
	BreakpointMap_t m_breakpoints;

	uint64_t m_startTimeStamp;

	int m_schedulerLock;
	int m_runLimit;
	uint64_t m_timeLimit;
};

class Session
{
public:
	Session(Controller &owner, int nThreads, Controller::ThreadData **threads);

	~Session();

	void removeThread(int pid, int which);

	bool handleBreakpoint(const PtraceEvent &ev);

	bool continueExecution();

	bool run();


	// Thread exit handler (just a marker)
	static void threadExit();

	Controller &m_owner;
	int m_nThreads;
	int m_curPid;
	int m_curThread;

	IThread **m_threads;
};


Controller::Controller()
{
	memset(m_threads, 0, sizeof(m_threads));
	m_nThreads = 0;

	m_schedulerLock = 0;

	m_runLimit = 0;
	m_timeLimit = 0;

	m_selector = new DefaultThreadSelector();
	m_startTimeStamp = getTimeStamp(0);

	IElf &elf = IElf::getInstance();

	elf.setFile(this, "/proc/self/exe");


	// Setup function breakpoints
	for (FunctionMap_t::iterator it = m_functions.begin();
			it != m_functions.end(); it++) {
		IFunction *cur = it->second;

		if (cur->getSize() == 0 ||
				cur->getEntry() == 0)
			continue;

		m_breakpoints[cur->getEntry()] = 1;
	}
}

Controller::~Controller()
{
	cleanup();
}


void Controller::onFunction(IFunction &fn)
{
	m_functions[fn.getEntry()] = &fn;
}

void Controller::setThreadSelector(IThreadSelector *selector)
{
	if (m_selector)
		delete m_selector;

	m_selector = selector;
}


bool Controller::addThread(int (*fn)(void *), void *priv)
{
	int cur = m_nThreads;

	m_nThreads++;

	// Add the thread to the list
	m_threads[cur] = new ThreadData(fn, priv);

	return true;
}


int Controller::lockScheduler()
{
	int out = m_schedulerLock;

	m_schedulerLock++;

	return out;
}

void Controller::unlockScheduler(int level)
{
	m_schedulerLock = level;
}

bool Controller::run()
{
	int runsLeft = -1;
	bool out;

	if (m_runLimit)
		runsLeft = m_runLimit;

	m_startTimeStamp = getTimeStamp(0);

	while (1) {
		Session cur(*this, m_nThreads, m_threads);

		out = cur.run();
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

void Controller::setRuns(int nRuns)
{
	m_runLimit = nRuns;
}

void Controller::setTimeLimit(int ms)
{
	m_timeLimit = ms * 1000;
}

void Controller::cleanup()
{
	if (m_selector)
		delete m_selector;
}

uint64_t Controller::getTimeStamp(uint64_t start)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return (tv.tv_usec + tv.tv_sec * 1000 * 1000) - start;
}



Session::Session(Controller &owner, int nThreads, Controller::ThreadData **threads) :
						m_owner(owner), m_nThreads(nThreads)
{
	m_threads = new IThread*[m_nThreads];
	m_curThread = 0;
	m_curPid = 0;

	for (int i = 0; i < m_nThreads; i++) {
		Controller::ThreadData *p = threads[i];

		m_threads[i] = &IThread::createThread(Session::threadExit,
				p->m_fn, p->m_priv);
	}
}

Session::~Session()
{
	for (int i = 0; i < m_nThreads; i++)
		IThread::releaseThread(*m_threads[i]);

	delete[] m_threads;
}

// Thread exit handler (just a marker)
void Session::threadExit()
{
}


void Session::removeThread(int pid, int which)
{
	if (m_nThreads < 1)
		return;

	IThread::releaseThread(*m_threads[which]);

	// Swap threads
	if (which != m_nThreads)
		m_threads[which] = m_threads[m_nThreads - 1];

	m_nThreads--;

	if (which == m_curThread || m_curThread >= m_nThreads)
		m_curThread = 0;

	if (m_nThreads > 0)
		IPtrace::getInstance().loadRegisters(pid, m_threads[0]->getRegs());
}

bool Session::handleBreakpoint(const PtraceEvent &ev)
{
	IFunction *function = m_owner.m_functions[ev.addr];
	IPtrace &ptrace = IPtrace::getInstance();

	// Step to next instruction
	ptrace.singleStep(m_curPid);

	if (function && function->getEntry() == (void *)Session::threadExit) {
		removeThread(m_curPid, m_curThread);

		if (m_nThreads == 0)
			return true;
		// Re-select the thread
	} else if (function) {
		// Visited a function for the first time, setup breakpoints
		if (ptrace.clearBreakpoint(ev.eventId) == false)
			error("Can't clear function breakpoint???");

		m_owner.m_breakpoints.erase(function->getEntry());

		std::list<void *> refs = function->getMemoryRefs();

		for (std::list<void *>::iterator it = refs.begin();
				it != refs.end(); it++) {
			if (ptrace.setBreakpoint(*it) < 0)
				error("Can't set breakpoint???");

			m_owner.m_breakpoints[*it] = 1;
		}

		return true;
	}

	// No reschedules if this is set
	if (m_owner.m_schedulerLock)
		return true;

	int nextThread;

	nextThread = m_owner.m_selector->selectThread(m_curThread, m_nThreads,
			m_owner.getTimeStamp(m_owner.m_startTimeStamp), &ev);

	// Perform the actual thread switch
	if (nextThread != m_curThread) {
		ptrace.saveRegisters(m_curPid, m_threads[m_curThread]->getRegs());
		ptrace.loadRegisters(m_curPid, m_threads[nextThread]->getRegs());

		m_curThread = nextThread;
	}

	return true;
}

bool Session::continueExecution()
{
	const PtraceEvent ev = IPtrace::getInstance().continueExecution(m_curPid);

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

bool Session::run()
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

		for (Controller::BreakpointMap_t::iterator it = m_owner.m_breakpoints.begin();
				it != m_owner.m_breakpoints.end(); it++) {
			void *p = it->first;

			int id = ptrace.setBreakpoint(p);
			if (id < 0)
				error("Can't set breakpoint!\n");
		}

		// Select an initial thread and load its registers
		m_curThread = m_owner.m_selector->selectThread(0, m_nThreads,
				m_owner.getTimeStamp(m_owner.m_startTimeStamp), NULL);

		void *regs = m_threads[m_curThread]->getRegs();
		IPtrace::getInstance().loadRegisters(m_curPid, regs);

		do {
			should_quit = !continueExecution();

			// Quit if all threads have exited cleanly
			if (m_nThreads == 0)
				break;
		} while (!should_quit);

		ptrace.kill(m_curPid);
		m_curPid = -1;
	}

	return m_nThreads == 0;
}



IController &IController::getInstance()
{
	static Controller *instance;

	if (!instance)
		instance = new Controller();

	return *instance;
}


int coincident_add_thread(int (*fn)(void *), void *priv)
{
	if (IController::getInstance().addThread(fn, priv) == false)
		return -1;

	// FIXME: Should return the thread ID!
	return 0;
}

void coincident_set_run_limit(int n_runs)
{
	IController::getInstance().setRuns(n_runs);
}

void coincident_set_time_limit(int n_ms)
{
	IController::getInstance().setTimeLimit(n_ms);
}

int coincident_run(void)
{
	if (IController::getInstance().run() == false)
		return 1;

	return 0;
}
