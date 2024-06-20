#define _GNU_SOURCE
#include "net.h"
#include "util.h"
#include <pthread.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <poll.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "net_util.h"

#define CMD_FD             0
#define SERVER_FD          1
#define OFFSET_FD          2
#define ACCEPT_QUEUE_SIZE  5

typedef struct
{
	u8 *buf;
	u8 *sbuf;
	size_t cap;
	size_t wp;
	size_t rp;
	ip_addr addr;
	u32 connected;
} Client;

struct Net
{
	int qfds[2];
	int sfd;
	int started;
	size_t num_clients, max_clients, bufsiz;
	pthread_t thread;
	Client *clients;
	struct pollfd *fds, *cfds;
	u16 port;
};

typedef struct
{
	u32 type;
	ip_addr dst;
	void *buf;
	size_t len;
} NetCmd;

enum
{
	NET_CMD_CONNECT,
	NET_CMD_SEND,
	NET_CMD_DISCONNECT
};

static void pollfd_start_writing(struct pollfd *pfd)
{
	pfd->events = POLLIN | POLLPRI | POLLOUT;
}

static void pollfd_done_writing(struct pollfd *pfd)
{
	pfd->events = POLLIN | POLLPRI;
}

static int fd_set_non_blocking(int fd)
{
	return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}

static void pollfd_init(struct pollfd *pfd, int fd, int flags)
{
	memset(pfd, 0, sizeof(*pfd));
	pfd->fd = fd;
	pfd->events = flags;
}

static void sockaddr_init(struct sockaddr_in *addr, ip_addr ip, uint16_t port)
{
	memset(addr, 0, sizeof(*addr));
	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	addr->sin_addr.s_addr = htonl(ip);
}

static void client_init(Client *client, size_t cap, struct sockaddr_in *cliaddr,
	int conn)
{
	client->cap = cap;
	client->buf = smalloc(cap);
	client->sbuf = smalloc(cap);
	client->connected = conn;
	client->rp = 0;
	client->wp = 0;
	client->addr = sockaddr_to_uint(cliaddr);
}

static void client_free(Client *client)
{
	sfree(client->buf);
	sfree(client->sbuf);
}

static void client_connected(Net *net, size_t i)
{
	Client *client = net->clients + i;
	struct pollfd *pfd = net->cfds + i;
	if(client->connected)
	{
		return;
	}

	net_connected(client->addr);
	client->connected = 1;
	if(!client->rp)
	{
		pollfd_done_writing(pfd);
	}
}

static int client_read(Net *net, size_t i)
{
	Client *client = net->clients + i;
	int fd = net->cfds[i].fd;
	ssize_t result;
	for(;;)
	{
		if(client->wp >= client->cap)
		{
			return -1;
		}

		result = read(fd, client->buf + client->wp, client->cap - client->wp);
		if(result == 0)
		{
			return -1;
		}
		else if(result < 0)
		{
			if(errno == EAGAIN || errno == EWOULDBLOCK)
			{
				break;
			}

			return -1;
		}

		client->wp += result;
	}

	result = net_received(client->addr, client->buf, client->wp);
	if(result < 0)
	{
		return -1;
	}

	if(result > 0)
	{
		memmove(client->buf, client->buf + result, client->wp - result);
		client->wp -= result;
	}

	return 0;
}

static int client_add_msg(Client *client,
	struct pollfd *pfd, void *buf, size_t len)
{
	if(client->rp + len > client->cap)
	{
		return -1;
	}

	pollfd_start_writing(pfd);
	memcpy(client->sbuf + client->rp, buf, len);
	client->rp += len;
	return 0;
}

