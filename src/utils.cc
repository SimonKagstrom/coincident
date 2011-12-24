#include <sched.h>
#include <sys/types.h>

#include "utils.hh"

int coin_get_current_cpu(void)
{
	return sched_getcpu();
}

void coin_set_cpu(pid_t pid, int cpu)
{
	// Switching CPU while running will cause icache
	// conflicts. So let's just forbid that.

	cpu_set_t *set = CPU_ALLOC(1);
	panic_if (!set,
			"Can't allocate CPU set!\n");
	CPU_SET(cpu, set);
	panic_if (sched_setaffinity(pid, CPU_ALLOC_SIZE(1), set) < 0,
			"Can't set CPU affinity. Coincident won't work");
	CPU_FREE(set);
}
