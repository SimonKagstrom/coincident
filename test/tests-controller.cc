#include "test.hh"

#include <controller.hh>
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


TEST(controllerRunChild)
{
	IController &controller = IController::getInstance();
	MockPtrace &ptrace = (MockPtrace &)IPtrace::getInstance();

	EXPECT_CALL(ptrace, forkAndAttach())
		.Times(AtLeast(1))
		.WillRepeatedly(Return(100));

	IPtrace::PtraceEvent ev;

	ev.type = ptrace_breakpoint;
	ev.eventId = 100;
	ev.addr = NULL;
	EXPECT_CALL(ptrace, continueExecution(_))
		.Times(AtLeast(1))
		.WillRepeatedly(Return(ev));

	// Add two threads
	controller.addThread(test_thread, NULL);
	controller.addThread(test_thread, NULL);

	controller.run();
}
