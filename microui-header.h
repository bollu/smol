/*
** Copyright (c) 2020 rxi
**
** This library is free software; you can redistribute it and/or modify it
** under the terms of the MIT license. See `microui.c` for details.
*/

#ifndef MICROUI_H
#define MICROUI_H

#define MU_VERSION "2.01"

#define MU_COMMANDLIST_SIZE     (256 * 1024)
#define MU_ROOTLIST_SIZE        32
#define MU_CONTAINERSTACK_SIZE  32
#define MU_CLIPSTACK_SIZE       32
#define MU_IDSTACK_SIZE         32
#define MU_LAYOUTSTACK_SIZE     16
#define MU_CONTAINERPOOL_SIZE   48
#define MU_TREENODEPOOL_SIZE    48
#define MU_MAX_WIDTHS           16
#define MU_REAL                 float
#define MU_REAL_FMT             "%.3g"
#define MU_SLIDER_FMT           "%.2f"
#define MU_MAX_FMT              127

#define mu_stack(T, n)          struct { int idx; T items[n]; }
#define mu_min(a, b)            ((a) < (b) ? (a) : (b))
#define mu_max(a, b)            ((a) > (b) ? (a) : (b))
#define mu_clamp(x, a, b)       mu_min(b, mu_max(a, x))

