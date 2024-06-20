#ifndef __LAYOUT_H__
#define __LAYOUT_H__

#include "types.h"
#include "gui.h"
#include "config.h"

#define PADDING      10
#define SIDEBAR_W   280
#define BTN_W       160
#define NICK_W      280

extern Input fld_addr;
extern Input fld_msg;
extern Input fld_nick;
extern Button lst_members[MAXCLIENTS];
extern Button btn_disconnect;
extern Label lbl_view;

extern Label lbl_setname;
extern Input fld_setname;

void layout_resize(i32 width, i32 height);
void layout_init(i32 width, i32 height);
void layout_free(void);

void fld_setname_enter(Element *e);
void btn_connect_clicked(Element *e);
void btn_disconnect_clicked(Element *e);
void btn_routes_clicked(Element *e);
void btn_member_clicked(Element *e);
void btn_logger_clicked(Element *e);
void btn_send_clicked(Element *e);

#endif
