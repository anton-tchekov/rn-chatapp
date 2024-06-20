#include "rt.h"
#include "util.h"
#include <string.h>

void rt_init(RT *rt, size_t max)
{
	rt->len = 0;
	rt->routes = smalloc(max * sizeof(Route));
}

void rt_copy(RT *dst, RT *src)
{
	dst->len = src->len;
	memcpy(dst->routes, src->routes, dst->len * sizeof(Route));
}

int rt_equals(RT *a, RT *b)
{
	if(a->len != b->len)
	{
		return 0;
	}

	for(size_t i = 0; i < a->len; ++i)
	{
		Route *x, *y;
		x = a->routes + i;
		y = b->routes + i;

		if(x->dst != y->dst ||
			x->via != y->via ||
			x->hops != y->hops)
		{
			return 0;
		}
	}

	return 1;
}

void rt_free(RT *rt)
{
	sfree(rt->routes);
}

ip_addr rt_get_via(RT *rt, ip_addr dst)
{
	for(size_t i = 0; i < rt->len; ++i)
	{
		if(rt->routes[i].dst == dst)
		{
			return rt->routes[i].via;
		}
	}

	return 0;
}

Route *rt_find(RT *rt, ip_addr dst)
{
	for(size_t i = 0; i < rt->len; ++i)
	{
		if(rt->routes[i].dst == dst)
		{
			return rt->routes + i;
		}
	}

	return NULL;
}

void rt_add(RT *rt, Route *ins)
{
	Route *re;
	if((re = rt_find(rt, ins->dst)))
	{
		if(re->hops > ins->hops)
		{
			*re = *ins;
		}
		return;
	}

	rt->routes[rt->len++] = *ins;
}

void rt_add_direct(RT *rt, ip_addr ip)
{
	Route ins;
	ins.dst = ip;
	ins.via = ip;
	ins.hops = 1;
	rt_add(rt, &ins);
}

static int rt_filter_via(void *elem, const void *data)
{
	ip_addr ip = *(const ip_addr *)data;
	const Route *cur = elem;
	return cur->via == cur->dst || cur->via != ip;
}

void rt_remove_via(RT *rt, ip_addr via)
{
	rt->len = filter(
		rt->routes, rt->len,
		sizeof(*rt->routes), &via,
		rt_filter_via);
}

static int rt_filter_disconn(void *elem, const void *data)
{
	ip_addr ip = *(const ip_addr *)data;
	const Route *cur = elem;
	return cur->via != ip;
}

void rt_remove_disconn(RT *rt, ip_addr via)
{
	rt->len = filter(
		rt->routes, rt->len,
		sizeof(*rt->routes), &via,
		rt_filter_disconn);
}
