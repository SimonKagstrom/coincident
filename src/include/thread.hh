#pragma once

class IThread
{
public:
	// getRegs should only be used by Session
	friend class Session;

	static IThread &createThread(void (*exitHook)(),
			int (*fn)(void *), void *arg);

	static void releaseThread(IThread &thread);

private:
	virtual void *getRegs() = 0;
};
