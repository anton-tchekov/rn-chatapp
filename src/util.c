#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t alloc_cnt, free_cnt, total_bytes;

static void mcheck(void *m, size_t size)
{
	if(!m)
	{
		fprintf(stderr, "Memory allocation of %zu bytes failed\n", size);
		exit(1);
	}

	if(size)
	{
		++alloc_cnt;
		total_bytes += size;
	}
}

void *smalloc(size_t size)
{
	void *m = malloc(size);
	mcheck(m, size);
	return m;
}

void *scalloc(size_t size)
{
	void *m = calloc(size, 1);
	mcheck(m, size);
	return m;
}

void sfree(void *p)
{
	if(p)
	{
		++free_cnt;
		free(p);
	}
}

void print_allocs(void)
{
	printf("%zu allocs, %zu frees, %zu bytes total\n",
		alloc_cnt, free_cnt, total_bytes);
}

size_t filter(void *base, size_t num, size_t width, const void *data,
	int (*keep)(void *elem, const void *data))
{
	size_t count;
	u8 *dst, *src, *src_end;
	count = 0;
	dst = base;
	src = base;
	src_end = src + num * width;
	while(src < src_end)
	{
		if(keep(src, data))
		{
			if(dst != src)
			{
				memcpy(dst, src, width);
			}

			++count;
			dst += width;
		}

		src += width;
	}

	return count;
}
