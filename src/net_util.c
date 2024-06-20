#include "net_util.h"
#include <stdio.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

ip_addr str_to_ip(const char *str)
{
	return ntohl(inet_addr(str));
}

char *ip_to_str(char *out, ip_addr ip)
{
	sprintf(out, "%d.%d.%d.%d",
		(ip >> 24) & 0xFF,
		(ip >> 16) & 0xFF,
		(ip >> 8) & 0xFF,
		ip & 0xFF);

	return out;
}

ip_addr sockaddr_to_uint(struct sockaddr_in *addr)
{
	return ntohl(addr->sin_addr.s_addr);
}

ip_addr getip(void)
{
	ip_addr ip = 0;
	struct ifaddrs *tmp, *addrs;
	getifaddrs(&addrs);
	tmp = addrs;
	while(tmp)
	{
		if(tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET)
		{
			ip_addr v = sockaddr_to_uint((struct sockaddr_in *)tmp->ifa_addr);
			if(v != 0x7F000001)
			{
				ip = v;
				if((ip >> 24) == 141)
				{
					break;
				}
			}
		}

		tmp = tmp->ifa_next;
	}

	freeifaddrs(addrs);
	return ip;
}
