#pragma once

namespace coincident
{
	class Session;

	class IThread
	{
	public:
		friend class ThreadFactory;

		/**
		 * Return function call argument number @a n
		 *
		 * @param n the argument to return
		 *
		 * @return the argument value, or gibberish if called outside function
		 * entry point context, or on an argument which doesn't exist
		 */
		virtual unsigned long getArgument(int n) = 0;

		/**
		 * Return the return value for a function
		 *
		 * @return the return value. If the PC is not at the instruction after
		 * a call, this value will be gibberish.
		 */
		virtual unsigned long getReturnValue() = 0;

		virtual void setReturnValue(unsigned long what) = 0;

		virtual void stepOverBreakpoint() = 0;

		virtual void saveRegisters() = 0;

		virtual void loadRegisters() = 0;

		virtual void setPc(void *addr) = 0;

		virtual void *getPc() = 0;


		/**
		 * Do a backtrace for this thread. Simply because the libc backtrace()
		 * function has a very faktab interface.
		 */
		virtual int backtrace(unsigned long *buf, int maxValues) = 0;

		virtual void dumpRegs(char *buf) = 0;


		virtual void block() = 0;

		virtual void unBlock() = 0;

		virtual bool isBlocked() = 0;

	private:
		static IThread *createThread(void (*exitHook)(),
				int (*fn)(void *), void *arg);

		static void releaseThread(IThread *thread);
	};


	class ThreadFactory
	{
	public:
		static IThread &createThread(void (*exitHook)(),
				int (*fn)(void *), void *arg);

		static void releaseThread(IThread &thread);
	};
}
