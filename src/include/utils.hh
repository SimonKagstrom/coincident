#pragma once

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define error(x...) do \
{ \
	fprintf(stderr, "Error: "); \
	fprintf(stderr, x); \
	fprintf(stderr, "\n"); \
} while(0)

#define warning(x...) do \
{ \
	fprintf(stderr, "Warning: "); \
	fprintf(stderr, x); \
	fprintf(stderr, "\n"); \
} while(0)

#define panic(x...) do \
{ \
	error(x); \
	exit(1); \
} while(0)

enum debug_mask
{
	INFO_MSG   = 1,
	PTRACE_MSG = 2,
	ELF_MSG    = 4,
	BP_MSG     = 8,
};
extern int g_coin_debug_mask;

static inline void coin_debug(enum debug_mask dbg, const char *fmt, ...) __attribute__((format(printf,2,3)));

static inline void coin_debug(enum debug_mask dbg, const char *fmt, ...)
{
	va_list ap;

	if ((g_coin_debug_mask & dbg) == 0)
		return;

	va_start(ap, fmt);
	vfprintf(stdout, fmt, ap);
	va_end(ap);
}

#define panic_if(cond, x...) \
		do { if ((cond)) panic(x); } while(0)

static inline char *xstrdup(const char *s)
{
	char *out = strdup(s);

	panic_if(!out, "strdup failed");

	return out;
}

static inline void *xmalloc(size_t sz)
{
  void *out = malloc(sz);

  panic_if(!out, "malloc failed");
  memset(out, 0, sz);

  return out;
}

int coin_get_current_cpu(void);

void coin_set_cpu(pid_t pid, int cpu);
