#include "test.hh"

#include <elf.hh>
#include <function.hh>
#include "mock-ptrace.hh"

#include <string>

IPtrace &IPtrace::getInstance()
{
	static IPtrace *instance;

	if (!instance)
		instance = new MockPtrace();

	return *instance;
}

class FunctionListener : public IElf::IFunctionListener
{
public:
		void onFunction(IFunction &fn)
		{
			m_map[std::string(fn.getName())]++;

			int v = fn.setupEntryBreakpoint();
			ASSERT_TRUE(v >= 0);

			std::list<int> list = fn.setupMemoryBreakpoints();
			// Can't read data with ptrace now...
			ASSERT_TRUE(list.size() == 0);
		}

		std::map<std::string, int> m_map;
};

TEST(elffile)
{
	IElf &elf = IElf::getInstance();
	FunctionListener listener;
	char filename[1024];
	bool res;

	res = elf.setFile(&listener, "vobb-mibb");
	ASSERT_TRUE(res == false);

	sprintf(filename, "%s/Makefile", crpcut::get_start_dir());
	res = elf.setFile(&listener, filename);
	ASSERT_TRUE(res == false);


	// Don't run the unit test as root!
	sprintf(filename, "%s/test-elf", crpcut::get_start_dir());
	res = elf.setFile(&listener, filename);
	ASSERT_TRUE(res == true);

	ASSERT_TRUE(listener.m_map[std::string("vobb")] > 0);
	ASSERT_TRUE(listener.m_map[std::string("mibb")] > 0);
}
