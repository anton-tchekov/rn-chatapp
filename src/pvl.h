#ifndef __PVL_H__
#define __PVL_H__

#include "types.h"
#include "util.h"
#include "net_util.h"

#define PVL_DEFAULT_TTL        15

#define PVL_ACK_HEADER_SIZE    16
#define PVL_NACK_HEADER_SIZE   20
#define PVL_HEADER_SIZE         8
#define PVL_MSG_HEADER_SIZE    16

#define PVL_OFFSET_VERSION      0
#define PVL_OFFSET_MSGTYPE      1
#define PVL_OFFSET_LENGTH       2
#define PVL_OFFSET_CRC          4

#define PVL_OFFSET_DST          8
#define PVL_OFFSET_SRC         12
#define PVL_OFFSET_MSGID       16
#define PVL_OFFSET_TTL         20
#define PVL_OFFSET_NACK_STATUS 24
#define PVL_OFFSET_MSG_DATA    24

#define PVL_ROUTE_SIZE          8

#define PVL_OFFSET_ROUTE_DST    0
#define PVL_OFFSET_ROUTE_HOPS   4

#define PVL_VERSION             1

#define FOREACH_MSGTYPE(MSGTYPE) \
	MSGTYPE(PVL_MESSAGE), \
	MSGTYPE(PVL_ROUTING), \
	MSGTYPE(PVL_ACK), \
	MSGTYPE(PVL_NACK), \
	MSGTYPE(PVL_PING), \
	MSGTYPE(PVL_PONG) \

typedef enum
{
	FOREACH_MSGTYPE(GENERATE_ENUM),
	PVL_MSGTYPE_CNT
} PvlMsgType;

void pvl_print_routing(const u8 *buf);
void pvl_print_msgheader(const u8 *buf);
void pvl_print_header(const u8 *header);

u32 pvl_calc_crc(const u8 *buf);
size_t pvl_total_len(const u8 *buf);

void pvl_set_length(u8 *buf, u16 val);
u16 pvl_get_length(const u8 *buf);

void pvl_set_crc(u8 *buf, u32 val);
u32 pvl_get_crc(const u8 *buf);

void pvl_set_version(u8 *buf);
u8 pvl_get_version(const u8 *buf);

void pvl_set_msgtype(u8 *buf, u8 msgtype);
u8 pvl_get_msgtype(const u8 *buf);

void pvl_set_msg_data(u8 *buf, const char *data, size_t len);
const char *pvl_get_msg_data(const u8 *buf);

ip_addr pvl_get_route_dst(const u8 *buf, int i);
void pvl_set_route_dst(u8 *buf, int i, ip_addr dst);

u32 pvl_get_route_hops(const u8 *buf, int i);
void pvl_set_route_hops(u8 *buf, int i, u32 hops);

int pvl_msgtype_valid(PvlMsgType type);
const char *pvl_msgtype_str(PvlMsgType type);
int pvl_version_valid(u32 version);

void pvl_set_dst(u8 *buf, ip_addr dst);
void pvl_set_src(u8 *buf, ip_addr src);
void pvl_set_msgid(u8 *buf, u32 msgid);
void pvl_set_ttl(u8 *buf, u32 ttl);

ip_addr pvl_get_dst(const u8 *buf);
ip_addr pvl_get_src(const u8 *buf);
u32 pvl_get_msgid(const u8 *buf);
u32 pvl_get_ttl(const u8 *buf);

void pvl_set_nack_status(u8 *buf, u32 status);
u32 pvl_get_nack_status(const u8 *buf);

#endif
