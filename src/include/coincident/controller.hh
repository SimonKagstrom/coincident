#pragma once

#include <stdint.h>

class PtraceEvent;
class IThread;

class IController
{
public:
	class IFunctionHandler
	{
	public:
		virtual bool handle(IThread *curThread, void *addr, const PtraceEvent &) = 0;
	};

	class IThreadSelector
	{
	public:
		virtual int selectThread(int curThread,
				IThread **threads,
				int nThreads,
				uint64_t timeUs,
				const PtraceEvent *) = 0;
	};


	static IController &getInstance();

	virtual void setThreadSelector(IThreadSelector *selector) = 0;


	virtual bool addThread(int (*fn)(void *), void *priv) = 0;


	virtual bool registerFunctionHandler(void *functionAddress,
			IFunctionHandler *handler) = 0;

	virtual bool unregisterFunctionHandler(void *functionAddress) = 0;


	/* --- Mutual exclusion --- */
	virtual int lockScheduler() = 0;

	virtual void unlockScheduler(int level) = 0;

	virtual IThread *getCurrentThread() = 0;


	virtual bool run() = 0;

	virtual void setRuns(int nRuns) = 0;

	virtual void setTimeLimit(int ms) = 0;
};