static int client_write(Client *client, struct pollfd *pfd)
{
	int fd;
	ssize_t result;
	size_t pos;

	fd = pfd->fd;
	pos = 0;
	for(;;)
	{
		if(pos == client->rp)
		{
			break;
		}

		result = write(fd, client->sbuf + pos, client->rp - pos);
		if(result == 0)
		{
			return -1;
		}
		else if(result < 0)
		{
			if(errno == EAGAIN || errno == EWOULDBLOCK)
			{
				break;
			}

			return -1;
		}

		pos += result;
	}

	if(pos > 0)
	{
		memmove(client->sbuf, client->sbuf + pos, client->rp - pos);
		client->rp -= pos;
	}

	if(!client->rp)
	{
		pollfd_done_writing(pfd);
	}

	return 0;
}

static int client_send_recv(Net *net, size_t i)
{
	Client *client = net->clients + i;
	struct pollfd *pfd = net->cfds + i;

	if(pfd->fd <= 0)
	{
		return 0;
	}

	if(pfd->revents & (POLLERR | POLLHUP))
	{
		return -1;
	}

	if(pfd->revents & POLLIN)
	{
		if(client_read(net, i))
		{
			return -1;
		}
	}

	if(pfd->revents & POLLOUT)
	{
		client_connected(net, i);
		if(client_write(client, pfd))
		{
			return -1;
		}
	}

	return 0;
}

static void net_notify(Net *net, NetCmd *cmd)
{
	if(write(net->qfds[1], &cmd, sizeof(cmd)) < 0)
	{
		perror("write() to pipe failed");
		exit(1);
	}
}

static void close_checked(int *fd)
{
	if(*fd > 0)
	{
		if(close(*fd) < 0)
		{
			perror("close(fd) failed");
		}

		*fd = 0;
	}
}

static void net_free(Net *net)
{
	for(size_t i = 0; i < net->num_clients; ++i)
	{
		close_checked(&net->cfds[i].fd);
		client_free(&net->clients[i]);
	}

	close_checked(net->qfds + 0);
	close_checked(net->qfds + 1);
	close_checked(&net->sfd);

	sfree(net->fds);
	sfree(net->clients);
	sfree(net);
}

static void net_client_close(Net *net, size_t idx)
{
	struct pollfd *pfd = net->cfds + idx;
	close(pfd->fd);
	pfd->fd = 0;
	client_free(&net->clients[idx]);
}

static int server_client_add(Net *net,
	int cfd, struct sockaddr_in *cliaddr, int flags)
{
	if(net->num_clients >= net->max_clients)
	{
		return -1;
	}

	client_init(net->clients + net->num_clients, net->bufsiz, cliaddr,
		(flags & POLLOUT) ? 0 : 1);

	pollfd_init(net->cfds + net->num_clients, cfd, flags);
	++net->num_clients;
	return 0;
}

static ssize_t server_client_find(Net *net, ip_addr dst)
{
	ssize_t i;
	for(i = 0; i < (ssize_t)net->num_clients; ++i)
	{
		if(net->clients[i].addr == dst)
		{
			return i;
		}
	}

	return -1;
}

