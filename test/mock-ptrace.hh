#pragma once

#include "test.hh"

#include <ptrace.hh>

class MockPtrace : public IPtrace {
public:
	MockPtrace() { }

	MOCK_METHOD3(readMemory, bool(uint8_t *dst, void *start, size_t bytes));
	MOCK_METHOD1(setBreakpoint, int(void *addr));
	MOCK_METHOD1(clearBreakpoint, bool(int id));
	MOCK_METHOD0(forkAndAttach, int());
	MOCK_METHOD1(singleStep, void(int pid));
	MOCK_METHOD1(continueExecution, const PtraceEvent(int pid));
};
