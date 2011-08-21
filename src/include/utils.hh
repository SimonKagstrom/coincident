#pragma once

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define panic(x...) do \
{ \
	fprintf(stderr, "============panic===========\n"); \
	fprintf(stderr, x); \
	fprintf(stderr, "============================\n"); \
	exit(1); \
} while(0)

#define panic_if(cond, x...) \
		do { if ((cond)) panic(x); } while(0)

static inline char *xstrdup(const char *s)
{
	char *out = strdup(s);

	panic_if(!out, "strdup failed");

	return out;
}
