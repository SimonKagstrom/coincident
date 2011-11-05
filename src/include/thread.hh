#pragma once

class IThread
{
public:
	// getRegs should only be used by Session
	friend class Session;

	/**
	 * Return function call argument number @a n
	 *
	 * @param n the argument to return
	 *
	 * @return the argument value, or gibberish if called outside function
	 * entry point context, or on an argument which doesn't exist
	 */
	virtual unsigned long getArgument(int n) = 0;

	virtual void stepOverBreakpoint() = 0;

	virtual void saveRegisters() = 0;


	virtual void block() = 0;

	virtual void unBlock() = 0;

	virtual bool isBlocked() = 0;


	static IThread &createThread(void (*exitHook)(),
			int (*fn)(void *), void *arg);

	static void releaseThread(IThread &thread);

private:
	virtual void *getRegs() = 0;
};
