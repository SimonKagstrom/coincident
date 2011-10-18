#include "test.hh"

#include <disassembly.hh>

class DisassemblyHarness : public IDisassembly::IInstructionListener
{
public:
	DisassemblyHarness()
	{
		clear();
	}

	void clear()
	{
		m_memRefs = 0;
		m_calls = 0;
		m_branches = 0;
	}

	void onMemoryReference(off_t offset, bool isLoad)
	{
		bool off_ok = offset == 2 || offset == 8 || offset == 13 || offset == 21;

		ASSERT_TRUE(off_ok);
		m_memRefs++;
	}

	void onCall(off_t offset)
	{
		ASSERT_TRUE(offset == 16);
		m_calls++;
	}

	void onBranch(off_t offset)
	{
		ASSERT_TRUE(offset == 0);
		m_branches++;
	}


	int m_memRefs;
	int m_calls;
	int m_branches;
};


static uint8_t asm_dump[] =
{
		0x76, 0x27,       //                    0 jbe    804ab40 <ud_decode+0x630>
		0x89, 0x1c, 0x24, //                    2 mov    %ebx,(%esp)
		0x83, 0xe0, 0x0f, //                    5 and    $0xf,%eax
		0x8d, 0x50, 0x01, //                    8 lea    0x1(%eax),%edx
		0xd1, 0xea,       //                   11 shr    %edx
		0x88, 0x55, 0xcc, //                   13 mov    %dl,-0x34(%ebp)
		0xe8, 0x88, 0xe3, 0xff, 0xff, //       16 call   8048ef0 <inp_next>
		0x8b, 0x83, 0x58, 0x02, 0x00, 0x00, // 21 mov    0x258(%ebx),%eax
};

TEST(disassembly)
{
	DisassemblyHarness harness;

	IDisassembly &dis = IDisassembly::getInstance();
	bool res;

	res = dis.execute(NULL, NULL, 0);
	ASSERT_TRUE(res == false);

	res = dis.execute(&harness, NULL, 0);
	ASSERT_TRUE(res == false);
	ASSERT_TRUE(harness.m_memRefs == 0);
	ASSERT_TRUE(harness.m_calls == 0);
	ASSERT_TRUE(harness.m_branches == 0);

	res = dis.execute(&harness, asm_dump, sizeof(asm_dump));
	ASSERT_TRUE(res == true);
	ASSERT_TRUE(harness.m_memRefs == 4);
	ASSERT_TRUE(harness.m_calls == 1);
	ASSERT_TRUE(harness.m_branches == 1);
}