static void net_msg_connect(Net *net, ip_addr ip, u16 port)
{
	int cfd;
	char ip_buf[IPV4_STRBUF];
	ip_to_str(ip_buf, ip);

	if((cfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		net_log("socket() failed: %s", strerror(errno));
		return;
	}

	if(fd_set_non_blocking(cfd) < 0)
	{
		net_log("fcntl(O_NONBLOCK) failed: %s", strerror(errno));
		close(cfd);
		return;
	}

	struct sockaddr_in addr;
	sockaddr_init(&addr, ip, port);
	int ret = connect(cfd, (struct sockaddr *)&addr, sizeof(addr));
	net_log("Connection to %s is in progress ...", ip_buf);

	if(ret == 0 || (ret < 0 && errno == EINPROGRESS))
	{
		if(server_client_add(net, cfd, &addr, POLLIN | POLLPRI | POLLOUT) < 0)
		{
			net_log("Maximum number of clients reached");
			close(cfd);
			return;
		}

		if(ret == 0)
		{
			client_connected(net, net->num_clients - 1);
		}

		return;
	}

	net_log("connect(%s, %d) failed: %s", ip_buf, port, strerror(errno));
	close(cfd);
}

static void net_msg_send(Net *net, ip_addr dst, void *buf, size_t len)
{
	ssize_t cli = server_client_find(net, dst);
	if(cli < 0)
	{
		char ip_buf[IPV4_STRBUF];
		net_log("Connection to %s not found", ip_to_str(ip_buf, dst));
		return;
	}

	client_add_msg(net->clients + cli, net->cfds + cli, buf, len);
}

static void net_msg_disconnect(Net *net, ip_addr dst)
{
	ssize_t cli = server_client_find(net, dst);
	if(cli < 0)
	{
		char ip_buf[IPV4_STRBUF];
		net_log("Connection to %s not found", ip_to_str(ip_buf, dst));
		return;
	}

	net_client_close(net, cli);
}

static void net_cmd_handle(Net *net, NetCmd *msg)
{
	switch(msg->type)
	{
	case NET_CMD_CONNECT:
		net_msg_connect(net, msg->dst, net->port);
		break;

	case NET_CMD_DISCONNECT:
		net_msg_disconnect(net, msg->dst);
		break;

	case NET_CMD_SEND:
		net_msg_send(net, msg->dst, msg->buf, msg->len);
		sfree(msg->buf);
		break;
	}

	sfree(msg);
}

static int net_cmd_check(Net *net)
{
	struct pollfd *spfd = net->fds + CMD_FD;
	if(!(spfd->revents & POLLIN))
	{
		return 0;
	}

	for(;;)
	{
		NetCmd *msg = NULL;
		if(read(spfd->fd, &msg, sizeof(msg)) < 0)
		{
			if(errno == EAGAIN || errno == EWOULDBLOCK)
			{
				break;
			}

			return -1;
		}

		if(!msg)
		{
			return -1;
		}

		net_cmd_handle(net, msg);
	}

	return 0;
}

static int net_accept(Net *net)
{
	if(!(net->fds[SERVER_FD].revents & POLLIN))
	{
		return 0;
	}

	for(;;)
	{
		int cfd;
		struct sockaddr_in cliaddr;
		socklen_t addrlen = sizeof(cliaddr);
		if((cfd = accept(net->sfd, (struct sockaddr *)&cliaddr, &addrlen)) < 0)
		{
			if(errno == EAGAIN || errno == EWOULDBLOCK)
			{
				break;
			}

			net_log("accept() failed: %s", strerror(errno));
			return -1;
		}

		if(fd_set_non_blocking(cfd) < 0)
		{
			net_log("fcntl(O_NONBLOCK) failed: %s", strerror(errno));
			close(cfd);
			return -1;
		}

		net_log("Accepted client (%s)", inet_ntoa(cliaddr.sin_addr));
		if(server_client_add(net, cfd, &cliaddr, POLLIN | POLLPRI) < 0)
		{
			close(cfd);
			return -1;
		}

		net_connected(sockaddr_to_uint(&cliaddr));
	}

	return 0;
}

static void net_remove_closed(Net *net)
{
	size_t src, dst;
	Client *clients = net->clients;
	struct pollfd *fds = net->cfds;
	for(src = 0, dst = 0; src < net->num_clients; ++src)
	{
		if(fds[src].fd > 0)
		{
			if(dst != src)
			{
				clients[dst] = clients[src];
				fds[dst] = fds[src];
			}

			++dst;
		}
		else
		{
			if(clients[dst].connected)
			{
				net_disconnected(clients[dst].addr);
			}
			else
			{
				char buf[IPV4_STRBUF];
				net_log("Failed to connect to %s",
					ip_to_str(buf, clients[dst].addr));
			}
		}
	}

	net->num_clients = dst;
}

static void net_send_recv(Net *net)
{
	size_t i;
	for(i = 0; i < net->num_clients; ++i)
	{
		if(client_send_recv(net, i) < 0)
		{
			net_client_close(net, i);
		}
	}
}

static int net_update(Net *net)
{
	int result = poll(net->fds, net->num_clients + OFFSET_FD, -1);
	if(!result)
	{
		/* Timed out */
		return 0;
	}
	else if(result < 0)
	{
		if(errno == EINTR || errno == EAGAIN)
		{
			/* Repeat call */
			return 0;
		}

		perror("poll() failed");
		return -1;
	}

	if(net_cmd_check(net))
	{
		return -1;
	}

	net_accept(net);
	net_send_recv(net);
	net_remove_closed(net);
	return 0;
}

static void *net_thread_poll(void *args)
{
	while(!net_update(args)) {}
	return NULL;
}

static void net_init_clients(Net *net, size_t max_clients)
{
	net->num_clients = 0;
	net->max_clients = max_clients;
	net->fds = smalloc((max_clients + OFFSET_FD) * sizeof(*net->fds));
	net->cfds = net->fds + OFFSET_FD;
	net->clients = smalloc(max_clients * sizeof(*net->clients));
}

static int net_init_thread(Net *net)
{
	if(pthread_create(&net->thread, NULL, net_thread_poll, net))
	{
		perror("pthread_create() failed");
		return -1;
	}

	net->started = 1;
	return 0;
}

static int net_init_cmd_pipe(Net *net)
{
	if(pipe2(net->qfds, O_DIRECT | O_NONBLOCK))
	{
		perror("pipe()");
		return -1;
	}

	pollfd_init(net->fds + CMD_FD, net->qfds[0], POLLIN | POLLPRI);
	return 0;
}

static int net_init_socket(Net *net)
{
	if((net->sfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("socket() failed");
		return -1;
	}

	if(fd_set_non_blocking(net->sfd) < 0)
	{
		perror("fcntl(O_NONBLOCK) failed");
		return -1;
	}

	int en = 1;
	if(setsockopt(net->sfd, SOL_SOCKET, SO_REUSEADDR, &en, sizeof(en)) < 0)
	{
		perror("setsockopt(SO_REUSEADDR) failed");
		return -1;
	}

	struct sockaddr_in addr;
	sockaddr_init(&addr, INADDR_ANY, net->port);
	if(bind(net->sfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("bind() failed");
		return -1;
	}

	if(listen(net->sfd, ACCEPT_QUEUE_SIZE) < 0)
	{
		perror("listen() failed");
		return -1;
	}

	pollfd_init(&net->fds[SERVER_FD], net->sfd, POLLIN | POLLPRI);
	return 0;
}

Net *net_start(size_t max_clients, size_t buf_size, u16 port)
{
	Net *net = smalloc(sizeof(*net));
	memset(net, 0, sizeof(*net));
	net->port = port;
	net->bufsiz = buf_size;
	net_init_clients(net, max_clients);
	if(net_init_cmd_pipe(net) ||
		net_init_socket(net) ||
		net_init_thread(net))
	{
		net_free(net);
		return NULL;
	}

	return net;
}

void net_quit(Net *net)
{
	net_notify(net, NULL);
	if(pthread_join(net->thread, NULL))
	{
		perror("pthread_join() failed");
		exit(1);
	}

	net_free(net);
}

void net_send(Net *net, ip_addr dst, void *buf, size_t len)
{
	NetCmd *cmd = smalloc(sizeof(*cmd));
	cmd->type = NET_CMD_SEND;
	cmd->dst = dst;
	cmd->buf = buf;
	cmd->len = len;
	net_notify(net, cmd);
}

void net_connect(Net *net, ip_addr dst)
{
	NetCmd *cmd = smalloc(sizeof(*cmd));
	cmd->type = NET_CMD_CONNECT;
	cmd->dst = dst;
	cmd->buf = NULL;
	cmd->len = 0;
	net_notify(net, cmd);
}

void net_disconnect(Net *net, ip_addr dst)
{
	NetCmd *cmd = smalloc(sizeof(*cmd));
	cmd->type = NET_CMD_DISCONNECT;
	cmd->dst = dst;
	cmd->buf = NULL;
	cmd->len = 0;
	net_notify(net, cmd);
}
