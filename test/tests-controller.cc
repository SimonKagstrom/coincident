#include "test.hh"

#include "../src/controller.cc"
#include "mock-ptrace.hh"

static int test_thread(void *priv)
{
	return 0;
}

class MockThreadSelector : public Controller::IThreadSelector
{
public:
	MOCK_METHOD4(selectThread, int(int curThread,
			int nThreads,
			uint64_t timeUs,
			const IPtrace::PtraceEvent *));
};

TEST(controllerForkError, DEADLINE_REALTIME_MS(10000))
{
	IController &controller = IController::getInstance();
	MockPtrace &ptrace = (MockPtrace &)IPtrace::getInstance();

	EXPECT_CALL(ptrace, forkAndAttach())
		.Times(AtLeast(1))
		.WillRepeatedly(Return(-1));

	controller.run();
}

TEST(controllerAddProcessForkError, DEADLINE_REALTIME_MS(10000))
{
	IController &controller = IController::getInstance();
	MockPtrace &ptrace = (MockPtrace &)IPtrace::getInstance();

	EXPECT_CALL(ptrace, forkAndAttach())
		.Times(AtLeast(1))
		.WillRepeatedly(Return(-1));

	controller.addThread(test_thread, NULL);
	controller.run();
}


TEST(controllerRunChild, DEADLINE_REALTIME_MS(10000))
{
	IController &controller = IController::getInstance();
	MockPtrace &ptrace = (MockPtrace &)IPtrace::getInstance();

	EXPECT_CALL(ptrace, forkAndAttach())
		.Times(AtLeast(1))
		.WillRepeatedly(Return(100));

	IPtrace::PtraceEvent ev, evDefault;

	evDefault.type = ptrace_exit;
	evDefault.eventId = 0;

	ev.type = ptrace_breakpoint;
	ev.eventId = 100;
	ev.addr = NULL;

	EXPECT_CALL(ptrace, continueExecution(_))
		.Times(AtLeast(1))
		.WillOnce(Return(ev))
		.WillRepeatedly(Return(evDefault));

	EXPECT_CALL(ptrace, singleStep(_))
		.Times(AtLeast(1));

	// Add two threads
	controller.addThread(test_thread, NULL);
	controller.addThread(test_thread, NULL);

	controller.run();
}

TEST(controllerTestThreadRemoval, DEADLINE_REALTIME_MS(10000))
{
	Controller &controller = (Controller &)IController::getInstance();
	MockPtrace &ptrace = (MockPtrace &)IPtrace::getInstance();

	IPtrace::PtraceEvent ev;

	EXPECT_CALL(ptrace, singleStep(_))
		.Times(AtLeast(1));


	ev.addr = (void *)Controller::threadExit;
	ev.eventId = 10;
	ev.type = ptrace_breakpoint;

	controller.addThread(test_thread, NULL);
	controller.addThread(test_thread, NULL);

	ASSERT_EQ(controller.m_nActiveThreads, 2);

	// Will remove thread since it exited
	controller.handleBreakpoint(ev);
	ASSERT_EQ(controller.m_nActiveThreads, 1);
}

TEST(controllerThreadScheduling, DEADLINE_REALTIME_MS(10000))
{
	Controller &controller = (Controller &)IController::getInstance();
	MockPtrace &ptrace = (MockPtrace &)IPtrace::getInstance();

	IPtrace::PtraceEvent ev;

	ev.type = ptrace_breakpoint;
	ev.eventId = 100;
	ev.addr = (void *)(((unsigned long)test_thread) + 1); // No function!

	EXPECT_CALL(ptrace, continueExecution(_))
		.Times(AtLeast(1))
		.WillRepeatedly(Return(ev));

	EXPECT_CALL(ptrace, singleStep(_))
		.Times(AtLeast(1));

	// Add two threads
	controller.addThread(test_thread, NULL);
	controller.addThread(test_thread, NULL);

	MockThreadSelector selector;

	controller.setThreadSelector(&selector);
	EXPECT_CALL(selector, selectThread(_,_,_,_))
		.Times(Exactly(1))
		.WillOnce(Return(1));

	EXPECT_CALL(ptrace, saveRegisters(_,_))
		.Times(AtLeast(1));
	EXPECT_CALL(ptrace, loadRegisters(_,_))
		.Times(AtLeast(1));

	ASSERT_EQ(controller.m_curThread, 0);
	controller.continueExecution();
	ASSERT_EQ(controller.m_curThread, 1);

	int level = controller.lockScheduler();
	ASSERT_EQ(level, 0);

	// Should not have changed thread
	controller.continueExecution();
	ASSERT_EQ(controller.m_curThread, 1);

	controller.unlockScheduler(level);
}
