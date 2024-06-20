#include <stdio.h>
#include "crc.h"
#include "gui.h"
#include "gfx.h"
#include "kbd.h"
#include "net_util.h"
#include "net.h"
#include "pvl.h"
#include "rt.h"
#include "util.h"
#include "config.h"
#include "layout.h"
#include "terminal.h"
#include <stdarg.h>
#include <SDL2/SDL.h>

enum
{
	MODE_ROUTES,
	MODE_LOGGER,
	MODE_CHAT
};

static Terminal logger;
static Net *net;
static u32 cur_partner = 0;
static int mode = MODE_LOGGER;
static ip_addr my_ip;
static u32 msg_id = 0;
static RT rt, rt_prev;

typedef struct
{
	ip_addr ip;
	char name[60];
	Terminal term;
} Alias;

static Alias names[MAXCLIENTS];
static size_t numnames;

static void addalias(ip_addr ip, const char *name)
{
	for(size_t i = 0; i < numnames; ++i)
	{
		if(names[i].ip == ip)
		{
			strcpy(names[i].name, name);
			return;
		}
	}

	term_init(&names[numnames].term, 64);
	strcpy(names[numnames].name, name);
	names[numnames].ip = ip;
	++numnames;
}

static char *getalias(ip_addr ip)
{
	for(size_t i = 0; i < numnames; ++i)
	{
		if(names[i].ip == ip)
		{
			return names[i].name;
		}
	}

	return NULL;
}

static Terminal *term_sel(ip_addr ip)
{
	for(size_t i = 0; i < numnames; ++i)
	{
		if(names[i].ip == ip)
		{
			return &names[i].term;
		}
	}

	return NULL;
}

static void update_gui_routes(void)
{
	size_t i;
	for(i = 0; i < rt.len; ++i)
	{
		Button *b = lst_members + i;
		ip_addr ip = rt.routes[i].dst;
		char *al = getalias(ip);
		if(al)
		{
			strcpy(b->Text, al);
		}
		else
		{
			ip_to_str(b->Text, ip);
		}

		b->Tag = rt.routes[i].dst;
		b->Flags &= ~FLAG_INVISIBLE;
	}

	for(; i < MAXCLIENTS; ++i)
	{
		Button *b = lst_members + i;
		b->Flags |= FLAG_INVISIBLE;
	}

	gfx_notify();
}

void net_log(const char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	term_print_va(&logger, TAG_LOG, msg, args);
	va_end(args);
}

static void pvl_send_rt(ip_addr dst)
{
	size_t len_bytes = rt.len * 8;
	size_t size = PVL_HEADER_SIZE + len_bytes;
	u8 *buf = scalloc(size);
	pvl_set_version(buf);
	pvl_set_msgtype(buf, PVL_ROUTING);
	pvl_set_length(buf, len_bytes);
	for(size_t i = 0; i < rt.len; ++i)
	{
		pvl_set_route_dst(buf, i, rt.routes[i].dst);
		pvl_set_route_hops(buf, i, rt.routes[i].hops);
	}

	pvl_set_crc(buf, pvl_calc_crc(buf));
	net_send(net, dst, buf, size);
}

static void pvl_broadcast_rt(void)
{
	for(size_t i = 0; i < rt.len; ++i)
	{
		Route *cur = rt.routes + i;
		if(cur->hops != 1)
		{
			continue;
		}

		pvl_send_rt(cur->via);
	}
}

void net_connected(ip_addr ip)
{
	char ipb[IPV4_STRBUF];
	term_print(&logger, TAG_LOG, "%s connected", ip_to_str(ipb, ip));
	rt_copy(&rt_prev, &rt);
	rt_add_direct(&rt, ip);
	addalias(ip, ipb);
	update_gui_routes();
	if(!rt_equals(&rt, &rt_prev))
	{
		pvl_broadcast_rt();
	}
}

