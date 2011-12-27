#include "test.hh"

#include <elf.hh>
#include <coincident/coincident.h>
#include <function.hh>
#include "mock-ptrace.hh"

#include <string>

static MockPtrace ptraceInstance;

IPtrace &IPtrace::getInstance()
{
	return ptraceInstance;
}

class FunctionListener : public IElf::IFunctionListener
{
public:
		void onFunction(IFunction &fn)
		{
			m_map[std::string(fn.getName())]++;

			IFunction::ReferenceList_t list = fn.getMemoryStores();
			if (strcmp(fn.getName(), "mockReadMemory") == 0)
				ASSERT_TRUE(list.size() > 0);
		}

		std::map<std::string, int> m_map;
};

static uint8_t asm_dump[] =
{
		0x76, 0x27,       //                   jbe    804ab40 <ud_decode+0x630>
		0x89, 0x1c, 0x24, //                   mov    %ebx,(%esp)
		0x83, 0xe0, 0x0f, //                   and    $0xf,%eax
		0x8d, 0x50, 0x01, //                   lea    0x1(%eax),%edx
		0xd1, 0xea,       //                   shr    %edx
		0x88, 0x55, 0xcc, //                   mov    %dl,-0x34(%ebp)
		0xe8, 0x88, 0xe3, 0xff, 0xff, //       call   8048ef0 <inp_next>
		0x8b, 0x83, 0x58, 0x02, 0x00, 0x00, // mov    0x258(%ebx),%eax
};


extern "C" bool mockReadMemory(uint8_t *dst, void *start, size_t bytes)
{
	size_t to_cpy = bytes;

	if (to_cpy < sizeof(asm_dump))
		to_cpy = sizeof(asm_dump);

	memcpy(dst, asm_dump, to_cpy);

	return true;
}

extern "C" int mockSetBreakpoint(void *addr)
{
	static int id = 0;

	id++;

	return id;
}

TEST(elffile, DEADLINE_REALTIME_MS(30000))
{
	FunctionListener listener;
	char filename[1024];
	bool res;

	IElf *elf = IElf::open("not-found");
	ASSERT_TRUE(!elf);

	sprintf(filename, "%s/Makefile", crpcut::get_start_dir());
	elf = IElf::open(filename);
	ASSERT_TRUE(!elf);

	EXPECT_CALL(ptraceInstance, setBreakpoint(_))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke(mockSetBreakpoint));
	EXPECT_CALL(ptraceInstance, readMemory(_, _,_))
		.WillRepeatedly(Invoke(mockReadMemory));

	coincident_set_debug_mask(0xff);
	// Don't run the unit test as root!
	sprintf(filename, "/proc/self/exe", crpcut::get_start_dir());
	elf = IElf::open(filename);
	ASSERT_TRUE(elf);

	res = elf->parse(&listener);
	ASSERT_TRUE(res == true);

	ASSERT_TRUE(listener.m_map[std::string("mockReadMemory")] > 0);
	ASSERT_TRUE(listener.m_map[std::string("mockSetBreakpoint")] > 0);

	ASSERT_FALSE(elf->functionByName("mockReadMemory").empty());
	ASSERT_TRUE(elf->functionByName("mockReadMemoryNotFound").empty());
}
