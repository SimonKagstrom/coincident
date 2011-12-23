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

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <link.h>

using namespace coincident;

class Function : public IFunction, IDisassembly::IInstructionListener
{
public:
	Function(const char *name, void *addr, size_t size,
			IFunction::FunctionType type)
	{
		m_refsValid = false;
		m_name = xstrdup(name);
		m_size = size;
		m_entry = addr;
		m_data = new uint8_t[m_size];
		m_type = type;
	}

	virtual ~Function()
	{
		delete m_name;
		delete m_data;
	}

	enum IFunction::FunctionType getType()
	{
		return m_type;
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

	void disassembleFunction()
	{
		bool res = IPtrace::getInstance().readMemory(m_data,
				m_entry, m_size);

		m_loadList.clear();
		m_storeList.clear();

		m_refsValid = true;
		if (!res) {
			error("Can't read memory at %p", m_entry);
			return;
		}

		IDisassembly::getInstance().execute(this, m_data, m_size);
	}

	ReferenceList_t &getMemoryLoads()
	{
		if (!m_refsValid)
			disassembleFunction();

		return m_loadList;
	}

	ReferenceList_t &getMemoryStores()
	{
		if (!m_refsValid)
			disassembleFunction();

		return m_storeList;
	}

	// These three functions are the IInstructionListerners
	void onMemoryReference(off_t offset, bool isLoad)
	{
		off_t addr = (off_t)m_entry + offset;

		if (isLoad)
			m_loadList.push_back((void *)addr);
		else
			m_storeList.push_back((void *)addr);
	}

	void onCall(off_t offset)
	{
	}

	void onBranch(off_t offset)
	{
	}

private:
	bool m_refsValid;
	const char *m_name;
	size_t m_size;
	void *m_entry;
	uint8_t *m_data;
	enum IFunction::FunctionType m_type;

	ReferenceList_t m_loadList;
	ReferenceList_t m_storeList;
};

class Elf : public IElf
{
public:
	Elf(const char *filename)
	{
		m_elf = NULL;
		m_listener = NULL;
		m_filename = strdup(filename);
		m_onlyDynsyms = false;
	}

	~Elf()
	{
		free((void *)m_filename);
	}

	bool checkFile()
	{
		Elf *elf;
		bool out = true;
		int fd;

		fd = ::open(m_filename, O_RDONLY, 0);
		if (fd < 0) {
				error("Cannot open %s\n", m_filename);
				return false;
		}

		if (!(elf = elf_begin(fd, ELF_C_READ, NULL)) ) {
				error("elf_begin failed on %s\n", m_filename);
				out = false;
				goto out_open;
		}
		if (!elf32_getehdr(elf)) {
				error("elf32_getehdr failed on %s\n", m_filename);
				out = false;
		}
		elf_end(elf);

out_open:
		close(fd);

		return out;
	}

	static int phdrCallback(struct dl_phdr_info *info, size_t size,
			void *data)
	{
		struct args
		{
			Elf *p;
			IFunctionListener *listener;
		};
		struct args *a = (struct args *)data;

		a->p->handlePhdr(a->listener, info, size);

		return 0;
	}

	void handlePhdr(IFunctionListener *listener,
			struct dl_phdr_info *info, size_t size)
	{
		int phdr;

		/* Only dynamic symbols for shared libraries */
		m_onlyDynsyms = strlen(info->dlpi_name) != 0;

		if (strlen(info->dlpi_name) != 0) {
			free( (void *)m_filename );
			m_filename = strdup(info->dlpi_name);
		}

		m_curSegments.clear();
		for (phdr = 0; phdr < info->dlpi_phnum; phdr++) {
			const ElfW(Phdr) *cur = &info->dlpi_phdr[phdr];

			if (cur->p_type != PT_LOAD)
				continue;

			m_curSegments.push_back(Segment(cur->p_paddr, info->dlpi_addr + cur->p_vaddr,
					cur->p_memsz, cur->p_align));
		}
		parseOne(listener);
	}

	bool parse(IFunctionListener *listener)
	{
		struct
		{
			Elf *p;
			IFunctionListener *listener;
		} cbArgs;

		m_functionsByAddress.clear();
		m_functionsByName.clear();

		cbArgs.p = this;
		cbArgs.listener = listener;
		dl_iterate_phdr(phdrCallback, (void *)&cbArgs);

		return true;
	}