void net_disconnected(ip_addr ip)
{
	char ipb[IPV4_STRBUF];
	term_print(&logger, TAG_LOG, "%s disconnected", ip_to_str(ipb, ip));
	rt_copy(&rt_prev, &rt);
	rt_remove_disconn(&rt, ip);
	if(cur_partner == ip)
	{
		btn_logger_clicked(NULL);
	}

	if(!rt_equals(&rt, &rt_prev))
	{
		pvl_broadcast_rt();
	}

	update_gui_routes();
}

static int pvl_send_msg(ip_addr dst, const char *msg, size_t len)
{
	ip_addr via = rt_get_via(&rt, dst);
	if(!via)
	{
		term_print(&logger, TAG_LOG, "No route to host");
		return 1;
	}

	size_t size = PVL_HEADER_SIZE + PVL_MSG_HEADER_SIZE + len;
	u8 *buf = scalloc(size);
	pvl_set_version(buf);
	pvl_set_msgtype(buf, PVL_MESSAGE);
	pvl_set_length(buf, len);

	pvl_set_dst(buf, dst);
	pvl_set_src(buf, my_ip);
	pvl_set_msgid(buf, msg_id);
	++msg_id;

	pvl_set_ttl(buf, 15);
	pvl_set_msg_data(buf, msg, len);

	pvl_set_crc(buf, pvl_calc_crc(buf));

	net_send(net, via, buf, size);
	return 0;
}

static int pvl_send_ack(ip_addr dst, u32 msgid)
{
	ip_addr via = rt_get_via(&rt, dst);
	if(!via)
	{
		term_print(&logger, TAG_LOG, "No route to host");
		return 1;
	}

	size_t size = PVL_HEADER_SIZE + PVL_ACK_HEADER_SIZE;
	u8 *buf = scalloc(size);
	pvl_set_version(buf);
	pvl_set_msgtype(buf, PVL_ACK);
	pvl_set_length(buf, 0);

	pvl_set_dst(buf, dst);
	pvl_set_src(buf, my_ip);
	pvl_set_msgid(buf, msgid);
	pvl_set_ttl(buf, PVL_DEFAULT_TTL);

	pvl_set_crc(buf, pvl_calc_crc(buf));
	net_send(net, via, buf, size);
	return 0;
}

static int pvl_send_nack(ip_addr dst, u32 msgid, u32 status)
{
	ip_addr via = rt_get_via(&rt, dst);
	if(!via)
	{
		term_print(&logger, TAG_LOG, "No route to host");
		return 1;
	}

	size_t size = PVL_HEADER_SIZE + PVL_NACK_HEADER_SIZE;
	u8 *buf = scalloc(size);
	pvl_set_version(buf);
	pvl_set_msgtype(buf, PVL_NACK);
	pvl_set_length(buf, 0);

	pvl_set_dst(buf, dst);
	pvl_set_src(buf, my_ip);
	pvl_set_msgid(buf, msgid);
	pvl_set_ttl(buf, PVL_DEFAULT_TTL);
	pvl_set_nack_status(buf, status);

	pvl_set_crc(buf, pvl_calc_crc(buf));
	net_send(net, via, buf, size);
	return 0;
}

static void pvl_send_ping(ip_addr dst)
{
	size_t size = PVL_HEADER_SIZE;
	u8 *buf = scalloc(size);
	pvl_set_version(buf);
	pvl_set_msgtype(buf, PVL_PING);
	pvl_set_length(buf, 0);

	pvl_set_crc(buf, pvl_calc_crc(buf));
	net_send(net, dst, buf, size);
}

static void pvl_send_pong(ip_addr dst)
{
	size_t size = PVL_HEADER_SIZE;
	u8 *buf = scalloc(size);
	pvl_set_version(buf);
	pvl_set_msgtype(buf, PVL_PONG);
	pvl_set_length(buf, 0);

	pvl_set_crc(buf, pvl_calc_crc(buf));
	net_send(net, dst, buf, size);
}

