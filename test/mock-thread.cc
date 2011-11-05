#include "test.hh"

#include <coincident/thread.hh>


class FakeThread
{
public:
	FakeThread() : m_blocked(false)
	{
	}

	void myBlock()
	{
		m_blocked = true;
	}

	void myUnBlock()
	{
		m_blocked = false;
	}

	bool isBlocked()
	{
		return m_blocked;
	}

	bool m_blocked;
};

class Thread : public IThread {
public:
	Thread(void (*exitHook)(),
			int (*fn)(void *), void *arg)
	{
		ON_CALL(*this, isBlocked())
			.WillByDefault(Invoke(&m_fake, &FakeThread::isBlocked));
		ON_CALL(*this, block())
			.WillByDefault(Invoke(&m_fake, &FakeThread::myBlock));
		ON_CALL(*this, unBlock())
			.WillByDefault(Invoke(&m_fake, &FakeThread::myUnBlock));

		EXPECT_CALL(*this, getRegs())
				.Times(AtLeast(1))
				.WillRepeatedly(Return((void *)m_regs))
				;

		EXPECT_CALL(*this, stepOverBreakpoint())
				.Times(AtLeast(1));
		EXPECT_CALL(*this, isBlocked())
			.Times(AnyNumber());
		EXPECT_CALL(*this, block())
			.Times(AnyNumber());
		EXPECT_CALL(*this, unBlock())
			.Times(AnyNumber());
	}

	MOCK_METHOD0(getRegs, void *());
	MOCK_METHOD0(stepOverBreakpoint, void());
	MOCK_METHOD0(saveRegisters, void());
	MOCK_METHOD1(getArgument,unsigned long(int n));
	MOCK_METHOD0(block, void());
	MOCK_METHOD0(unBlock, void());
	MOCK_METHOD0(isBlocked, bool());

	uint8_t m_regs[8];
	FakeThread m_fake;
};

#include "../src/thread.cc"
