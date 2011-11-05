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
	MOCK_METHOD5(selectThread, int(int curThread,
			IThread **threads,
			int nThreads,
			uint64_t timeUs,
			const PtraceEvent *));
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

	PtraceEvent ev, evDefault;

	evDefault.type = ptrace_exit;
	evDefault.eventId = 0;

	ev.type = ptrace_breakpoint;
	ev.eventId = 100;
	ev.addr = NULL;

	EXPECT_CALL(ptrace, continueExecution())
		.Times(AtLeast(1))
		.WillOnce(Return(ev))
		.WillRepeatedly(Return(evDefault));

	EXPECT_CALL(ptrace, singleStep())
		.Times(AtLeast(1));

	// Add two threads
	controller.addThread(test_thread, NULL);
	controller.addThread(test_thread, NULL);

	controller.setRuns(1);
	controller.run();
}

TEST(controllerTestThreadRemoval, DEADLINE_REALTIME_MS(10000))
{
	Controller &controller = (Controller &)IController::getInstance();
	MockPtrace &ptrace = (MockPtrace &)IPtrace::getInstance();

	PtraceEvent ev;

	EXPECT_CALL(ptrace, singleStep())
		.Times(AtLeast(1));


	ev.addr = (void *)Session::threadExit;
	ev.eventId = 10;
	ev.type = ptrace_breakpoint;

	controller.addThread(test_thread, NULL);
	controller.addThread(test_thread, NULL);
	ASSERT_EQ(controller.m_nThreads, 2);

	Session cur(controller, controller.m_nThreads, controller.m_threads);
	ASSERT_EQ(cur.m_nThreads, 2);

	// Will remove thread since it exited
	cur.handleBreakpoint(ev);
	ASSERT_EQ(cur.m_nThreads, 1);
}

TEST(controllerThreadScheduling, DEADLINE_REALTIME_MS(10000))
{
	Controller &controller = (Controller &)IController::getInstance();
	MockPtrace &ptrace = (MockPtrace &)IPtrace::getInstance();

	PtraceEvent ev;

	ev.type = ptrace_breakpoint;
	ev.eventId = 100;
	ev.addr = (void *)(((unsigned long)test_thread) + 1); // No function!

	EXPECT_CALL(ptrace, continueExecution())
		.Times(AtLeast(1))
		.WillRepeatedly(Return(ev));

	EXPECT_CALL(ptrace, singleStep())
		.Times(AtLeast(1));

	// Add two threads
	controller.addThread(test_thread, NULL);
	controller.addThread(test_thread, NULL);

	MockThreadSelector selector;

	controller.setThreadSelector(&selector);
	EXPECT_CALL(selector, selectThread(_,_,_,_,_))
		.Times(Exactly(1))
		.WillOnce(Return(1));

	EXPECT_CALL(ptrace, saveRegisters(_))
		.Times(AtLeast(1));
	EXPECT_CALL(ptrace, loadRegisters(_))
		.Times(AtLeast(1));

	Session cur(controller, controller.m_nThreads, controller.m_threads);

	ASSERT_EQ(cur.m_curThread, 0);
	cur.continueExecution();
	ASSERT_EQ(cur.m_curThread, 1);

	int level = controller.lockScheduler();
	ASSERT_EQ(level, 0);

	// Should not have changed thread
	cur.continueExecution();
	ASSERT_EQ(cur.m_curThread, 1);

	controller.unlockScheduler(level);
}