static void pvl_print_ack(ip_addr src, u32 msgid, uint32_t m)
{
	Terminal *term;
	if(!(term = term_sel(src)))
	{
		return;
	}

	for(u32 i = 0; i < term->num_lines; ++i)
	{
		TLine *line = term_line(term, i);
		if(!line)
		{
			break;
		}

		if(line->tag == msgid)
		{
			line->tag = m;
			break;
		}
	}
}

static void pvl_print_msg(ip_addr src, u32 len, const char *buf)
{
	Terminal *term;
	if(!(term = term_sel(src)))
	{
		return;
	}

	term_print(term, TAG_RECV, "%.*s", len, buf);
}

static void pvl_print_my_msg(ip_addr src, u32 msgid, u32 len, const char *buf)
{
	Terminal *term;
	if(!(term = term_sel(src)))
	{
		return;
	}

	term_print(term, msgid, "%.*s", len, buf);
}

static void pvl_handle_rt(u32 src, const u8 *buf)
{
	int length = pvl_get_length(buf);
	printf("\n\n--- ROUTING INFO ---\n");
	if(length % PVL_ROUTE_SIZE != 0)
	{
		printf("Invalid routing table length %d, must be a multiple of PVL_ROUTE_SIZE = %d\n",
			length, PVL_ROUTE_SIZE);
		return;
	}

	rt_copy(&rt_prev, &rt);
	rt_remove_via(&rt, src);

	length /= PVL_ROUTE_SIZE;
	for(int i = 0; i < length; ++i)
	{
		char ipb[IPV4_STRBUF];
		Route ins = { pvl_get_route_dst(buf, i), src, pvl_get_route_hops(buf, i) + 1 };

		ip_to_str(ipb, ins.dst);
		if(ins.dst == my_ip || ins.dst == src)
		{
			continue;
		}

		printf("Route %d: Dst IP %s (%d Hops)\n", i, ipb, ins.hops);

		rt_add(&rt, &ins);
		addalias(ins.dst, ipb);
	}

	if(!rt_equals(&rt, &rt_prev))
	{
		pvl_broadcast_rt();
		update_gui_routes();
	}
}

#define NACK_UNREACHABLE 1
#define NACK_TTL         2
#define NACK_CRC         3

static void pvl_forward(const u8 *buf)
{
	u32 len = pvl_total_len(buf);
	ip_addr dst = pvl_get_dst(buf);
	ip_addr src = pvl_get_src(buf);
	ip_addr via = rt_get_via(&rt, dst);
	if(!via)
	{
		term_print(&logger, TAG_LOG, "No route to host while forwarding, sending NACK");
		pvl_send_nack(src, pvl_get_msgid(buf), NACK_UNREACHABLE);
	}

	int ttl = pvl_get_ttl(buf);
	--ttl;
	if(ttl <= 0)
	{
		term_print(&logger, TAG_LOG, "TTL expired");
		pvl_send_nack(src, pvl_get_msgid(buf), NACK_TTL);
	}

	u8 *msg = smalloc(len);
	memcpy(msg, buf, len);
	pvl_set_ttl(msg, ttl);
	pvl_set_crc(msg, pvl_calc_crc(msg));
	net_send(net, via, msg, len);
}