#ifdef __cplusplus
extern "C" {
#endif

enum {
  MU_CLIP_PART = 1,
  MU_CLIP_ALL
};

enum {
  MU_COMMAND_JUMP = 1,
  MU_COMMAND_CLIP,
  MU_COMMAND_RECT,
  MU_COMMAND_TEXT,
  MU_COMMAND_ICON,
  MU_COMMAND_MAX
};

enum {
  MU_COLOR_TEXT,
  MU_COLOR_BORDER,
  MU_COLOR_WINDOWBG,
  MU_COLOR_TITLEBG,
  MU_COLOR_TITLETEXT,
  MU_COLOR_PANELBG,
  MU_COLOR_BUTTON,
  MU_COLOR_BUTTONHOVER,
  MU_COLOR_BUTTONFOCUS,
  MU_COLOR_BASE,
  MU_COLOR_BASEHOVER,
  MU_COLOR_BASEFOCUS,
  MU_COLOR_SCROLLBASE,
  MU_COLOR_SCROLLTHUMB,
  MU_COLOR_MAX
};

enum {
  MU_ICON_CLOSE = 1,
  MU_ICON_CHECK,
  MU_ICON_COLLAPSED,
  MU_ICON_EXPANDED,
  MU_ICON_MAX
};

enum {
  MU_RES_ACTIVE       = (1 << 0),
  MU_RES_SUBMIT       = (1 << 1),
  MU_RES_CHANGE       = (1 << 2)
};

enum {
  MU_OPT_ALIGNCENTER  = (1 << 0),
  MU_OPT_ALIGNRIGHT   = (1 << 1),
  MU_OPT_NOINTERACT   = (1 << 2),
  MU_OPT_NOFRAME      = (1 << 3),
  MU_OPT_NORESIZE     = (1 << 4),
  MU_OPT_NOSCROLL     = (1 << 5),
  MU_OPT_NOCLOSE      = (1 << 6),
  MU_OPT_NOTITLE      = (1 << 7),
  MU_OPT_HOLDFOCUS    = (1 << 8),
  MU_OPT_AUTOSIZE     = (1 << 9),
  MU_OPT_POPUP        = (1 << 10),
  MU_OPT_CLOSED       = (1 << 11),
  MU_OPT_EXPANDED     = (1 << 12)
};

enum {
  MU_MOUSE_LEFT       = (1 << 0),
  MU_MOUSE_RIGHT      = (1 << 1),
  MU_MOUSE_MIDDLE     = (1 << 2)
};



typedef struct mu_Context mu_Context;
typedef unsigned mu_Id;
typedef MU_REAL mu_Real;
typedef void* mu_Font;

typedef struct { int x, y; } mu_Vec2;
typedef struct { int x, y, w, h; } mu_Rect;
typedef struct { unsigned char r, g, b, a; } mu_Color;
typedef struct { mu_Id id; int last_update; } mu_PoolItem;

typedef struct { 
  int type; // type is one of `MU_COMMAND_*`;
  int size; // size is the size of memory occupied by the derived classes.
  } mu_BaseCommand;
typedef struct { mu_BaseCommand base; void *dst; } mu_JumpCommand; // what is jump?
typedef struct { mu_BaseCommand base; mu_Rect rect; } mu_ClipCommand; // set clipping rectangle.
typedef struct { mu_BaseCommand base; mu_Rect rect; mu_Color color; } mu_RectCommand; // draw a rectangle.
typedef struct { mu_BaseCommand base; mu_Font font; mu_Vec2 pos; mu_Color color; char str[1]; } mu_TextCommand; // draw text
typedef struct { mu_BaseCommand base; mu_Rect rect; int id; mu_Color color; } mu_IconCommand; // draw icon.

typedef union {
  int type;
  mu_BaseCommand base;
  mu_JumpCommand jump;
  mu_ClipCommand clip;
  mu_RectCommand rect;
  mu_TextCommand text;
  mu_IconCommand icon;
} mu_Command;

typedef struct {
  mu_Rect body; // ?
  mu_Rect next; // ?
  mu_Vec2 position; // the poisition of the item to be returned by a call to [mu_layout_next]
  mu_Vec2 size; // ?
  mu_Vec2 max; // ?
  int widths[MU_MAX_WIDTHS]; // widths of each item in the layout.
  int items; // number of items in the layout
  int item_index; // the index of the item to be returned by a call to [mu_layout_next].
  int next_row; // the `y` coordinate of the next row.
  int next_type; // is RELATIVE or ABSOLUTE or 0
  int indent; // ?
} mu_Layout;

typedef struct {
  mu_Command *head, *tail; // command vector.
  mu_Rect rect; // what is rect v/s body?
  mu_Rect body;
  mu_Vec2 content_size; // what is content_size?
} mu_Container;

typedef struct {
  mu_Font font; // font stype
  mu_Vec2 size; //font size?
  int padding; // padding of buttons.
  int spacing; // text spacing?
  int indent; // ??
  int title_height; // height of title bar?
  int scrollbar_size; // scrollbar width?
  int thumb_size; // ???
  mu_Color colors[MU_COLOR_MAX];
} mu_Style;

struct mu_Context {
  /* callbacks */
  int (*text_width)(mu_Font font, const char *str, int len);
  int (*text_height)(mu_Font font);
  mu_Style _style; //style value ?
  mu_Id last_id; // last created id/hash.
  mu_Rect last_rect; // last rect that was using during layout
  int frame; // index of which frame we are on.
  /* stacks */
  // TODO: convert to stack
  // TODO: remove all need for jumpCommand
  mu_stack(char, MU_COMMANDLIST_SIZE) command_list; // list of draw commands to be interpreted by the client.
  mu_stack(mu_Container*, MU_ROOTLIST_SIZE) root_list; // ?
  mu_stack(mu_Container*, MU_CONTAINERSTACK_SIZE) container_stack; // ?
  mu_stack(mu_Rect, MU_CLIPSTACK_SIZE) clip_stack; // stack of clip rects for nested containers.
  mu_stack(mu_Id, MU_IDSTACK_SIZE) id_stack; // ?
  mu_stack(mu_Layout, MU_LAYOUTSTACK_SIZE) layout_stack; // ?
  /* retained state pools */
  mu_PoolItem container_pool[MU_CONTAINERPOOL_SIZE]; // ?
  mu_Container containers[MU_CONTAINERPOOL_SIZE]; // ?
};


mu_Vec2 mu_vec2(int x, int y);
mu_Rect mu_rect(int x, int y, int w, int h);
mu_Color mu_color(int r, int g, int b, int a);

void mu_init(mu_Context *ctx,
             int (*text_width)(mu_Font font, const char *str, int len),
             int (*text_height)(mu_Font font));

void mu_finalize_events_begin_draw(mu_Context *ctx);
void mu_end(mu_Context *ctx);
void mu_set_focus(mu_Context *ctx, mu_Id id);
mu_Id mu_get_id(mu_Context *ctx, const void *data, int size);
void mu_push_id(mu_Context *ctx, const void *data, int size);
void mu_pop_id(mu_Context *ctx);
void mu_push_clip_rect(mu_Context *ctx, mu_Rect rect);
void mu_pop_clip_rect(mu_Context *ctx);
mu_Rect mu_get_clip_rect(mu_Context *ctx);
int mu_check_clip(mu_Context *ctx, mu_Rect r);
mu_Container* mu_get_current_container(mu_Context *ctx);
mu_Container* mu_get_container(mu_Context *ctx, const char *name);

// what are pools for?
int mu_pool_init(mu_Context *ctx, mu_PoolItem *items, int len, mu_Id id);
int mu_pool_get(mu_Context *ctx, mu_PoolItem *items, int len, mu_Id id);
void mu_pool_update(mu_Context *ctx, mu_PoolItem *items, int idx);

// emit commands from microui and handle
mu_Command* mu_push_command(mu_Context *ctx, int type, int size);
int mu_next_command(mu_Context *ctx, mu_Command **cmd);
void mu_draw_clip(mu_Context *ctx, mu_Rect rect);
void mu_draw_rect(mu_Context *ctx, mu_Rect rect, mu_Color color);
void mu_draw_border_box(mu_Context *ctx, mu_Rect rect, mu_Color color);
void mu_draw_text(mu_Context *ctx, mu_Font font, const char *str, int len, mu_Vec2 pos, mu_Color color);
void mu_draw_icon(mu_Context *ctx, int id, mu_Rect rect, mu_Color color);

void mu_layout_row(mu_Context *ctx, int items, const int *widths, int height);
void mu_layout_width(mu_Context *ctx, int width);
void mu_layout_height(mu_Context *ctx, int height);
void mu_layout_begin_column(mu_Context *ctx);
void mu_layout_end_column(mu_Context *ctx);
void mu_layout_set_next(mu_Context *ctx, mu_Rect r, int relative);
mu_Rect mu_layout_next(mu_Context *ctx);

void mu_draw_control_frame(mu_Context *ctx, mu_Id id, mu_Rect rect, int colorid, int opt);
void mu_draw_control_text(mu_Context *ctx, const char *str, mu_Rect rect, int colorid, int opt);
void mu_update_control(mu_Context *ctx, mu_Id id, mu_Rect rect, int opt);

#define mu_button(ctx, label)             mu_button_ex(ctx, label, 0, MU_OPT_ALIGNCENTER)
#define mu_header(ctx, label)             mu_header_ex(ctx, label, 0)
#define mu_begin_window(ctx, title, rect) mu_begin_window_ex(ctx, title, rect, 0)
#define mu_begin_panel(ctx, name)         mu_begin_panel_ex(ctx, name, 0)

/// crate UI elements.
void mu_label(mu_Context *ctx, const char *text);
int mu_button_ex(mu_Context *ctx, const char *label, int icon, int opt);
int mu_header_ex(mu_Context *ctx, const char *label, int opt);
int mu_begin_window_ex(mu_Context *ctx, const char *title, mu_Rect rect, int opt);
void mu_end_window(mu_Context *ctx);
void mu_begin_panel_ex(mu_Context *ctx, const char *name, int opt);
void mu_end_panel(mu_Context *ctx);

#ifdef __cplusplus
}
#endif

#endif
