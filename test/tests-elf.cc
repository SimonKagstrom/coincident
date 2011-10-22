#include "test.hh"

#include <elf.hh>
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

			std::list<void *> list = fn.getMemoryRefs();
			if (strcmp(fn.getName(), "vobb") == 0)
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


bool mockReadMemory(uint8_t *dst, void *start, size_t bytes)
{
	size_t to_cpy = bytes;

	if (to_cpy < sizeof(asm_dump))
		to_cpy = sizeof(asm_dump);

	memcpy(dst, asm_dump, to_cpy);

	return true;
}

int mockSetBreakpoint(void *addr)
{
	static int id = 0;

	id++;

	return id;
}

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

	EXPECT_CALL(ptraceInstance, setBreakpoint(_))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke(mockSetBreakpoint));
	EXPECT_CALL(ptraceInstance, readMemory(_, _,_))
		.WillRepeatedly(Invoke(mockReadMemory));

	// Don't run the unit test as root!
	sprintf(filename, "%s/test-elf", crpcut::get_start_dir());
	res = elf.setFile(&listener, filename);
	ASSERT_TRUE(res == true);

	ASSERT_TRUE(listener.m_map[std::string("vobb")] > 0);
	ASSERT_TRUE(listener.m_map[std::string("mibb")] > 0);

	ASSERT_NE(elf.functionByName("vobb"), 0);
	ASSERT_EQ(elf.functionByName("vobb2"), 0);
}