static ssize_t pvl_read_msg(ip_addr ip, const u8 *buf, size_t len)
{
	char ipb[IPV4_STRBUF];
	ip_to_str(ipb, ip);

	if(len < PVL_HEADER_SIZE)
	{
		return 0;
	}

	u32 version = pvl_get_version(buf);
	if(!pvl_version_valid(version))
	{
		term_print(&logger, TAG_LOG, "Received invalid protocol version %d, closing connection with %s", version, ipb);
		return -1;
	}

	u32 msgtype = pvl_get_msgtype(buf);
	if(!pvl_msgtype_valid(msgtype))
	{
		term_print(&logger, TAG_LOG, "Received invalid message type %d, closing connection with %s", msgtype, ipb);
		return -1;
	}

	size_t total_len = pvl_total_len(buf);
	if(len < total_len)
	{
		return 0;
	}

	printf("total_len = %d\n", (int)total_len);

	u32 recv_crc = pvl_get_crc(buf);
	u32 calc_crc = pvl_calc_crc(buf);
	if(recv_crc != calc_crc)
	{
		term_print(&logger, TAG_LOG, "Received CRC %08X != calculated %08X, closing connection with %s",
			recv_crc, calc_crc, ipb);
		/* return -1; */
	}

	pvl_print_header(buf);
	switch(msgtype)
	{
	case PVL_MESSAGE:
		{
			pvl_print_msgheader(buf);

			ip_addr dst = pvl_get_dst(buf);
			ip_addr src = pvl_get_src(buf);

			char dstb[IPV4_STRBUF];
			ip_to_str(dstb, dst);

			term_print(&logger, TAG_LOG, "%s -> %s: %.*s",
				ipb, dstb, pvl_get_length(buf), pvl_get_msg_data(buf));

			if(dst == my_ip)
			{
				pvl_print_msg(src, pvl_get_length(buf), pvl_get_msg_data(buf));
				pvl_send_ack(src, pvl_get_msgid(buf));
			}
			else
			{
				pvl_forward(buf);
			}
		}
		break;

	case PVL_ROUTING:
		pvl_handle_rt(ip, buf);
		break;

	case PVL_ACK:
		{
			ip_addr dst = pvl_get_dst(buf);
			ip_addr src = pvl_get_src(buf);
			u32 msgid = pvl_get_msgid(buf);
			if(dst == my_ip)
			{
				pvl_print_ack(src, msgid, TAG_ACK);
			}
			else
			{
				pvl_forward(buf);
			}
		}
		break;

	case PVL_NACK:
		{
			ip_addr dst = pvl_get_dst(buf);
			ip_addr src = pvl_get_src(buf);
			u32 msgid = pvl_get_msgid(buf);
			if(dst == my_ip)
			{
				pvl_print_ack(src, msgid, TAG_NACK);
			}
			else
			{
				pvl_forward(buf);
			}
		}
		break;

	case PVL_PING:
		pvl_send_pong(ip);
		break;
	}

	return total_len;
}

ssize_t net_received(ip_addr ip, const u8 *buf, size_t len)
{
	printf("net received: %zu bytes\n", len);

	ssize_t bytes = 0;
	ssize_t result;
	do
	{
		result = pvl_read_msg(ip, buf + bytes, len - bytes);
		if(result < 0)
		{
			return -1;
		}

		bytes += result;
	}
	while(result > 0);
	gfx_notify();
	return bytes;
}

static void setname_show(void)
{
	lbl_setname.Flags &= ~FLAG_INVISIBLE;
	fld_setname.Flags &= ~FLAG_INVISIBLE;
}

static void setname_hide(void)
{
	lbl_setname.Flags |= FLAG_INVISIBLE;
	fld_setname.Flags |= FLAG_INVISIBLE;
}

void fld_setname_enter(Element *e)
{
	if(!cur_partner) { return; }
	if(!fld_setname.Length) { return; }
	fld_setname.Text[fld_setname.Length] = '\0';
	addalias(cur_partner, fld_setname.Text);
	input_clear(&fld_setname);
	update_gui_routes();
	(void)e;
}

void btn_routes_clicked(Element *e)
{
	cur_partner = 0;
	mode = MODE_ROUTES;
	btn_disconnect.Flags |= FLAG_INVISIBLE;
	strcpy(lbl_view.Text, "View: Routing Table");
	setname_hide();
	(void)e;
}

void btn_logger_clicked(Element *e)
{
	cur_partner = 0;
	mode = MODE_LOGGER;
	btn_disconnect.Flags |= FLAG_INVISIBLE;
	strcpy(lbl_view.Text, "View: Logger");
	setname_hide();
	(void)e;
}

