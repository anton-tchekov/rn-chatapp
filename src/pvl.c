#include "pvl.h"
#include "util.h"
#include "net_util.h"
#include "crc.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>

static void w16(u8 *buf, u16 val)
{
	buf[0] = val >> 8;
	buf[1] = val;
}

static void w32(u8 *buf, u32 val)
{
	buf[0] = val >> 24;
	buf[1] = val >> 16;
	buf[2] = val >> 8;
	buf[3] = val;
}

static u16 r16(const u8 *buf)
{
	return (((u16)buf[0]) << 8) | ((u16)buf[1]);
}

static u32 r32(const u8 *buf)
{
	return (((u32)buf[0]) << 24) |
		(((u32)buf[1]) << 16) |
		(((u32)buf[2]) << 8) |
		((u32)buf[3]);
}

u16 pvl_get_length(const u8 *buf)
{
	return r16(buf + PVL_OFFSET_LENGTH);
}

u32 pvl_get_crc(const u8 *buf)
{
	return r32(buf + PVL_OFFSET_CRC);
}

u8 pvl_get_version(const u8 *buf)
{
	return buf[PVL_OFFSET_VERSION];
}

u8 pvl_get_msgtype(const u8 *buf)
{
	return buf[PVL_OFFSET_MSGTYPE];
}

u32 pvl_get_ttl(const u8 *buf)
{
	return r32(buf + PVL_OFFSET_TTL);
}

void pvl_set_length(u8 *buf, u16 val)
{
	w16(buf + PVL_OFFSET_LENGTH, val);
}

void pvl_set_crc(u8 *buf, u32 val)
{
	w32(buf + PVL_OFFSET_CRC, val);
}

void pvl_set_version(u8 *buf)
{
	buf[PVL_OFFSET_VERSION] = PVL_VERSION;
}

void pvl_set_msgtype(u8 *buf, u8 msgtype)
{
	buf[PVL_OFFSET_MSGTYPE] = msgtype;
}

void pvl_set_ttl(u8 *buf, u32 ttl)
{
	w32(buf + PVL_OFFSET_TTL, ttl);
}

void pvl_set_dst(u8 *buf, ip_addr val)
{
	w32(buf + PVL_OFFSET_DST, val);
}

void pvl_set_src(u8 *buf, ip_addr val)
{
	w32(buf + PVL_OFFSET_SRC, val);
}

void pvl_set_msgid(u8 *buf, u32 val)
{
	w32(buf + PVL_OFFSET_MSGID, val);
}

ip_addr pvl_get_dst(const u8 *buf)
{
	return r32(buf + PVL_OFFSET_DST);
}

ip_addr pvl_get_src(const u8 *buf)
{
	return r32(buf + PVL_OFFSET_SRC);
}

u32 pvl_get_msgid(const u8 *buf)
{
	return r32(buf + PVL_OFFSET_MSGID);
}

void pvl_set_msg_data(u8 *buf, const char *data, size_t len)
{
	memcpy(buf + PVL_OFFSET_MSG_DATA, data, len);
}

const char *pvl_get_msg_data(const u8 *buf)
{
	return (const char *)(buf + PVL_OFFSET_MSG_DATA);
}

size_t pvl_total_len(const u8 *buf)
{
	size_t len = PVL_HEADER_SIZE + pvl_get_length(buf);
	switch(pvl_get_msgtype(buf))
	{
	case PVL_MESSAGE:
		len += PVL_MSG_HEADER_SIZE;
		break;

	case PVL_ACK:
		len += PVL_ACK_HEADER_SIZE;
		break;

	case PVL_NACK:
		len += PVL_NACK_HEADER_SIZE;
		break;
	}

	return len;
}

int pvl_msgtype_valid(PvlMsgType type)
{
	size_t idx = type;
	return idx < PVL_MSGTYPE_CNT;
}

const char *pvl_msgtype_str(PvlMsgType type)
{
	static const char *names[] =
	{
		FOREACH_MSGTYPE(GENERATE_STRING)
	};

	size_t idx = type;
	assert(idx < ARRLEN(names));
	return names[type];
}

int pvl_version_valid(u32 version)
{
	return version == PVL_VERSION;
}

u32 pvl_calc_crc(const u8 *buf)
{
	size_t size = pvl_total_len(buf);
	u32 crc = crc_update(0, buf, PVL_OFFSET_CRC);
	return crc_update(crc, buf + PVL_HEADER_SIZE, size - PVL_HEADER_SIZE);
}

void pvl_print_header(const u8 *header)
{
	size_t length = pvl_get_length(header);
	PvlMsgType type = pvl_get_msgtype(header);
	int version = pvl_get_version(header);
	const char *type_str = pvl_msgtype_valid(type) ?
		pvl_msgtype_str(type) : "INVALID";

	printf("\n\n--- PVL HEADER ---\n"
		"Version: %d (%s)\n"
		"MsgType: %d (%s)\n"
		"Payload Length: %zu bytes\n"
		"CRC: %08X\n",
		version, pvl_version_valid(version) ? "VALID" : "INVALID",
		type, type_str,
		length,
		pvl_get_crc(header));
}

void pvl_print_msgheader(const u8 *buf)
{
	char buf_src[IPV4_STRBUF], buf_dst[IPV4_STRBUF];
	printf("\n\n--- TEXT MESSAGE HEADER ---\n"
		"Dst IP: %s\n"
		"Src IP: %s\n"
		"Message ID: %"PRIu32"\n"
		"TTL: %"PRIu32"\n",
		ip_to_str(buf_dst, pvl_get_dst(buf)),
		ip_to_str(buf_src, pvl_get_src(buf)),
		pvl_get_msgid(buf),
		pvl_get_ttl(buf));
}

ip_addr pvl_get_route_dst(const u8 *buf, int i)
{
	return r32(buf + PVL_HEADER_SIZE + i * PVL_ROUTE_SIZE + PVL_OFFSET_ROUTE_DST);
}

u32 pvl_get_route_hops(const u8 *buf, int i)
{
	return r32(buf + PVL_HEADER_SIZE + i * PVL_ROUTE_SIZE + PVL_OFFSET_ROUTE_HOPS);
}

void pvl_set_route_dst(u8 *buf, int i, ip_addr dst)
{
	w32(buf + PVL_HEADER_SIZE + i * PVL_ROUTE_SIZE + PVL_OFFSET_ROUTE_DST, dst);
}

void pvl_set_route_hops(u8 *buf, int i, u32 hops)
{
	w32(buf + PVL_HEADER_SIZE + i * PVL_ROUTE_SIZE + PVL_OFFSET_ROUTE_HOPS, hops);
}

void pvl_set_nack_status(u8 *buf, u32 status)
{
	w32(buf + PVL_OFFSET_NACK_STATUS, status);
}

u32 pvl_get_nack_status(const u8 *buf)
{
	return r32(buf + PVL_OFFSET_NACK_STATUS);
}
