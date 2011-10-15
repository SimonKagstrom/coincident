#pragma once

#include <ptrace.hh>
#include <stdint.h>

class IController
{
public:
	class IThreadSelector
	{
	public:
		virtual int selectThread(int curThread,
				int nThreads,
				uint64_t timeUs,
				const IPtrace::PtraceEvent *) = 0;
	};


	static IController &getInstance();

	virtual void setThreadSelector(IThreadSelector *selector) = 0;


	virtual bool addThread(int (*fn)(void *), void *priv) = 0;


	virtual bool run() = 0;

	virtual void setRuns(int nRuns) = 0;

	virtual void setTimeLimit(int ms) = 0;
};
