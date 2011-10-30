#include <elf.hh>
#include <ptrace.hh>
#include <function.hh>
#include <disassembly.hh>
#include <utils.hh>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libelf.h>
#include <map>
#include <string>

class Function : public IFunction, IDisassembly::IInstructionListener
{
public:
	Function(const char *name, void *addr, size_t size)
	{
		m_name = xstrdup(name);
		m_size = size;
		m_entry = addr;
		m_data = new uint8_t[m_size];
	}

	virtual ~Function()
	{
		delete m_name;
		delete m_data;
	}

	const char *getName()
	{
		return m_name;
	}

	size_t getSize()
	{
		return m_size;
	}

	void *getEntry()
	{
		return m_entry;
	}

	void setAddress(void *addr)
	{
		m_entry = addr;
	}

	void setSize(size_t size)
	{
		m_size = size;
	}

	std::list<void *> &getMemoryRefs()
	{
		bool res = IPtrace::getInstance().readMemory(m_data,
				m_entry, m_size);

		m_memoryRefList.clear();

		if (!res) {
			error("Can't read memory at %p", m_entry);
			return m_memoryRefList;
		}

		IDisassembly::getInstance().execute(this, m_data, m_size);

		return m_memoryRefList;
	}

	// These three functions are the IInstructionListerners
	void onMemoryReference(off_t offset, bool isLoad)
	{
		off_t addr = (off_t)m_entry + offset;

		m_memoryRefList.push_back((void *)addr);
	}

	void onCall(off_t offset)
	{
	}

	void onBranch(off_t offset)
	{
	}

private:
	const char *m_name;
	size_t m_size;
	void *m_entry;
	uint8_t *m_data;

	std::list<void *> m_memoryRefList;
};

class Elf : public IElf
{
public:
	Elf()
	{
		m_elf = NULL;
		m_listener = NULL;
	}

	bool setFile(IFunctionListener *listener,
			const char *filename)
	{
		Elf_Scn *scn = NULL;
		Elf32_Ehdr *ehdr;
		size_t shstrndx;
		bool ret = false;
		int fd;

		m_listener = listener;

		m_functionsByAddress.clear();
		m_functionsByName.clear();

		panic_if(elf_version(EV_CURRENT) == EV_NONE,
				"ELF version failed on %s\n", filename);

		fd = open(filename, O_RDONLY, 0);
		if (fd < 0) {
				error("Cannot open %s\n", filename);
				return false;
		}

		if (!(m_elf = elf_begin(fd, ELF_C_READ, NULL)) ) {
				error("elf_begin failed on %s\n", filename);
				goto out_open;
		}


		if (!(ehdr = elf32_getehdr(m_elf))) {
				error("elf32_getehdr failed on %s\n", filename);
				goto out_elf_begin;
		}

		if (elf_getshdrstrndx(m_elf, &shstrndx) < 0) {
				error("elf_getshstrndx failed on %s\n", filename);
				goto out_elf_begin;
		}

		while ( (scn = elf_nextscn(m_elf, scn)) != NULL )
		{
			Elf32_Shdr *shdr = elf32_getshdr(scn);
			Elf_Data *data = elf_getdata(scn, NULL);
			char *name;

			name = elf_strptr(m_elf, shstrndx, shdr->sh_name);
			if(!data) {
					error("elf_getdata failed on section %s in %s\n",
					name, filename);
					goto out_elf_begin;
			}

			/* Handle symbols */
			if (shdr->sh_type == SHT_SYMTAB)
				handleSymtab(scn);
			if (shdr->sh_type == SHT_DYNSYM)
				handleDynsym(scn);
		}
		elf_end(m_elf);
		if (!(m_elf = elf_begin(fd, ELF_C_READ, NULL)) ) {
				error("elf_begin failed on %s\n", filename);
				goto out_open;
		}
		while ( (scn = elf_nextscn(m_elf, scn)) != NULL )
		{
			Elf32_Shdr *shdr = elf32_getshdr(scn);
			char *name = elf_strptr(m_elf, shstrndx, shdr->sh_name);

			// .rel.plt
			if (shdr->sh_type == SHT_REL && strcmp(name, ".rel.plt") == 0)
				handleRelPlt(scn);
		}
		m_fixupFunctions.clear();
		for (FunctionsByAddress_t::iterator it = m_functionsByAddress.begin();
				it != m_functionsByAddress.end();
				it++) {
			Function *fn = it->second;

			m_listener->onFunction(*fn);
		}

		ret = true;

out_elf_begin:
		elf_end(m_elf);
out_open:
		close(fd);

		return ret;
	}

