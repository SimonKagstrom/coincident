#include "test.hh"

#include <thread.hh>

class Thread : public IThread {
public:
	Thread(void (*exitHook)(),
			int (*fn)(void *), void *arg)
	{
	}

	MOCK_METHOD0(getRegs, void *());
};

#include "../src/thread.cc"
