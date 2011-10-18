#pragma once

class IThread
{
public:
	virtual void *getRegs() = 0;

	static IThread &createThread(void (*exitHook)(),
			int (*fn)(void *), void *arg);

	static void releaseThread(IThread &thread);
};
