#include <coincident/controller.hh>
#include <coincident/coincident.h>
#include <coincident/thread.hh>
#include <ptrace.hh>
#include <apis.hh>
#include <utils.hh>
#include <elf.hh>
#include <stdarg.h>
#include <function.hh>

#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <map>
#include <string>

using namespace coincident;

#define N_THREADS 16

class DefaultThreadSelector : public IController::IThreadSelector
{
public:
	int selectThread(int curThread,
			IThread **threads,
			int nThreads,
			uint64_t timeUs,
			const PtraceEvent *ev)
	{
		return rand() % nThreads;
	}
};

class TimeListSelector : public IController::IThreadSelector
{
public:
	TimeListSelector(int *entries, int numEntries)
	{
		m_entries = numEntries;

		m_buckets = new int[numEntries];
		memcpy(m_buckets, entries, m_entries * sizeof(int));

		m_curBucket = 0;
		m_curTimeLeft = m_buckets[m_curBucket];
	}

	virtual ~TimeListSelector()
	{
		delete[] m_buckets;
	}

	int selectThread(int curThread,
			IThread **threads,
			int nThreads,
			uint64_t timeUs,
			const PtraceEvent *ev)
	{
		bool switchThread = false;

		if (m_curTimeLeft <= 0) {
			m_curBucket = (m_curBucket + 1) % m_entries;
			m_curTimeLeft = m_buckets[m_curBucket];

			switchThread = true;
		}
		if (curThread >= nThreads) {
			curThread = 0;
			switchThread = true;
		}

		m_curTimeLeft--;

		if (switchThread)
			curThread = (curThread + 1) % nThreads;

		return curThread;
	}

private:
	int *m_buckets;
	int m_entries;
	int m_curTimeLeft;
	int m_curBucket;
};

namespace coincident
{
	class Session;
}

class Controller : public IController, IElf::IFunctionListener
{
public:
	friend class coincident::Session;

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

	IThread *getCurrentThread();

	void forceReschedule();

	bool run();

	void setRuns(int nRuns);

	void setTimeLimit(int ms);

	void cleanup();

	bool registerFunctionHandler(void *functionAddress,
			IFunctionHandler *handler);

	bool unregisterFunctionHandler(void *functionAddress);


	uint64_t getTimeStamp(uint64_t start);

	void reportError(const char *fmt, ...);

	const char *getError();

	typedef std::map<void *, IFunction *> FunctionMap_t;
	typedef std::map<void *, IFunctionHandler *> FunctionHandlerMap_t;
	typedef std::map<int, IFunction *> FunctionBreakpointMap_t;
	typedef std::map<void *, int> BreakpointMap_t;


	int m_nThreads;
	ThreadData *m_threads[N_THREADS];
	IThreadSelector *m_selector;

	FunctionMap_t m_functions;
	FunctionHandlerMap_t m_functionHandlers;
	BreakpointMap_t m_breakpoints;

	uint64_t m_startTimeStamp;

	int m_schedulerLock;
	int m_runLimit;
	uint64_t m_timeLimit;

	std::string m_error;

	IElf *m_elf;

	// Valid while non-NULL
	Session *m_curSession;
};

class coincident::Session : public Controller::IFunctionHandler
{
public:
	Session(Controller &owner, int nThreads, Controller::ThreadData **threads);

	virtual ~Session();

	void removeThread(int pid, int which);

	bool handleBreakpoint(const PtraceEvent &ev);

	bool continueExecution();

	void switchThread(const PtraceEvent &ev);

	void releaseLastThread();

	bool run();

	std::string backtraceToString(unsigned long *buf, int nValues);

	class ExitHandler : public Controller::IFunctionHandler
	{
	public:
		ExitHandler(Session &owner);

		bool handle(IThread *cur, void *addr, const PtraceEvent &ev);
	private:
		Session &m_owner;
	};

	// Default handler
	bool handle(IThread *cur, void *addr, const PtraceEvent &ev);

	// Thread exit handler (just a marker)
	static void threadExit();

	Controller &m_owner;

	ExitHandler m_exitHandler;
	int m_nThreads;
	int m_curPid;
	int m_curThread;
	bool m_lastThreadLoose;

	IThread **m_threads;
};


