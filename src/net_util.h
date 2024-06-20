#ifndef __NET_UTIL_H__
#define __NET_UTIL_H__

#include "types.h"
#include <netinet/in.h>

#define IPV4_STRBUF 16

typedef u32 ip_addr;

ip_addr str_to_ip(const char *str);
char *ip_to_str(char *out, ip_addr ip);
ip_addr sockaddr_to_uint(struct sockaddr_in *addr);
ip_addr getip(void);

#endif
