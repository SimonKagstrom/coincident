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
	Function(IElf::IFunctionListener *listener,
			const char *name, void *addr, size_t size)
	{
		m_name = xstrdup(name);
		m_size = size;
		m_entry = addr;
		m_data = new uint8_t[m_size];

		listener->onFunction(*this);
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
				IFunction *fn = new Function(m_listener, sym_name, (void *)addr, size);

				m_functionsByAddress[(void *)addr] = fn;
				m_functionsByName[std::string(sym_name)] = fn;
			}

			s++;
		}
	}
	std::map<std::string, IFunction *> m_functionsByName;
	std::map<void *, IFunction *> m_functionsByAddress;

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
