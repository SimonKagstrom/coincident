#pragma once

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define error(x...) do \
{ \
	fprintf(stderr, "Error: "); \
	fprintf(stderr, x); \
} while(0)

#define warning(x...) do \
{ \
	fprintf(stderr, "Warning: "); \
	fprintf(stderr, x); \
} while(0)

#define panic(x...) do \
{ \
	error(x); \
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
