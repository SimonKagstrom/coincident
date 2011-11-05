#include "test.hh"

#include <thread.hh>

class Thread : public IThread {
public:
	Thread(void (*exitHook)(),
			int (*fn)(void *), void *arg)
	{
		EXPECT_CALL(*this, getRegs())
				.Times(AtLeast(1))
				.WillRepeatedly(Return((void *)m_regs))
				;
	}

	MOCK_METHOD0(getRegs, void *());
	MOCK_METHOD0(stepOverBreakpoint, void());
	MOCK_METHOD0(saveRegisters, void());
	MOCK_METHOD1(getArgument,unsigned long(int n));
	MOCK_METHOD0(block, void());
	MOCK_METHOD0(unBlock, void());
	MOCK_METHOD0(isBlocked, bool());

	uint8_t m_regs[8];
};

#include "../src/thread.cc"