static void getnamebuf(char *namebuf, size_t bs, ip_addr addr)
{
	char ipb[IPV4_STRBUF];
	ip_to_str(ipb, addr);
	char *al = getalias(addr);
	if(al && !strcmp(al, ipb))
	{
		strncpy(namebuf, ipb, bs);
	}
	else
	{
		snprintf(namebuf, bs, "%s (%s)", al, ipb);
	}
}

void btn_member_clicked(Element *e)
{
	Button *b = (Button *)e;
	cur_partner = b->Tag;
	Route *r = rt_find(&rt, cur_partner);
	mode = MODE_CHAT;

	char namebuf[64];
	getnamebuf(namebuf, sizeof(namebuf), r->dst);
	if(r->hops == 1)
	{
		sprintf(lbl_view.Text, "View: %s - direct", namebuf);
		btn_disconnect.Flags &= ~FLAG_INVISIBLE;
	}
	else
	{
		char vianb[64];
		getnamebuf(vianb, sizeof(vianb), r->via);
		sprintf(lbl_view.Text, "View: %s via %s - %d Hops",
			namebuf, vianb, r->hops);
		btn_disconnect.Flags |= FLAG_INVISIBLE;
	}

	setname_show();
}

void btn_disconnect_clicked(Element *e)
{
	if(!cur_partner)
	{
		return;
	}

	net_disconnect(net, cur_partner);
	btn_logger_clicked(NULL);
	(void)e;
}

#define TABLE_H 20

static void table_sep(int x, int *y)
{
	font_string(x, *y, "+-----+-----------------+-----------------+------+", 0, 0);
	*y += TABLE_H;
}

static void routes_draw(void)
{
	char buf[128];
	char dst_buf[IPV4_STRBUF], via_buf[IPV4_STRBUF];

	int x = SIDEBAR_W + 2 * PADDING;
	int y = 2 * INPUT_HEIGHT + FONT_HEIGHT + 4 * PADDING;

	table_sep(x, &y);
	font_string(x, y, "| No. | Destination     | Via             | Hops |", 0, 0);
	y += TABLE_H;
	table_sep(x, &y);

	for(size_t i = 0; i < rt.len; ++i)
	{
		snprintf(buf, sizeof(buf),
			"| %3zu | %15s | %15s | %4d |",
			i,
			ip_to_str(dst_buf, rt.routes[i].dst),
			ip_to_str(via_buf, rt.routes[i].via),
			rt.routes[i].hops);

		font_string(x, y, buf, 0, 0);
		y += TABLE_H;
		table_sep(x, &y);
	}
}

static int handle_command(const char *s)
{
	static const char cmd_clear[] = "/clear";
	if(!strncmp(s, cmd_clear, sizeof(cmd_clear)))
	{
		if(mode == MODE_LOGGER)
		{
			term_clear(&logger);
		}
		else
		{
			term_clear(term_sel(cur_partner));
		}
		return 1;
	}

	return 0;
}

void btn_connect_clicked(Element *e)
{
	char *host;
	if(!fld_addr.Length)
	{
		return;
	}

	host = fld_addr.Text;
	host[fld_addr.Length] = '\0';
	net_connect(net, str_to_ip(host));
	input_clear(&fld_addr);
	(void)e;
}

static size_t append(char *buf, size_t pos, char *s, size_t len)
{
	memcpy(buf + pos, s, len);
	return pos + len;
}

static void msg_send_to(void)
{
	static char msgbuf[1024];
	size_t pos = 0;
	if(fld_nick.Length)
	{
		pos = append(msgbuf, pos, fld_nick.Text, fld_nick.Length);
		pos = append(msgbuf, pos, ": ", 2);
	}

	pos = append(msgbuf, pos, fld_msg.Text, fld_msg.Length);
	msgbuf[pos] = '\0';
	pvl_send_msg(cur_partner, msgbuf, pos);
	pvl_print_my_msg(cur_partner, msg_id - 1, fld_msg.Length, fld_msg.Text);
}

