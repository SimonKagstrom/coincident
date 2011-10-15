#pragma once

class IThread
{
public:
	class IThreadExitListener
	{
	public:
		virtual void threadExit(IThread &thread) = 0;
	};

	virtual void *getRegs() = 0;

	static IThread &createThread(IThreadExitListener &listener,
			int (*fn)(void *), void *arg);

	static void releaseThread(IThread &thread);
};
