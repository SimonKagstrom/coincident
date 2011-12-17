#include <utils.hh>
#include <coincident/controller.hh>
#include <coincident/thread.hh>
#include <apis.hh>

#include <map>
#include <pthread.h>

#include "common.h"

using namespace coincident;

static void function_replacement(void)
{
}

class PthreadSelf : public IController::IFunctionHandler
{
public:
	bool handle(IThread *curThread, void *addr, const PtraceEvent &ev)
	{
		static bool first;
		static pthread_t pt;

		if (!first) {
			first = false;
			pt = pthread_self();
		}

		// Don't execute the real pthread stuff, instead just return
		curThread->setPc((void *)function_replacement);
		curThread->setReturnValue((unsigned long)curThread);

		return true;
	}
};


void coincident_api_init_pthreads(void)
{
	IController &controller = IController::getInstance();
	bool res;

	res = controller.registerFunctionHandler((void *)pthread_self,
			new PthreadSelf());
	panic_if(!res, "Can't register handler");

	local_coincident_api_init_pthreads_mutex();
}
