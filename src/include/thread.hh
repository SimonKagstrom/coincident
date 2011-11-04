#pragma once

class IThread
{
public:
	// getRegs should only be used by Session
	friend class Session;

	virtual void stepOverBreakpoint() = 0;

	virtual void saveRegisters() = 0;

	static IThread &createThread(void (*exitHook)(),
			int (*fn)(void *), void *arg);

	static void releaseThread(IThread &thread);

private:
	virtual void *getRegs() = 0;
};
