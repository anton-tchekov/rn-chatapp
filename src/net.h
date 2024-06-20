#ifndef __NET_H__
#define __NET_H__

#include "types.h"
#include "net_util.h"

typedef struct Net Net;

typedef struct
{
	int type;
} NetEvent;

Net *net_start(size_t max_clients, size_t buf_size, u16 port);

void net_log(const char *msg, ...);
void net_disconnected(ip_addr addr);
void net_connected(ip_addr addr);
ssize_t net_received(ip_addr addr, const u8 *buf, size_t size);

void net_quit(Net *net);
void net_send(Net *net, ip_addr dst, void *buf, size_t len);
void net_connect(Net *net, ip_addr dst);
void net_disconnect(Net *net, ip_addr ip);

#endif
