#ifndef __RT_H__
#define __RT_H__

#include "net_util.h"

typedef struct
{
	ip_addr dst;
	ip_addr via;
	u32 hops;
} Route;

typedef struct
{
	size_t len;
	Route *routes;
} RT;

void rt_copy(RT *dst, RT *src);
int rt_equals(RT *a, RT *b);
void rt_init(RT *rt, size_t max);
void rt_free(RT *rt);
Route *rt_find(RT *rt, ip_addr dst);
ip_addr rt_get_via(RT *rt, ip_addr dst);
void rt_add(RT *rt, Route *ins);
void rt_add_direct(RT *rt, ip_addr ip);
void rt_remove_via(RT *rt, ip_addr via);
void rt_remove_disconn(RT *rt, ip_addr via);

#endif
