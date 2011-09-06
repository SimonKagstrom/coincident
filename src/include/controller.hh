#pragma once

#include <ptrace.hh>
#include <stdint.h>

class IController
{
public:
	class IProcessSelector
	{
		virtual int selectProcess(int curProcess,
				uint64_t timeUs,
				IPtrace::PtraceEvent *) = 0;
	};


	static IController &getInstance();


	virtual bool addThread(int (*fn)(void *), void *priv) = 0;


	virtual bool run() = 0;

	virtual void setRuns(int nRuns) = 0;

	virtual void setTimeLimit(int ms) = 0;
};
