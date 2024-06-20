#include "layout.h"
#include "gfx.h"
#include "util.h"

static Label lbl_addr =
{
	.Type = ELEMENT_TYPE_LABEL,
	.Flags = 0,
	.X = 3 * PADDING + SIDEBAR_W + NICK_W + 2,
	.Y = PADDING,
	.Text = "Address:"
};

Label lbl_setname =
{
	.Type = ELEMENT_TYPE_LABEL,
	.Flags = FLAG_INVISIBLE,
	.X = PADDING,
	.Y = PADDING,
	.Text = "Assign Name:"
};

static char fld_setname_buf[64];
Input fld_setname =
{
	.Type = ELEMENT_TYPE_INPUT,
	.Flags = FLAG_INVISIBLE,
	.X = PADDING,
	.Y = PADDING,
	.W = SIDEBAR_W,
	.Position = 0,
	.Selection = 0,
	.Length = 0,
	.Text = fld_setname_buf,
	.Enter = fld_setname_enter
};

static char fld_addr_buf[128];
Input fld_addr =
{
	.Type = ELEMENT_TYPE_INPUT,
	.Flags = 0,
	.X = 3 * PADDING + SIDEBAR_W + NICK_W,
	.Y = 2 * PADDING + FONT_HEIGHT,
	.W = 0,
	.Position = 0,
	.Selection = 0,
	.Length = 0,
	.Text = fld_addr_buf,
	.Enter = btn_connect_clicked
};

static Button btn_connect =
{
	.Type = ELEMENT_TYPE_BUTTON,
	.Flags = 0,
	.X = PADDING,
	.Y = 2 * PADDING + FONT_HEIGHT,
	.W = BTN_W,
	.H = INPUT_HEIGHT,
	.Text = "Connect",
	.Click = btn_connect_clicked
};

Button btn_disconnect =
{
	.Type = ELEMENT_TYPE_BUTTON,
	.Flags = FLAG_INVISIBLE,
	.X = PADDING,
	.Y = PADDING,
	.W = SIDEBAR_W,
	.H = INPUT_HEIGHT,
	.Text = "Disconnect",
	.Click = btn_disconnect_clicked
};

static Label lbl_member =
{
	.Type = ELEMENT_TYPE_LABEL,
	.Flags = 0,
	.X = PADDING,
	.Y = PADDING,
	.Text = "Select:"
};

char lbl_view_buf[64];
Label lbl_view =
{
	.Type = ELEMENT_TYPE_LABEL,
	.Flags = 0,
	.X = 2 * PADDING + SIDEBAR_W,
	.Y = 3 * PADDING + FONT_HEIGHT + INPUT_HEIGHT,
	.Text = lbl_view_buf
};

static Button btn_logger =
{
	.Type = ELEMENT_TYPE_BUTTON,
	.Flags = 0,
	.Tag = 0,
	.X = PADDING,
	.Y = PADDING + FONT_HEIGHT + PADDING,
	.W = SIDEBAR_W,
	.H = INPUT_HEIGHT,
	.Text = "Logger",
	.Click = btn_logger_clicked
};

static Button btn_routes =
{
	.Type = ELEMENT_TYPE_BUTTON,
	.Flags = 0,
	.Tag = 0,
	.X = PADDING,
	.Y = PADDING + FONT_HEIGHT + PADDING + INPUT_HEIGHT + PADDING,
	.W = SIDEBAR_W,
	.H = INPUT_HEIGHT,
	.Text = "Routing Table",
	.Click = btn_routes_clicked
};

Button lst_members[MAXCLIENTS];
static Button lst_member =
{
	.Type = ELEMENT_TYPE_BUTTON,
	.Flags = FLAG_INVISIBLE,
	.Tag = 0,
	.X = PADDING,
	.Y = 4 * PADDING + FONT_HEIGHT + 2 * INPUT_HEIGHT,
	.W = SIDEBAR_W,
	.H = INPUT_HEIGHT,
	.Text = NULL,
	.Click = btn_member_clicked
};

static Label lbl_nick =
{
	.Type = ELEMENT_TYPE_LABEL,
	.Flags = 0,
	.X = 2 * PADDING + SIDEBAR_W + 2,
	.Y = PADDING,
	.Text = "Name:"
};

static char fld_nick_buf[128];
Input fld_nick =
{
	.Type = ELEMENT_TYPE_INPUT,
	.Flags = 0,
	.X = 2 * PADDING + SIDEBAR_W,
	.Y = 2 * PADDING + FONT_HEIGHT,
	.W = NICK_W,
	.Position = 0,
	.Selection = 0,
	.Length = 0,
	.Text = fld_nick_buf
};

static Label lbl_msg =
{
	.Type = ELEMENT_TYPE_LABEL,
	.Flags = 0,
	.X = 2 * PADDING + SIDEBAR_W + 2,
	.Y = 0,
	.Text = "Message:"
};

static char fld_msg_buf[128];
Input fld_msg =
{
	.Type = ELEMENT_TYPE_INPUT,
	.Flags = 0,
	.X = SIDEBAR_W + 2 * PADDING,
	.Y = PADDING,
	.W = 0,
	.Position = 0,
	.Selection = 0,
	.Length = 0,
	.Text = fld_msg_buf,
	.Enter = btn_send_clicked
};

static Button btn_send =
{
	.Type = ELEMENT_TYPE_BUTTON,
	.Flags = 0,
	.X = PADDING,
	.Y = PADDING,
	.W = BTN_W,
	.H = INPUT_HEIGHT,
	.Text = "Send",
	.Click = btn_send_clicked
};

#define ELEM_CNT 15
static void *elems[ELEM_CNT + MAXCLIENTS] =
{
	&btn_logger,
	&btn_routes,

	&lbl_member,
	&lbl_view,

	&lbl_nick,
	&fld_nick,

	&lbl_addr,
	&fld_addr,

	&btn_connect,

	&lbl_setname,
	&fld_setname,
	&btn_disconnect,

	&lbl_msg,
	&fld_msg,

	&btn_send
};

static Window frame =
{
	elems, ARRLEN(elems), -1, 1
};

void layout_resize(i32 w, i32 h)
{
	btn_connect.X = w - btn_connect.W - PADDING;

	lbl_setname.Y = h - 2 * INPUT_HEIGHT - 3 * PADDING - FONT_HEIGHT;
	fld_setname.Y = h - 2 * INPUT_HEIGHT - 2 * PADDING;

	btn_disconnect.Y = h - INPUT_HEIGHT - PADDING;

	fld_addr.W = w - fld_nick.W - lst_member.W - btn_connect.W - 5 * PADDING;

	btn_send.X = w - btn_connect.W - PADDING;
	btn_send.Y = h - btn_connect.H - PADDING;

	lbl_msg.Y = h - INPUT_HEIGHT - 2 * PADDING - FONT_HEIGHT;
	fld_msg.W = w - lst_member.W - btn_send.W - 4 * PADDING;
	fld_msg.Y = h - INPUT_HEIGHT - PADDING;
}

void layout_init(i32 w, i32 h)
{
	for(size_t i = 0; i < MAXCLIENTS; ++i)
	{
		lst_member.Text = smalloc(16);
		lst_members[i] = lst_member;
		elems[i + ELEM_CNT] = &lst_members[i];
		lst_member.Y += INPUT_HEIGHT + PADDING;
	}

	layout_resize(w, h);
	window_open(&frame);
}

void layout_free(void)
{
	for(size_t i = 0; i < MAXCLIENTS; ++i)
	{
		sfree(lst_members[i].Text);
	}
}