	IFunction *functionByAddress(void *addr)
	{
		return m_functionsByAddress[addr];
	}

	IFunction *functionByName(const char *name)
	{
		return m_functionsByName[std::string(name)];
	}

private:
	typedef std::map<int, Function *> FixupMap_t;

	void *offsetTableToAddress(Elf32_Addr addr)
	{
		/*
		 * The .got.plt table contains a pointer to the push instruction
		 * below:
		 *
		 *  08070f10 <pthread_self@plt>:
		 *   8070f10:       ff 25 58 93 0b 08       jmp    *0x80b9358
		 *   8070f16:       68 b0 06 00 00          push   $0x6b0
		 *
		 * so to get the entry point, we rewind the pointer to the start
		 * of the jmp.
		 */
		return (void *)(addr - 6);
	}

	void handleRelPlt(Elf_Scn *scn)
	{
		Elf32_Shdr *shdr = elf32_getshdr(scn);
		Elf_Data *data = elf_getdata(scn, NULL);
		Elf32_Rel *r = (Elf32_Rel *)data->d_buf;
		int n = data->d_size / sizeof(Elf32_Rel);

		panic_if(n <= 0,
				"Section data too small (%zd) - no symbols\n",
				data->d_size);

		for (int i = 0; i < n; i++, r++) {
			Elf32_Addr *got_plt = (Elf32_Addr *)r->r_offset;

			FixupMap_t::iterator it = m_fixupFunctions.find(ELF32_R_SYM(r->r_info));

			if (it == m_fixupFunctions.end())
				continue;
			Function *fn = it->second;

			fn->setAddress(offsetTableToAddress(*got_plt));
			fn->setSize(1);
			m_functionsByAddress[fn->getEntry()] = fn;
		}
	}

	void handleDynsym(Elf_Scn *scn)
	{
		handleSymtab(scn);
	}

	void handleSymtab(Elf_Scn *scn)
	{
		Elf32_Shdr *shdr = elf32_getshdr(scn);
		Elf_Data *data = elf_getdata(scn, NULL);
		Elf32_Sym *s = (Elf32_Sym *)data->d_buf;
		int n_syms = 0;
		int n_fns = 0;
		int n_datas = 0;
		int n = data->d_size / sizeof(Elf32_Sym);

		panic_if(n <= 0,
				"Section data too small (%zd) - no symbols\n",
				data->d_size);

		/* Iterate through all symbols */
		for (int i = 0; i < n; i++)
		{
			const char *sym_name = elf_strptr(m_elf, shdr->sh_link, s->st_name);
			int type = ELF32_ST_TYPE(s->st_info);

			/* Ohh... This is an interesting symbol, add it! */
			if ( type == STT_FUNC) {
				Elf32_Addr addr = s->st_value;
				Elf32_Word size = s->st_size;
				Function *fn = new Function(sym_name, (void *)addr, size);

				m_functionsByName[std::string(sym_name)] = fn;
				// Needs fixup?
				if (shdr->sh_type == SHT_DYNSYM && size == 0)
					m_fixupFunctions[i] = fn;
				else
					m_functionsByAddress[(void *)addr] = fn;
			}

			s++;
		}
	}
	std::map<std::string, IFunction *> m_functionsByName;
	std::map<void *, IFunction *> m_functionsByAddress;

	FixupMap_t m_fixupFunctions;

	Elf *m_elf;
	IFunctionListener *m_listener;
};

IElf &IElf::getInstance()
{
	static Elf *instance;

	if (!instance)
		instance = new Elf();

	return *instance;
}