void btn_send_clicked(Element *e)
{
	if(!fld_msg.Length)
	{
		return;
	}

	fld_msg.Text[fld_msg.Length] = '\0';
	if(!handle_command(fld_msg.Text))
	{
		if(cur_partner)
		{
			msg_send_to();
		}
	}

	input_clear(&fld_msg);
	(void)e;
}

static void resized(int w, int h)
{
	width = w;
	height = h;
	layout_resize(w, h);
}

static void term_render(Terminal *term)
{
	term_draw(term,
		2 * PADDING + SIDEBAR_W,
		height - INPUT_HEIGHT - 3 * PADDING - FONT_HEIGHT,
		INPUT_HEIGHT + FONT_HEIGHT + 10 * PADDING);
}

int main(void)
{
#ifndef NDEBUG
	crc_test();
#endif

	msg_id = 0xFF;
	my_ip = getip();
	if(!my_ip)
	{
		fprintf(stderr, "Couldn't get IP Address\n");
		return 1;
	}

	rt_init(&rt_prev, MAXCLIENTS);
	rt_init(&rt, MAXCLIENTS);

	int running = 1;
	gfx_init();
	layout_init(width, height);
	btn_logger_clicked(NULL);
	term_init(&logger, 64);

	char mipb[IPV4_STRBUF];
	term_print(&logger, TAG_LOG, "My IP: %s", ip_to_str(mipb, my_ip));

	char buf[64];
	snprintf(buf, sizeof(buf), "RN Chatapp (%s)", mipb);
	gfx_set_title(buf);

	if(!(net = net_start(MAXCLIENTS, BUFSIZE, PORT)))
	{
		return 1;
	}

	while(running)
	{
		SDL_Event e;
		gui_render();

		switch(mode)
		{
		case MODE_LOGGER:
			term_render(&logger);
			break;

		case MODE_ROUTES:
			routes_draw();
			break;

		case MODE_CHAT:
			term_render(term_sel(cur_partner));
			break;
		}

		gfx_update();

		if(!SDL_WaitEvent(&e))
		{
			break;
		}

		switch(e.type)
		{
		case SDL_QUIT:
			running = 0;
			break;

		case SDL_WINDOWEVENT:
			if(e.window.event == SDL_WINDOWEVENT_RESIZED)
			{
				resized(e.window.data1, e.window.data2);
			}
			break;

		case SDL_KEYDOWN:
			{
				int key;
				if(e.key.keysym.sym == SDLK_ESCAPE)
				{
					running = 0;
					break;
				}

				key = key_convert(e.key.keysym.scancode, e.key.keysym.mod);
				gui_event_key(key, key_to_codepoint(key),
					e.key.repeat ? KEYSTATE_REPEAT : KEYSTATE_PRESSED);
			}
			break;

		case SDL_MOUSEMOTION:
			gui_mousemove(e.motion.x, e.motion.y);
			break;

		case SDL_MOUSEBUTTONDOWN:
			if(e.button.button != SDL_BUTTON_LEFT) { break; }
			gui_mousedown(e.button.x, e.button.y);
			break;

		case SDL_MOUSEBUTTONUP:
			if(e.button.button != SDL_BUTTON_LEFT) { break; }
			gui_mouseup(e.button.x, e.button.y);
			break;

		case SDL_KEYUP:
			{
				int key = key_convert(e.key.keysym.scancode, e.key.keysym.mod);
				gui_event_key(key, key_to_codepoint(key), KEYSTATE_RELEASED);
			}
			break;
		}
	}

	net_quit(net);
	term_free(&logger);
	for(size_t i = 0; i < numnames; ++i)
	{
		term_free(&names[i].term);
	}

	layout_free();
	gfx_destroy();
	rt_free(&rt);
	rt_free(&rt_prev);
	print_allocs();
	return 0;
}
