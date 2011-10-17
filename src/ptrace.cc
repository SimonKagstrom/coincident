#include <ptrace.hh>
#include <utils.hh>

#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sched.h>
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
			kill(getpid(), SIGSTOP);

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

		m_child = child;

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

		// Set the breakpoint
		writeByte(m_child, addr, 0xcc);

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

		// Clear the actual breakpoint instruction
		writeByte(m_child, addr, m_instructionMap[addr]);

		return true;
	}

	void saveRegisters(int pid, void *regs)
	{
		ptrace(PTRACE_GETREGS, pid, 0, regs);
	}

	void loadRegisters(int pid, void *regs)
	{
		ptrace(PTRACE_SETREGS, pid, 0, regs);
	}

	void singleStep(int pid)
	{
		void *pc = getPc(pid);

		panic_if(m_instructionMap.find(pc) == m_instructionMap.end(),
				"Single-step over no breakpoint at %p", pc);

		writeByte(pid, pc, m_instructionMap[pc]);
		long res = ptrace(PTRACE_SINGLESTEP, pid, 0, NULL);
		panic_if(res < 0,
				"ptrace singlestep failed!\n");
		writeByte(pid, pc, 0xcc);
	}

	const PtraceEvent continueExecution(int pid)
	{
		PtraceEvent out;
		int status;
		int who;

		// Assume error
		out.type = ptrace_error;
		out.eventId = -1;
		out.addr = NULL;

		ptrace(PTRACE_CONT, pid, 0, 0);

		who = waitpid(pid, &status, __WALL);
		if (who == -1)
			return out;

		// A signal?
		if (WIFSTOPPED(status)) {
			// A trap?
			if (WSTOPSIG(status) == SIGTRAP) {
				out.type = ptrace_breakpoint;
				out.eventId = -1;
				out.addr = getPc(pid);

				return out;
			}
			// No, deliver it directly
			ptrace(PTRACE_CONT, who, 0, WSTOPSIG(status));
		}
		// Thread died?
		if (WIFSIGNALED(status) || WIFEXITED(status)) {
			if (who == pid) {
				out.type = ptrace_exit;
				out.eventId = -1;
				out.addr = NULL;
				return out;
			}
		}

		return out;
	}


private:
	void *getPc(int pid)
	{
		struct user_regs_struct regs;

		memset(&regs, 0, sizeof(regs));
		ptrace(PTRACE_GETREGS, pid, 0, &regs);

		return (void *)(regs.eip - 1);
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

	typedef std::map<int, void *> breakpointToAddrMap_t;
	typedef std::map<void *, int> addrToBreakpointMap_t;
	typedef std::map<void *, uint8_t> instructionMap_t;

	int m_breakpointId;

	instructionMap_t m_instructionMap;
	breakpointToAddrMap_t m_breakpointToAddrMap;
	addrToBreakpointMap_t m_addrToBreakpointMap;

	pid_t m_child;
};

IPtrace &IPtrace::getInstance()
{
	static Ptrace *instance;

	if (!instance)
		instance = new Ptrace();

	return *instance;
}