	bool parseOne(IFunctionListener *listener)
	{
		Elf_Scn *scn = NULL;
		Elf32_Ehdr *ehdr;
		size_t shstrndx;
		bool ret = false;
		int fd;

		m_listener = listener;

		fd = ::open(m_filename, O_RDONLY, 0);
		if (fd < 0) {
				error("Cannot open %s\n", m_filename);
				return false;
		}

		if (!(m_elf = elf_begin(fd, ELF_C_READ, NULL)) ) {
				error("elf_begin failed on %s\n", m_filename);
				goto out_open;
		}


		if (!(ehdr = elf32_getehdr(m_elf))) {
				error("elf32_getehdr failed on %s\n", m_filename);
				goto out_elf_begin;
		}

		if (elf_getshdrstrndx(m_elf, &shstrndx) < 0) {
				error("elf_getshstrndx failed on %s\n", m_filename);
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
							name, m_filename);
					goto out_elf_begin;
			}

			/* Handle symbols */
			if (shdr->sh_type == SHT_SYMTAB && m_onlyDynsyms == false)
				handleSymtab(scn);
			if (shdr->sh_type == SHT_DYNSYM)
				handleDynsym(scn);
		}
		elf_end(m_elf);
		if (!(m_elf = elf_begin(fd, ELF_C_READ, NULL)) ) {
			error("elf_begin failed on %s\n", m_filename);
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

	IElf::FunctionList_t functionByName(const char *name)
	{
		return m_functionsByName[std::string(name)];
	}

private:
	class Segment
	{
	public:
		Segment(ElfW(Addr) paddr, ElfW(Addr) vaddr, size_t size, ElfW(Word) align) :
			m_paddr(paddr), m_vaddr(vaddr), m_size(size), m_align(align)
		{
		}

		ElfW(Addr) m_paddr;
		ElfW(Addr) m_vaddr;
		ElfW(Word) m_align;
		size_t m_size;
	};

	typedef std::map<std::string, IElf::FunctionList_t> FunctionsByName_t;
	typedef std::map<void *, Function *> FunctionsByAddress_t;
	typedef std::map<int, Function *> FixupMap_t;
	typedef std::list<Segment> SegmentList_t;

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

	ElfW(Addr) adjustAddressBySegment(ElfW(Addr) addr)
	{
		for (SegmentList_t::iterator it = m_curSegments.begin();
				it != m_curSegments.end(); it++) {
			Segment cur = *it;

			if (addr >= cur.m_paddr && addr < cur.m_paddr + cur.m_size) {
				addr = (addr - cur.m_paddr + cur.m_vaddr);
				break;
			}
		}

		return addr;
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
			Elf32_Addr *got_plt = (Elf32_Addr *)adjustAddressBySegment(r->r_offset);

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
		handleSymtabGeneric(scn, IFunction::SYM_DYNAMIC);
	}

	void handleSymtab(Elf_Scn *scn)
	{
		handleSymtabGeneric(scn, IFunction::SYM_NORMAL);
	}

	void handleSymtabGeneric(Elf_Scn *scn, enum IFunction::FunctionType symType)
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
				Elf32_Addr addr = adjustAddressBySegment(s->st_value);
				Elf32_Word size = s->st_size;
				Function *fn = new Function(sym_name, (void *)addr, size, symType);

				m_functionsByName[std::string(sym_name)].push_back(fn);
				// Needs fixup?
				if (shdr->sh_type == SHT_DYNSYM && size == 0)
					m_fixupFunctions[i] = fn;
				else
					m_functionsByAddress[(void *)addr] = fn;
			}

			s++;
		}
	}

	FunctionsByName_t m_functionsByName;
	FunctionsByAddress_t m_functionsByAddress;
	FixupMap_t m_fixupFunctions;
	SegmentList_t m_curSegments;
	bool m_onlyDynsyms;

	Elf *m_elf;
	IFunctionListener *m_listener;
	const char *m_filename;
};

IElf *IElf::open(const char *filename)
{
	static bool initialized = false;
	Elf *p;

	if (!initialized) {
		panic_if(elf_version(EV_CURRENT) == EV_NONE,
				"ELF version failed\n");
		initialized = true;
	}

	p = new Elf(filename);

	if (p->checkFile() == false) {
		delete p;

		return NULL;
	}

	return p;
}
