#include "test.hh"

#include "../src/controller.cc"
#include "mock-ptrace.hh"

static int test_thread(void *priv)
{
	return 0;
}

TEST(controllerForkError)
{
	IController &controller = IController::getInstance();
	MockPtrace &ptrace = (MockPtrace &)IPtrace::getInstance();

	EXPECT_CALL(ptrace, forkAndAttach())
		.Times(AtLeast(1))
		.WillRepeatedly(Return(-1));

	controller.run();
}

TEST(controllerAddProcessForkError)
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

TEST(controllerTestThreadRemoval)
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
	ASSERT_EQ(controller.m_nThreads, 2);

	// Will remove thread since it exited
	controller.handleBreakpoint(ev);
	ASSERT_EQ(controller.m_nThreads, 1);
}

