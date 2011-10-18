#include <stdio.h>
#include <udis86.h>

#include <disassembly.hh>

static int next_byte(ud_t *ud);

class Disassembly : public IDisassembly
{
public:
	friend int next_byte(ud_t *);

	Disassembly()
	{
		ud_init(&m_ud);

		ud_set_user_opaque_data(&m_ud, (void *)this);
		ud_set_mode(&m_ud, 32);

		m_data = NULL;
		m_dataSize = 0;
		m_count = 0;
	}

	bool execute(IDisassembly::IInstructionListener *listener,
			uint8_t *data, size_t size)
	{
		if (!listener)
			return false;

		if (!data || size == 0)
			return false;

		ud_set_pc(&m_ud, 0);

		m_data = data;
		m_dataSize = size;
		m_count = 0;

		ud_set_input_hook(&m_ud, next_byte);

		while (ud_disassemble(&m_ud)) {
			bool mem = (m_ud.operand[0].type == UD_OP_MEM ||
					m_ud.operand[1].type == UD_OP_MEM ||
					m_ud.operand[2].type == UD_OP_MEM);

			bool call = m_ud.mnemonic == UD_Icall;

			bool branch = m_ud.mnemonic == UD_Ijo ||
					m_ud.mnemonic == UD_Ijno ||
					m_ud.mnemonic == UD_Ijb ||
					m_ud.mnemonic == UD_Ijae ||
					m_ud.mnemonic == UD_Ijz ||
					m_ud.mnemonic == UD_Ijnz ||
					m_ud.mnemonic == UD_Ijbe ||
					m_ud.mnemonic == UD_Ija ||
					m_ud.mnemonic == UD_Ijs ||
					m_ud.mnemonic == UD_Ijns ||
					m_ud.mnemonic == UD_Ijp ||
					m_ud.mnemonic == UD_Ijnp ||
					m_ud.mnemonic == UD_Ijl ||
					m_ud.mnemonic == UD_Ijge ||
					m_ud.mnemonic == UD_Ijle ||
					m_ud.mnemonic == UD_Ijg ||
					m_ud.mnemonic == UD_Ijcxz ||
					m_ud.mnemonic == UD_Ijecxz ||
					m_ud.mnemonic == UD_Ijrcxz ||
					m_ud.mnemonic == UD_Ijmp;


			if (mem)
				listener->onMemoryReference(ud_insn_off(&m_ud), false);

			if (call)
				listener->onCall(ud_insn_off(&m_ud));

			if (branch)
				listener->onBranch(ud_insn_off(&m_ud));
		}

		return true;
	}

private:
	int nextUdByte()
	{
		if (m_count == m_dataSize)
			return UD_EOI;

		return m_data[m_count++];
	}

	ud_t m_ud;
	uint8_t *m_data;
	size_t m_dataSize;
	size_t m_count;
};

static int next_byte(ud_t *ud)
{
	Disassembly *p = (Disassembly *)ud_get_user_opaque_data(ud);

	return p->nextUdByte();
}



IDisassembly &IDisassembly::getInstance()
{
	static Disassembly *instance;

	if (!instance)
		instance = new Disassembly();

	return *instance;
}