Controller::Controller()
{
	coin_set_cpu(getpid(), 0);

	memset(m_threads, 0, sizeof(m_threads));
	m_nThreads = 0;

	m_schedulerLock = 0;

	m_runLimit = 0;
	m_timeLimit = 0;

	m_selector = new DefaultThreadSelector();
	m_startTimeStamp = getTimeStamp(0);

	m_curSession = NULL;

	m_elf = IElf::open("/proc/self/exe");
	panic_if (!m_elf,
			"Can't open executable");

	m_elf->parse(this);


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


bool Controller::registerFunctionHandler(void *functionAddress,
			IFunctionHandler *handler)
{
	if (m_functions.find(functionAddress) == m_functions.end())
		return false;

	IFunction *fn = m_functions[functionAddress];
	IElf::FunctionList_t allFunctions = m_elf->functionByName(fn->getName());

	for (IElf::FunctionList_t::iterator it = allFunctions.begin();
			it != allFunctions.end(); it++) {
		IFunction *cur = *it;

		m_functionHandlers[cur->getEntry()] = handler;
	}

	m_functionHandlers[functionAddress] = handler;

	return true;
}

bool Controller::unregisterFunctionHandler(void *functionAddress)
{
	if (m_functionHandlers.find(functionAddress) == m_functionHandlers.end())
		return false;

	m_functionHandlers.erase(functionAddress);

	return true;
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

		m_curSession = &cur;
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
	m_curSession = NULL;

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

IThread *Controller::getCurrentThread()
{
	if (!m_curSession)
		return NULL;

	if (m_curSession->m_nThreads == 0)
		return NULL;

	return m_curSession->m_threads[m_curSession->m_curThread];
}

void Controller::forceReschedule()
{
	if (!m_curSession)
		return;

	PtraceEvent ev;

	// Fake something
	ev.type = ptrace_breakpoint;
	ev.addr = NULL;
	ev.eventId = 0;

	m_curSession->switchThread(ev);
}

void Controller::reportError(const char *fmt, ...)
{
	int n, size = 1024;
	std::string str;
	va_list ap;

	panic_if (!m_curSession,
			"Error reported but coincident not running");

	str.resize(size);
	va_start(ap, fmt);
	n = vsnprintf((char *)str.c_str(), size, fmt, ap);
	va_end(ap);

	panic_if (n < 0, "Too long error description");

	m_error = str;
}

const char *Controller::getError()
{
	if (m_error.empty())
		return NULL;

	return m_error.c_str();
}



Session::Session(Controller &owner, int nThreads, Controller::ThreadData **threads) :
						m_owner(owner), m_nThreads(nThreads), m_exitHandler(*this)
{
	m_threads = new IThread*[m_nThreads];
	m_curThread = 0;
	m_curPid = 0;
	m_lastThreadLoose = false;

	for (int i = 0; i < m_nThreads; i++) {
		Controller::ThreadData *p = threads[i];

		m_threads[i] = &ThreadFactory::createThread(Session::threadExit,
				p->m_fn, p->m_priv);
	}

	m_owner.registerFunctionHandler((void *)Session::threadExit,
			&m_exitHandler);
}

Session::~Session()
{
	for (int i = 0; i < m_nThreads; i++)
		ThreadFactory::releaseThread(*m_threads[i]);

	delete[] m_threads;

	m_owner.unregisterFunctionHandler((void *)Session::threadExit);
}

// Thread exit handler (just a marker)
void Session::threadExit()
{
}


void Session::removeThread(int pid, int which)
{
	if (m_nThreads < 1)
		return;

	ThreadFactory::releaseThread(*m_threads[which]);

	// Swap threads
	if (which != m_nThreads)
		m_threads[which] = m_threads[m_nThreads - 1];

	m_nThreads--;

	if (which == m_curThread || m_curThread >= m_nThreads)
		m_curThread = 0;

	if (m_nThreads > 0)
		m_threads[0]->loadRegisters();
}

Session::ExitHandler::ExitHandler(Session &owner) : m_owner(owner)
{
}

bool Session::ExitHandler::handle(IThread *cur, void *addr, const PtraceEvent &ev)
{
	m_owner.removeThread(m_owner.m_curPid, m_owner.m_curThread);

	coin_debug(BP_MSG, "Thread %p exited\n", cur);

	if (m_owner.m_nThreads == 0)
		return false;

	m_owner.switchThread(ev);

	return true;
}

bool Session::handle(IThread *cur, void *addr, const PtraceEvent &ev)
{
	IFunction *function = m_owner.m_functions[addr];
	IPtrace &ptrace = IPtrace::getInstance();

	m_threads[m_curThread]->stepOverBreakpoint();
	// Visited a function for the first time, setup breakpoints
	if (ptrace.clearBreakpoint(ev.eventId) == false) {
		error("Can't clear function breakpoint???");

		return false;
	}

	m_owner.m_breakpoints.erase(function->getEntry());
	// Don't setup breakpoints in libraries
	if (function->getType() == IFunction::SYM_DYNAMIC)
		return true;
	coin_debug(BP_MSG, "BP visited %s at %p\n",
			function->getName(), function->getEntry());

	IFunction::ReferenceList_t refs = function->getMemoryStores();

	for (IFunction::ReferenceList_t::iterator it = refs.begin();
			it != refs.end(); it++) {
		coin_debug(BP_MSG, "BP set at %p\n", *it);
		if (ptrace.setBreakpoint(*it) < 0)
			error("Can't set breakpoint???");

		m_owner.m_breakpoints[*it] = 1;
	}

	return true;
}


bool Session::handleBreakpoint(const PtraceEvent &ev)
{
	IFunction *function = m_owner.m_functions[ev.addr];

	m_threads[m_curThread]->saveRegisters();

	if (function) {
		// Assume default handler
		IFunctionHandler *handler = this;

		Controller::FunctionHandlerMap_t::iterator it = m_owner.m_functionHandlers.find(ev.addr);
		if (it != m_owner.m_functionHandlers.end())
			handler = it->second;

		return handler->handle(m_threads[m_curThread], ev.addr, ev);
	}

	// Step to next instruction
	m_threads[m_curThread]->stepOverBreakpoint();

	// No reschedules if this is set
	if (m_owner.m_schedulerLock)
		return true;

	switchThread(ev);

	return true;
}

void Session::switchThread(const PtraceEvent &ev)
{
	IPtrace &ptrace = IPtrace::getInstance();
	int nextThread;
	IThread *threads[m_nThreads];
	int unblocked = 0;
	int cur = -1;

	// Filter out unblocked threads
	for (int i = 0; i < m_nThreads; i++) {
		if (m_threads[i]->isBlocked())
			continue;

		threads[unblocked] = m_threads[i];

		// Might not be the same
		if (m_curThread == i)
			cur = unblocked;

		unblocked++;
	}

	nextThread = m_owner.m_selector->selectThread(cur,
			threads, unblocked,
			m_owner.getTimeStamp(m_owner.m_startTimeStamp), &ev);

	// Perform the actual thread switch
	if (nextThread != cur) {

		// Convert back to the blocked thread numbers
		for (int i = 0; i < m_nThreads; i++) {
			if (m_threads[i] == threads[nextThread]) {
				nextThread = i;
				break;
			}
		}

		m_curThread = nextThread;
	}
}

void Session::releaseLastThread()
{
	if (m_lastThreadLoose)
		return;

	IPtrace &ptrace = IPtrace::getInstance();

	ptrace.clearAllBreakpoints();
	ptrace.setBreakpoint((void *)Session::threadExit);

	m_lastThreadLoose = true;
}

std::string Session::backtraceToString(unsigned long *buf, int nValues)
{
	char str[20 * nValues];
	char *p = str;
	std::string out;

	if (nValues == 0)
		return std::string("");

	for (int i = 0; i < nValues - 1; i++)
		p += sprintf(p, "0x%08lx -> ", buf[i]);

	sprintf(p, "0x%08lx -> ", buf[nValues - 1]);

	return std::string(str);
}

bool Session::continueExecution()
{
	m_threads[m_curThread]->loadRegisters();
	const PtraceEvent ev = IPtrace::getInstance().continueExecution();

	coin_debug(PTRACE_MSG, "PT event at %p: id/type: 0x%08x/%d\n",
			ev.addr, ev.eventId, ev.type);

	switch (ev.type) {
	case ptrace_error:
	case ptrace_crash:
	{
		char regs[1024];
		unsigned long buf[8];
		int n = m_threads[m_curThread]->backtrace(buf, 8);

		m_threads[m_curThread]->dumpRegs(regs);
		coin_debug(PTRACE_MSG, "PT error at %p. backtrace %s\n%s",
				ev.addr, backtraceToString(buf, n).c_str(), regs);
		m_owner.reportError("ptrace %s at %p (backtrace %s)\n%s",
				ev.type == ptrace_error ? "error" : "crash",
				ev.addr,
				backtraceToString(buf, n).c_str(),
				regs);
		return false;
	}

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
		m_curThread = m_owner.m_selector->selectThread(0, m_threads, m_nThreads,
				m_owner.getTimeStamp(m_owner.m_startTimeStamp), NULL);

		do {
			should_quit = !continueExecution();

			if (!m_owner.m_error.empty())
				break;

			// Quit if all threads have exited cleanly
			if (m_nThreads == 0)
				break;

			// If there is only one thread left, let it loose to
			// improve performance
			if (m_owner.m_nThreads - m_nThreads == 1)
				releaseLastThread();
		} while (!should_quit);

		ptrace.kill();
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

void coincident_set_bucket_selector(int *buckets, unsigned int n_buckets)
{
	IController::getInstance().setThreadSelector(new TimeListSelector(buckets, n_buckets));
}

int coincident_run(void)
{
	if (IController::getInstance().run() == false)
		return 1;

	return 0;
}

int g_coin_debug_mask;
void coincident_set_debug_mask(int mask)
{
	g_coin_debug_mask = mask;
}

void coincident_init(void)
{
	// Loads the ELF symbols
	IController::getInstance();

	coincident_api_init_pthreads();
}
