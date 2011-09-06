#include <ptrace.hh>
#include <utils.hh>

#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <map>
#include <list>

class Ptrace : public IPtrace
{
public:
	Ptrace()
	{
		m_breakpointId = 0;
	}

	bool readMemory(uint8_t *dst, void *start, size_t bytes)
	{
		memcpy(dst, start, bytes);

		return true;
	}

	int forkAndAttach()
	{
		pid_t child, who;
		int status;

		child = fork();
		if (child < 0) {
			error("fork failed!\n");
			return -1;
		}

		if (child == 0) {
			int res;

			/* We're in the child, set me as traced */
			res = ptrace(PTRACE_TRACEME, 0, 0, 0);
			if (res < 0) {
				error("Can't set me as ptraced");
				return -1;
			}

			return 0;
		}

		/* Wait for the initial stop */
		who = waitpid(child, &status, 0);
		if (who < 0) {
			error("waitpid failed");
			return -1;
		}
		if (!WIFSTOPPED(status)) {
			error("Child hasn't stopped: %x\n", status);
			return -1;
		}
		ptrace(PTRACE_SETOPTIONS, child, 0, PTRACE_O_TRACECLONE | PTRACE_O_TRACEFORK);

		m_children.push_back(child);

		return child;
	}

	int setBreakpoint(void *addr)
	{
		uint8_t data;
		int id;

		// There already?
		if (m_addrToBreakpointMap.find(addr) != m_addrToBreakpointMap.end())
			return m_addrToBreakpointMap[addr];

		if (readMemory(&data, addr, 1) == false)
			return -1;

		id = m_breakpointId++;

		m_breakpointToAddrMap[id] = addr;
		m_addrToBreakpointMap[addr] = id;
		m_instructionMap[addr] = data;

		// Set the breakpoint in all children
		for (std::list<int>::iterator iter = m_children.begin();
				iter != m_children.end();
				iter++)
			writeByte(*iter, addr, 0xcc);

		return -1;
	}

	bool clearBreakpoint(int id)
	{
		if (m_breakpointToAddrMap.find(id) == m_breakpointToAddrMap.end())
			return false;

		void *addr = m_breakpointToAddrMap[id];

		panic_if(m_addrToBreakpointMap.find(addr) == m_addrToBreakpointMap.end(),
				"Breakpoint id, but no addr-to-id map!");

		panic_if(m_instructionMap.find(addr) == m_instructionMap.end(),
				"Breakpoint found, but no instruction data at that point!");

		m_breakpointToAddrMap.erase(id);
		m_addrToBreakpointMap.erase(addr);

		// Clear the actual breakpoint instruction in all children
		for (std::list<int>::iterator iter = m_children.begin();
				iter != m_children.end();
				iter++)
			writeByte(*iter, addr, m_instructionMap[addr]);

		return true;
	}

	void singleStep(int pid)
	{
		void *pc = getPc(pid);

		panic_if(m_instructionMap.find(pc) == m_instructionMap.end(),
				"Signle-step over no breakpoint!");

		writeByte(pid, pc, m_instructionMap[pc]);
		long res = ptrace(PTRACE_SINGLESTEP, pid, 0, NULL);
		panic_if(res < 0,
				"ptrace singlestep failed!\n");
		writeByte(pid, pc, 0xcc);
	}

	const PtraceEvent continueExecution(int pid)
	{
		PtraceEvent out;

		out.type = ptrace_error;
		out.eventId = -1;
		out.addr = NULL;

		return out;
	}


private:
	void *getPc(int pid)
	{
		struct user_regs_struct regs;

		memset(&regs, 0, sizeof(regs));
		ptrace(PTRACE_GETREGS, pid, 0, &regs);

		return (void *)regs.eip;
	}

	// Assume x86 with single-byte breakpoint instructions for now...
	void writeByte(int pid, void *addr, uint8_t byte)
	{
		unsigned long aligned = getAligned((unsigned long)addr);
		unsigned long offs = (unsigned long)addr - aligned;
		unsigned long shift = 8 * offs;
		unsigned long data = byte;
		unsigned long old_data;
		unsigned long val;

		old_data = ptrace(PTRACE_PEEKTEXT, pid, 0, 0);
		val = (old_data & ~(0xffUL << shift)) | (data << shift);
		ptrace(PTRACE_POKETEXT, pid, aligned, val);
	}

	unsigned long getAligned(unsigned long addr)
	{
		return (addr / sizeof(unsigned long)) * sizeof(unsigned long);
	}


	int m_breakpointId;

	std::map<void *, uint8_t> m_instructionMap;
	std::map<int, void *> m_breakpointToAddrMap;
	std::map<void *, int> m_addrToBreakpointMap;

	std::list<int> m_children;
};

IPtrace &IPtrace::getInstance()
{
	static Ptrace *instance;

	if (!instance)
		instance = new Ptrace();

	return *instance;
}
