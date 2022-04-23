/*
** Copyright (c) 2020 rxi
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to
** deal in the Software without restriction, including without limitation the
** rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
** sell copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
** FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
** IN THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "microui-header.h"

// Poor man's `assert`
#define expect(x) do {                                               \
    if (!(x)) {                                                      \
      fprintf(stderr, "Fatal error: %s:%d: assertion '%s' failed\n", \
        __FILE__, __LINE__, #x);                                     \
      abort();                                                       \
    }                                                                \
  } while (0)



// # Usage
// * **[Overview](#overview)**
// * **[Getting Started](#getting-started)**
// * **[Layout System](#layout-system)**
// * **[Style Customisation](#style-customisation)**
// * **[Custom Controls](#custom-controls)**
//



// ## Initializing `mu_Context`
// Before use a `mu_Context` should be initialised:
// ```c
// mu_Context *ctx = malloc(sizeof(mu_Context));
// mu_init(ctx);
// ```
// Following which the context's `text_width` and `text_height` callback functions
// should be set:
// ```c
// ctx->text_width = text_width;
// ctx->text_height = text_height;
// ```

static void draw_frame(mu_Context *ctx, mu_Rect rect, int colorid);
static mu_Style default_style;

void mu_init(mu_Context *ctx,
             int (*text_width)(mu_Font font, const char *str, int len),
             int (*text_height)(mu_Font font)) {
  memset(ctx, 0, sizeof(*ctx));
  // ctx->draw_frame = draw_frame; // why is this flexibility necessary?
  
  ctx->_style = default_style;
  ctx->text_width = text_width;
  ctx->text_height = text_height;
}

// The default style is encoded in a struct which represents TODO
static mu_Style default_style = {
  /* font | size | padding | spacing | indent */
  NULL, { 68, 10 }, 5, 4, 24,
  /* title_height | scrollbar_size | thumb_size */
  24, 12, 8,
  {
    { 230, 230, 230, 255 }, /* MU_COLOR_TEXT */
    { 25,  25,  25,  255 }, /* MU_COLOR_BORDER */
    { 50,  50,  50,  255 }, /* MU_COLOR_WINDOWBG */
    { 25,  25,  25,  255 }, /* MU_COLOR_TITLEBG */
    { 240, 240, 240, 255 }, /* MU_COLOR_TITLETEXT */
    { 0,   0,   0,   0   }, /* MU_COLOR_PANELBG */
    { 75,  75,  75,  255 }, /* MU_COLOR_BUTTON */
    { 95,  95,  95,  255 }, /* MU_COLOR_BUTTONHOVER */
    { 115, 115, 115, 255 }, /* MU_COLOR_BUTTONFOCUS */
    { 30,  30,  30,  255 }, /* MU_COLOR_BASE */
    { 35,  35,  35,  255 }, /* MU_COLOR_BASEHOVER */
    { 40,  40,  40,  255 }, /* MU_COLOR_BASEFOCUS */
    { 43,  43,  43,  255 }, /* MU_COLOR_SCROLLBASE */
    { 30,  30,  30,  255 }  /* MU_COLOR_SCROLLTHUMB */
  }
};

// ## The Main Loop
// The overall structure when using the library is as follows:
// ```
// initialise `mu_Context`
// 
// main loop:
//   1. call `mu_input_...` functions
//   2. call `mu_begin()`
//   3. process ui
//   4. call `mu_end()`
//   5. iterate commands/callbacks/events with
//        `mu_command_next()`
// ```
// 
// 



// ### 1. Input handling with `mu_input_...`
// In your main loop you should first pass user input to microui using the
// `mu_input_...` functions. It is safe to call the input functions multiple times
// if the same input event occurs in a single frame.


/*============================================================================
** input handlers
**============================================================================*/




// #### Macros for push & pop
// These macros are used to push and pop into a C stack data structure.

#define push(stk, val) do {                                                 \
    expect((stk).idx < (int) (sizeof((stk).items) / sizeof(*(stk).items))); \
    (stk).items[(stk).idx] = (val);                                         \
    (stk).idx++; /* incremented after incase `val` uses this value */       \
  } while (0)

#define pop(stk) do {      \
    expect((stk).idx > 0); \
    (stk).idx--;           \
  } while (0)




// #### Metadata: ID management

/* 32bit fnv-1a hash */
#define HASH_INITIAL 2166136261

// Implementation of the [Fowler-Noll-Vo](https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function) hash
// function.

static void hash(mu_Id *hash, const void *data, int size) {
  const unsigned char *p = data;
  while (size--) {
    *hash = (*hash ^ *p++) * 16777619;
  }
}

mu_Id mu_get_id(mu_Context *ctx, const void *data, int size) {
  int idx = ctx->id_stack.idx; // size of stack.
  mu_Id res = (idx > 0) ? ctx->id_stack.items[idx - 1] : HASH_INITIAL;
  hash(&res, data, size);
  ctx->last_id = res;
  return res;
}

// create a new ID.
void mu_push_id(mu_Context *ctx, const void *data, int size) {
  push(ctx->id_stack, mu_get_id(ctx, data, size));
}

// pop the last ID that was pushed.
void mu_pop_id(mu_Context *ctx) {
  pop(ctx->id_stack);
}


// ## 2. Call `mu_finalize_events_begin_draw`

// After handling the input the `mu_finalize_events_begin_draw()` function must be called before
// processing your UI:
// ```c
// mu_finalize_events_begin_draw(ctx);
// ```

void mu_finalize_events_begin_draw(mu_Context *ctx) {
  // check that text_width and text_height are initialized.
  expect(ctx->text_width && ctx->text_height);
  ctx->command_list.idx = 0;
  ctx->root_list.idx = 0;
  ctx->frame++;
}



// Before any controls can be used we must begin a window using one of the
// `mu_begin_window...` or `mu_begin_popup...` functions. The `mu_begin_...` window
// functions return a truthy value if the window is open, if this is not the case
// we should not process the window any further. When we are finished processing
// the window's ui the `mu_end_...` window function should be called.
// 
// ```c
// if (mu_begin_window(ctx, "My Window", 
//       mu_rect(10, 10, 300, 400))) {
//   /* process ui here... */
//   mu_end_window(ctx);
// }
// ```
// 
// It is safe to nest `mu_begin_window()` calls, this can be useful for things like
// context menus; the windows will still render separate from one another like
// normal.

static mu_Container* get_container(mu_Context *ctx, mu_Id id, int opt);
static void mu_begin_window_ex_begin_root_container(mu_Context *ctx, mu_Container *cnt);
static void push_container_body(  mu_Context *ctx, mu_Container *cnt, mu_Rect body, int opt);
static mu_Layout* get_layout(mu_Context *ctx);



int mu_begin_window_ex(mu_Context *ctx, const char *title, mu_Rect rect, int opt) {
  mu_Rect body;
  // hash object based on title.
  mu_Id id = mu_get_id(ctx, title, strlen(title));
  // find the container for this, and raise it to the front.
  mu_Container *cnt = get_container(ctx, id, opt);
  // if we can't find a container, or it is closed, give up.
  if (!cnt) { return 0; }
  // push the container ID onto the stack. (TODO: why?)
  push(ctx->id_stack, id);
  // if container is new(?), set its rect. (TODO: WHY?)
  if (cnt->rect.w == 0) { cnt->rect = rect; }

  mu_begin_window_ex_begin_root_container(ctx, cnt);
  rect = body = cnt->rect;

  /* draw frame */
  // if opt does NOT have MU_OPT_NOFRAME set:
  if (~opt & MU_OPT_NOFRAME) {
    draw_frame(ctx, rect, MU_COLOR_WINDOWBG);
  }

  /* do title bar */
  if (~opt & MU_OPT_NOTITLE) {
    mu_Rect tr = rect;
    tr.h = ctx->_style.title_height;
    draw_frame(ctx, tr, MU_COLOR_TITLEBG);

    /* do title text */
    if (~opt & MU_OPT_NOTITLE) {
      mu_Id id = mu_get_id(ctx, "!title", 6);
      mu_update_control(ctx, id, tr, opt);
      mu_draw_control_text(ctx, title, tr, MU_COLOR_TITLETEXT, opt);
      body.y += tr.h;
      body.h -= tr.h;
    }

    /* do `close` button */
    if (~opt & MU_OPT_NOCLOSE) {
      mu_Id id = mu_get_id(ctx, "!close", 6);
      mu_Rect r = mu_rect(tr.x + tr.w - tr.h, tr.y, tr.h, tr.h);
      tr.w -= r.w;
      mu_draw_icon(ctx, MU_ICON_CLOSE, r, ctx->_style.colors[MU_COLOR_TITLETEXT]);
      mu_update_control(ctx, id, r, opt);
    }
  }

  push_container_body(ctx, cnt, body, opt);

  /* do `resize` handle */
  if (~opt & MU_OPT_NORESIZE) {
    int sz = ctx->_style.title_height;
    mu_Id id = mu_get_id(ctx, "!resize", 7);
    mu_Rect r = mu_rect(rect.x + rect.w - sz, rect.y + rect.h - sz, sz, sz);
    mu_update_control(ctx, id, r, opt);
  }

  /* resize to content size */
  if (opt & MU_OPT_AUTOSIZE) {
    mu_Rect r = get_layout(ctx)->body;
    cnt->rect.w = cnt->content_size.x + (cnt->rect.w - r.w);
    cnt->rect.h = cnt->content_size.y + (cnt->rect.h - r.h);
  }

  mu_push_clip_rect(ctx, cnt->body);
  return MU_RES_ACTIVE;
}


static void end_root_container(mu_Context *ctx);
void mu_pop_clip_rect(mu_Context *ctx);

void mu_end_window(mu_Context *ctx) {
  mu_pop_clip_rect(ctx);
  end_root_container(ctx);
}


// While inside a window block we can safely process controls. Controls that allow
// user interaction return a bitset of `MU_RES_...` values. Some controls — such
// as buttons — can only potentially return a single `MU_RES_...`, thus their
// return value can be treated as a boolean:
// ```c
// if (mu_button(ctx, "My Button")) {
//   printf("'My Button' was pressed\n");
// }
// ```
// 
// The library generates unique IDs for controls internally to keep track of which
// are focused, hovered, etc. These are typically generated from the name/label
// passed to the function, or, in the case of sliders and checkboxes the value
// pointer. An issue arises then if you have several buttons in a window or panel
// that use the same label. The `mu_push_id()` and `mu_pop_id()` functions are
// provided for such situations, allowing you to push additional data that will be
// mixed into the unique ID:
// ```c
// for (int i = 0; i < 10; i++) {
//   mu_push_id(ctx, &i, sizeof(i));
//   if (mu_button(ctx, "x")) {
//     printf("Pressed button %d\n", i);
//   }
//   mu_pop_id(ctx);
// }
// ```
// 
// When we're finished processing the UI for this frame the `mu_end()` function
// should be called:
// ```c
// mu_end(ctx);
// ```
// 
// When we're ready to draw the UI the `mu_next_command()` can be used to iterate
// the resultant commands. The function expects a `mu_Command` pointer initialised
// to `NULL`. It is safe to iterate through the commands list any number of times:
// ```c
// mu_Command *cmd = NULL;
// while (mu_next_command(ctx, &cmd)) {
//   if (cmd->type == MU_COMMAND_TEXT) {
//     render_text(cmd->text.font, cmd->text.text, cmd->text.pos.x, cmd->text.pos.y, cmd->text.color);
//   }
//   if (cmd->type == MU_COMMAND_RECT) {
//     render_rect(cmd->rect.rect, cmd->rect.color);
//   }
//   if (cmd->type == MU_COMMAND_ICON) {
//     render_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color);
//   }
//   if (cmd->type == MU_COMMAND_CLIP) {
//     set_clip_rect(cmd->clip.rect);
//   }
// }
// ```
// 
// 
// ## Layout System
// The layout system is primarily based around *rows* — Each row
// can contain a number of *items* or *columns* each column can itself
// contain a number of rows and so forth. A row is initialised using the
// `mu_layout_row()` function, the user should specify the number of items
// on the row, an array containing the width of each item, and the height
// of the row:
// ```c
// /* initialise a row of 3 items: the first item with a width
// ** of 90 and the remaining two with the width of 100 */
// mu_layout_row(ctx, 3, (int[]) { 90, 100, 100 }, 0);
// ```
// When a row is filled the next row is started, for example, in the above
// code 6 buttons immediately after would result in two rows. The function
// can be called again to begin a new row.
// 
// As well as absolute values, width and height can be specified as `0`
// which will result in the Context's `style.size` value being used, or a
// negative value which will size the item relative to the right/bottom edge,
// thus if we wanted a row with a small button at the left, a textbox filling
// most the row and a larger button at the right, we could do the following:
// ```c
// mu_layout_row(ctx, 3, (int[]) { 30, -90, -1 }, 0);
// mu_button(ctx, "X");
// mu_textbox(ctx, buf, sizeof(buf));
// mu_button(ctx, "Submit");
// ```
// 
// If the `items` parameter is `0`, the `widths` parameter is ignored
// and controls will continue to be added to the row at the width last
// specified by `mu_layout_width()` or `style.size.x` if this function has
// not been called:
// ```c
// mu_layout_row(ctx, 0, NULL, 0);
// mu_layout_width(ctx, -90);
// mu_textbox(ctx, buf, sizeof(buf));
// mu_layout_width(ctx, -1);
// mu_button(ctx, "Submit");
// ```
// 
// A column can be started at any point on a row using the
// `mu_layout_begin_column()` function. Once begun, rows will act inside
// the body of the column — all negative size values will be relative to
// the column's body as opposed to the body of the container. All new rows
// will be contained within this column until the `mu_layout_end_column()`
// function is called.
// 
// Internally controls use the `mu_layout_next()` function to retrieve the
// next screen-positioned-Rect and advance the layout system, you should use
// this function when making custom controls or if you want to advance the
// layout system without placing a control.
// 
// The `mu_layout_set_next()` function is provided to set the next layout
// Rect explicitly. This will be returned by `mu_layout_next()` when it is
// next called. By using the `relative` boolean you can choose to provide
// a screen-space Rect or a Rect which will have the container's position
// and scroll offset applied to it. You can peek the next Rect from the
// layout system by using the `mu_layout_next()` function to retrieve it,
// followed by `mu_layout_set_next()` to return it:
// ```c
// mu_Rect rect = mu_layout_next(ctx);
// mu_layout_set_next(ctx, rect, 0);
// ```



// If you want to position controls arbitrarily inside a container the
// `relative` argument of `mu_layout_set_next()` should be true:
// ```c
// /* place a (40, 40) sized button at (300, 300) inside the container: */
// mu_layout_set_next(ctx, mu_rect(300, 300, 40, 40), 1);
// mu_button(ctx, "X");
// ```
// A Rect set with `relative` true will also effect the `content_size`
// of the container, causing it to effect the scrollbars if it exceeds the
// width or height of the container's body.
// 
// 
// ## Style Customisation
// The library provides styling support via the `mu_Style` struct and, if you
// want greater control over the look, the `draw_frame()` callback function.
// 
// The `mu_Style` struct contains spacing and sizing information, as well
// as a `colors` array which maps `colorid` to `mu_Color`. The library uses
// the `style` pointer field of the context to resolve colors and spacing,
// it is safe to change this pointer or modify any fields of the resultant
// struct at any point. See [`microui.h`](../src/microui.h) for the struct's
// implementation.
// 
// In addition to the style struct the context stores a `draw_frame()`
// callback function which is used whenever the *frame* of a control needs
// to be drawn, by default this function draws a rectangle using the color
// of the `colorid` argument, with a one-pixel border around it using the
// `MU_COLOR_BORDER` color.
// 
// 
// ## Custom Controls
// The library exposes the functions used by built-in controls to allow the
// user to make custom controls. A control should take a `mu_Context*` value
// as its first argument and return a `MU_RES_...` value. Your control's
// implementation should use `mu_layout_next()` to get its destination
// Rect and advance the layout system. `mu_get_id()` should be used with
// some data unique to the control to generate an ID for that control and
// `mu_update_control()` should be used to update the context's `hover`
// and `focus` values based on the mouse input state.
// 
// The `MU_OPT_HOLDFOCUS` opt value can be passed to `mu_update_control()`
// if we want the control to retain focus when the mouse button is released
// — this behaviour is used by textboxes which we want to stay focused
// to allow for text input.
// 
// A control that acts as a button which displays an integer and, when
// clicked increments that integer, could be implemented as such:
// ```c
// int incrementer(mu_Context *ctx, int *value) {
//   mu_Id     id = mu_get_id(ctx, &value, sizeof(value));
//   mu_Rect rect = mu_layout_next(ctx);
//   mu_update_control(ctx, id, rect, 0);
// 
//   /* handle input */
//   int res = 0;
//   if (ctx->mouse_pressed == MU_MOUSE_LEFT && ctx->focus == id) {
//     (*value)++;
//     res |= MU_RES_CHANGE;
//   }
// 
//   /* draw */
//   char buf[32];
//   sprintf(buf, "%d", *value);
//   mu_draw_control_frame(ctx, id, rect, MU_COLOR_BUTTON, 0);
//   mu_draw_control_text(ctx, buf, rect, MU_COLOR_TEXT, MU_OPT_ALIGNCENTER);
// 
//   return res;
// }
// ```


#define unused(x) ((void) (x))


static mu_Rect unclipped_rect = { 0, 0, 0x1000000, 0x1000000 };


// # Vectors
// these are used to define graphics primitives.

mu_Vec2 mu_vec2(int x, int y) {
  mu_Vec2 res;
  res.x = x; res.y = y;
  return res;
}


mu_Rect mu_rect(int x, int y, int w, int h) {
  mu_Rect res;
  res.x = x; res.y = y; res.w = w; res.h = h;
  return res;
}


mu_Color mu_color(int r, int g, int b, int a) {
  mu_Color res;
  res.r = r; res.g = g; res.b = b; res.a = a;
  return res;
}


// # Expand
// expands a rectangle by `n` in all directions

static mu_Rect expand_rect(mu_Rect rect, int n) {
  return mu_rect(rect.x - n, rect.y - n, rect.w + n * 2, rect.h + n * 2);
}

// # Intersect
// Intersects two rectangles.

static mu_Rect intersect_rects(mu_Rect r1, mu_Rect r2) {
  int x1 = mu_max(r1.x, r2.x);
  int y1 = mu_max(r1.y, r2.y);
  int x2 = mu_min(r1.x + r1.w, r2.x + r2.w);
  int y2 = mu_min(r1.y + r1.h, r2.y + r2.h);
  if (x2 < x1) { x2 = x1; }
  if (y2 < y1) { y2 = y1; }
  return mu_rect(x1, y1, x2 - x1, y2 - y1);
}

// Overlap
// Check if vector lies in rectangle
static int rect_overlaps_vec2(mu_Rect r, mu_Vec2 p) {
  return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
}


// Draw Frame: draw a frame, with an optional border.
static void draw_frame(mu_Context *ctx, mu_Rect rect, int colorid) {
  mu_draw_rect(ctx, rect, ctx->_style.colors[colorid]);
  if (colorid == MU_COLOR_SCROLLBASE  ||
      colorid == MU_COLOR_SCROLLTHUMB ||
      colorid == MU_COLOR_TITLEBG) { return; }
  /* draw border */
  if (0 && ctx->_style.colors[MU_COLOR_BORDER].a) {
    mu_draw_border_box(ctx, expand_rect(rect, 1), ctx->_style.colors[MU_COLOR_BORDER]);
  }
}




// End
// =====

// Check that all stacks have been flushed, apply scrolling and events.
void mu_end(mu_Context *ctx) {
  int i, n;
  /* check stacks */
  expect(ctx->container_stack.idx == 0);
  expect(ctx->clip_stack.idx      == 0);
  expect(ctx->id_stack.idx        == 0);
  expect(ctx->layout_stack.idx    == 0);

  // unset focus if focus id was not touched this frame
  // if (!ctx->have_updated_focus) { ctx->focus = 0; }
  // ctx->have_updated_focus = 0;



  n = ctx->root_list.idx;

  // TODO: what is a jump command?
  // that is, create pointers that connect the set of commands.
  // a jump command is a simple wau to connect linked lists.
  // sset root container jump commands
  for (i = 0; i < n; i++) {
    mu_Container *cnt = ctx->root_list.items[i];
    /* if this is the first container then make the first command jump to it.
    ** otherwise set the previous container's tail to jump to this one */
    if (i == 0) {
      mu_Command *cmd = (mu_Command*) ctx->command_list.items;
      cmd->jump.dst = (char*) cnt->head + sizeof(mu_JumpCommand);
    } else {
      mu_Container *prev = ctx->root_list.items[i - 1];
      prev->tail->jump.dst = (char*) cnt->head + sizeof(mu_JumpCommand);
    }
    /* make the last container's tail jump to the end of command list */
    if (i == n - 1) {
      cnt->tail->jump.dst = ctx->command_list.items + ctx->command_list.idx;
    }
  }
}




void mu_push_clip_rect(mu_Context *ctx, mu_Rect rect) {
  mu_Rect last = mu_get_clip_rect(ctx);
  push(ctx->clip_stack, intersect_rects(rect, last));
}


void mu_pop_clip_rect(mu_Context *ctx) {
  pop(ctx->clip_stack);
}


mu_Rect mu_get_clip_rect(mu_Context *ctx) {
  expect(ctx->clip_stack.idx > 0);
  return ctx->clip_stack.items[ctx->clip_stack.idx - 1];
}


int mu_check_clip(mu_Context *ctx, mu_Rect r) {
  mu_Rect cr = mu_get_clip_rect(ctx);
  if (r.x > cr.x + cr.w || r.x + r.w < cr.x ||
      r.y > cr.y + cr.h || r.y + r.h < cr.y   ) { return MU_CLIP_ALL; }
  if (r.x >= cr.x && r.x + r.w <= cr.x + cr.w &&
      r.y >= cr.y && r.y + r.h <= cr.y + cr.h ) { return 0; }
  return MU_CLIP_PART;
}


static void push_layout(mu_Context *ctx, mu_Rect body, mu_Vec2 scroll) {
  mu_Layout layout;
  int width = 0;
  memset(&layout, 0, sizeof(layout));
  layout.body = mu_rect(body.x - scroll.x, body.y - scroll.y, body.w, body.h);
  layout.max = mu_vec2(-0x1000000, -0x1000000);
  push(ctx->layout_stack, layout);
  mu_layout_row(ctx, 1, &width, 0);
}


// return top of layout stack
static mu_Layout* get_layout(mu_Context *ctx) {
  return &ctx->layout_stack.items[ctx->layout_stack.idx - 1];
}


static void pop_container(mu_Context *ctx) {
  mu_Container *cnt = mu_get_current_container(ctx);
  mu_Layout *layout = get_layout(ctx);
  cnt->content_size.x = layout->max.x - layout->body.x;
  cnt->content_size.y = layout->max.y - layout->body.y;
  /* pop container, layout and id */
  pop(ctx->container_stack);
  pop(ctx->layout_stack);
  mu_pop_id(ctx);
}


mu_Container* mu_get_current_container(mu_Context *ctx) {
  expect(ctx->container_stack.idx > 0);
  return ctx->container_stack.items[ ctx->container_stack.idx - 1 ];
}


static mu_Container* get_container(mu_Context *ctx, mu_Id id, int opt) {
  mu_Container *cnt;
  /* try to get existing container from pool */
  int idx = mu_pool_get(ctx, ctx->container_pool, MU_CONTAINERPOOL_SIZE, id);
  if (idx >= 0) {
    // TODO: why is this || ?
    if (~opt & MU_OPT_CLOSED) {
      mu_pool_update(ctx, ctx->container_pool, idx);
    }
    return &ctx->containers[idx];
  }
  if (opt & MU_OPT_CLOSED) { return NULL; }
  /* container not found in pool: init new container */
  idx = mu_pool_init(ctx, ctx->container_pool, MU_CONTAINERPOOL_SIZE, id);
  cnt = &ctx->containers[idx];
  memset(cnt, 0, sizeof(*cnt));
  return cnt;
}


mu_Container* mu_get_container(mu_Context *ctx, const char *name) {
  mu_Id id = mu_get_id(ctx, name, strlen(name));
  return get_container(ctx, id, 0);
}



/*============================================================================
** pool
**============================================================================*/


int mu_pool_init(mu_Context *ctx, mu_PoolItem *items, int len, mu_Id id) {
  int n = -1, f = ctx->frame;
  for (int i = 0; i < len; i++) {
    // find *rightmost* old slot and reuse it.
    // TODO: why *rightmost*? 
    if (items[i].last_update < f) {
      f = items[i].last_update; n = i;
    }
  }
  expect(n > -1);
  items[n].id = id;
  mu_pool_update(ctx, items, n);
  return n;
}


int mu_pool_get(mu_Context *ctx, mu_PoolItem *items, int len, mu_Id id) {
  int i;
  unused(ctx);
  for (i = 0; i < len; i++) {
    if (items[i].id == id) { return i; }
  }
  return -1;
}


void mu_pool_update(mu_Context *ctx, mu_PoolItem *items, int idx) {
  items[idx].last_update = ctx->frame;
}




/*============================================================================
** commandlist
**============================================================================*/

mu_Command* mu_push_command(mu_Context *ctx, int type, int size) {
  mu_Command *cmd = (mu_Command*) (ctx->command_list.items + ctx->command_list.idx);
  expect(ctx->command_list.idx + size < MU_COMMANDLIST_SIZE);
  cmd->base.type = type;
  cmd->base.size = size;
  ctx->command_list.idx += size;
  return cmd;
}


// return 1 if there is a command to be processsed, and 0 if all commands are exhausted.
int mu_next_command(mu_Context *ctx, mu_Command **cmd) {
  if (*cmd) {
    // go to the next location in memory where we have a command.
    *cmd = (mu_Command*) (((char*) *cmd) + (*cmd)->base.size);
  } else {
    // start at first command.
    *cmd = (mu_Command*) ctx->command_list.items;
  }
  // follow jump commands till we reach a non-jump command, or reach end of list.
  while ((char*) *cmd != ctx->command_list.items + ctx->command_list.idx) {
    if ((*cmd)->type != MU_COMMAND_JUMP) { return 1; }
    *cmd = (*cmd)->jump.dst;
  }
  return 0;
}


static mu_Command* push_jump(mu_Context *ctx, mu_Command *dst) {
  mu_Command *cmd;
  cmd = mu_push_command(ctx, MU_COMMAND_JUMP, sizeof(mu_JumpCommand));
  cmd->jump.dst = dst;
  return cmd;
}


// queue a clipping command.
void mu_draw_clip(mu_Context *ctx, mu_Rect rect) {
  mu_Command *cmd;
  cmd = mu_push_command(ctx, MU_COMMAND_CLIP, sizeof(mu_ClipCommand));
  cmd->clip.rect = rect;
}

// queue a draw rectangle command.
void mu_draw_rect(mu_Context *ctx, mu_Rect rect, mu_Color color) {
  mu_Command *cmd;
  rect = intersect_rects(rect, mu_get_clip_rect(ctx));
  if (rect.w > 0 && rect.h > 0) {
    cmd = mu_push_command(ctx, MU_COMMAND_RECT, sizeof(mu_RectCommand));
    cmd->rect.rect = rect;
    cmd->rect.color = color;
  }
}


// Draw a box of 1px width around the rectangle.
void mu_draw_border_box(mu_Context *ctx, mu_Rect rect, mu_Color color) {
  mu_draw_rect(ctx, mu_rect(rect.x + 1, rect.y, rect.w - 2, 1), color);
  mu_draw_rect(ctx, mu_rect(rect.x + 1, rect.y + rect.h - 1, rect.w - 2, 1), color);
  mu_draw_rect(ctx, mu_rect(rect.x, rect.y, 1, rect.h), color);
  mu_draw_rect(ctx, mu_rect(rect.x + rect.w - 1, rect.y, 1, rect.h), color);
}


void mu_draw_text(mu_Context *ctx, mu_Font font, const char *str, int len,
  mu_Vec2 pos, mu_Color color)
{
  mu_Command *cmd;
  mu_Rect rect = mu_rect(
    pos.x, pos.y, ctx->text_width(font, str, len), ctx->text_height(font));
  int clipped = mu_check_clip(ctx, rect);
  if (clipped == MU_CLIP_ALL ) { return; }
  if (clipped == MU_CLIP_PART) { mu_draw_clip(ctx, mu_get_clip_rect(ctx)); }
  /* add command */
  if (len < 0) { len = strlen(str); }
  cmd = mu_push_command(ctx, MU_COMMAND_TEXT, sizeof(mu_TextCommand) + len);
  memcpy(cmd->text.str, str, len);
  cmd->text.str[len] = '\0';
  cmd->text.pos = pos;
  cmd->text.color = color;
  cmd->text.font = font;
  /* reset clipping if it was set */
  if (clipped) { mu_draw_clip(ctx, unclipped_rect); }
}


void mu_draw_icon(mu_Context *ctx, int id, mu_Rect rect, mu_Color color) {
  mu_Command *cmd;
  /* do clip command if the rect isn't fully contained within the cliprect */
  int clipped = mu_check_clip(ctx, rect);
  if (clipped == MU_CLIP_ALL ) { return; }
  if (clipped == MU_CLIP_PART) { mu_draw_clip(ctx, mu_get_clip_rect(ctx)); }
  /* do icon command */
  cmd = mu_push_command(ctx, MU_COMMAND_ICON, sizeof(mu_IconCommand));
  cmd->icon.id = id;
  cmd->icon.rect = rect;
  cmd->icon.color = color;
  /* reset clipping if it was set */
  if (clipped) { mu_draw_clip(ctx, unclipped_rect); }
}


/*============================================================================
** layout
**============================================================================*/

// Should we have begin/end for row as well as column?


enum { RELATIVE = 1, ABSOLUTE = 2 };


void mu_layout_begin_column(mu_Context *ctx) {
  push_layout(ctx, mu_layout_next(ctx), mu_vec2(0, 0));
}


void mu_layout_end_column(mu_Context *ctx) {
  mu_Layout *a, *b;
  // top of layout stack; the column we had begun
  b = get_layout(ctx);
  pop(ctx->layout_stack);
  /* inherit position/next_row/max from child layout if they are greater */
  a = get_layout(ctx);
  // TODO: think about these computations.
  // The idea is to reflow the parent of the column, based on the column data.
  a->position.x = mu_max(a->position.x, b->position.x + b->body.x - a->body.x);
  a->next_row = mu_max(a->next_row, b->next_row + b->body.y - a->body.y);
  a->max.x = mu_max(a->max.x, b->max.x);
  a->max.y = mu_max(a->max.y, b->max.y);
}


// create a new layout row.
void mu_layout_row(mu_Context *ctx, int items, const int *widths, int height) {
  mu_Layout *layout = get_layout(ctx);
  if (widths) {
    expect(items <= MU_MAX_WIDTHS);
    memcpy(layout->widths, widths, items * sizeof(widths[0]));
  }
  layout->items = items;
  layout->position = mu_vec2(layout->indent, layout->next_row);
  layout->size.y = height;
  layout->item_index = 0;
}


// set layout width.
void mu_layout_width(mu_Context *ctx, int width) {
  get_layout(ctx)->size.x = width;
}

// set layout height.
void mu_layout_height(mu_Context *ctx, int height) {
  get_layout(ctx)->size.y = height;
}


void mu_layout_set_next(mu_Context *ctx, mu_Rect r, int relative) {
  mu_Layout *layout = get_layout(ctx);
  layout->next = r;
  layout->next_type = relative ? RELATIVE : ABSOLUTE;
}


// key function that performs layouting!
mu_Rect mu_layout_next(mu_Context *ctx) {
  mu_Layout *layout = get_layout(ctx);
  mu_Style *style = &ctx->_style;
  mu_Rect res;

  // no idea what it means for next_type to be unset!
  if (layout->next_type) {
    /* handle rect set by `mu_layout_set_next` */
    int type = layout->next_type;
    layout->next_type = 0;
    res = layout->next;
    // TODO: we don't increment item_index?
    if (type == ABSOLUTE) { return (ctx->last_rect = res); }
  } else {

    /* handle next row */
    if (layout->item_index == layout->items) {
      mu_layout_row(ctx, layout->items, NULL, layout->size.y);
    }

    // position
    res.x = layout->position.x;
    res.y = layout->position.y;

    // size 
    res.w = layout->items > 0 ? layout->widths[layout->item_index] : layout->size.x;
    res.h = layout->size.y;
    // if zero, then auto-size??
    if (res.w == 0) { res.w = style->size.x + style->padding * 2; }
    if (res.h == 0) { res.h = style->size.y + style->padding * 2; }
    // negative width, height is interpreted as: leave this much room from the border.
    if (res.w <  0) { res.w += layout->body.w - res.x + 1; }
    if (res.h <  0) { res.h += layout->body.h - res.y + 1; }

    layout->item_index++;
  }

  /* update position */
  layout->position.x += res.w + style->spacing;
  layout->next_row = mu_max(layout->next_row, res.y + res.h + style->spacing);

  /* apply body offset */
  res.x += layout->body.x;
  res.y += layout->body.y;

  /* update max position */
  layout->max.x = mu_max(layout->max.x, res.x + res.w);
  layout->max.y = mu_max(layout->max.y, res.y + res.h);

  return (ctx->last_rect = res);
}


/*============================================================================
** controls
**============================================================================*/

// draw a frame which colors based on focus/hover. Hence, a "control frame",
// such as a button.
void mu_draw_control_frame(mu_Context *ctx, mu_Id id, mu_Rect rect,
  int colorid, int opt)
{
  if (opt & MU_OPT_NOFRAME) { return; }
  // colorid += (ctx->focus == id) ? 2 : (ctx->hover == id) ? 1 : 0;
  // colorid += (ctx->focus == id) ? 2 : 0;
  colorid += 0;
  draw_frame(ctx, rect, colorid);
}


// draw text clipped to the rectangle.
void mu_draw_control_text(mu_Context *ctx, const char *str, mu_Rect rect,
  int colorid, int opt)
{
  mu_Vec2 pos;
  mu_Font font = ctx->_style.font;
  int tw = ctx->text_width(font, str, -1);
  mu_push_clip_rect(ctx, rect);
  pos.y = rect.y + (rect.h - ctx->text_height(font)) / 2;
  if (opt & MU_OPT_ALIGNCENTER) {
    pos.x = rect.x + (rect.w - tw) / 2;
  } else if (opt & MU_OPT_ALIGNRIGHT) {
    pos.x = rect.x + rect.w - tw - ctx->_style.padding;
  } else {
    pos.x = rect.x + ctx->_style.padding;
  }
  mu_draw_text(ctx, font, str, -1, pos, ctx->_style.colors[colorid]);
  mu_pop_clip_rect(ctx);
}




// update the state of the context relative to the object `id`, which inhabits location
// `rect`. this updates:
// - ctx->have_updated_focus: whether focus was updated.
// - ctx->hover: whether this element is being hovered on.
// - mu_set_focus(): if this element should be focused, which sets:
// - ctx->focus: the ID of the item being focused.
// - ctx->have_updated_focus: whether focus has been updated.
void mu_update_control(mu_Context *ctx, mu_Id id, mu_Rect rect, int opt) {
  // if (ctx->focus == id) { ctx->have_updated_focus = 1; }
  if (opt & MU_OPT_NOINTERACT) { return; }
}


void mu_text(mu_Context *ctx, const char *text) {
  const char *start, *end, *p = text;
  int width = -1;
  mu_Font font = ctx->_style.font;
  mu_Color color = ctx->_style.colors[MU_COLOR_TEXT];
  mu_layout_begin_column(ctx);
  mu_layout_row(ctx, 1, &width, ctx->text_height(font));
  do {
    mu_Rect r = mu_layout_next(ctx);
    int w = 0;
    start = end = p;
    do {
      const char* word = p;
      while (*p && *p != ' ' && *p != '\n') { p++; }
      w += ctx->text_width(font, word, p - word);
      if (w > r.w && end != start) { break; }
      w += ctx->text_width(font, p, 1);
      end = p++;
    } while (*end && *end != '\n');
    mu_draw_text(ctx, font, start, end - start, mu_vec2(r.x, r.y), color);
    p = end + 1;
  } while (*end);
  mu_layout_end_column(ctx);
}


void mu_label(mu_Context *ctx, const char *text) {
  mu_draw_control_text(ctx, text, mu_layout_next(ctx), MU_COLOR_TEXT, 0);
}


int mu_button_ex(mu_Context *ctx, const char *label, int icon, int opt) {
  int res = 0;
  mu_Id id = label ? mu_get_id(ctx, label, strlen(label))
                   : mu_get_id(ctx, &icon, sizeof(icon));
  mu_Rect r = mu_layout_next(ctx);
  mu_update_control(ctx, id, r, opt);
  /* handle click */
  /* draw */
  mu_draw_control_frame(ctx, id, r, MU_COLOR_BUTTON, opt);
  if (label) { mu_draw_control_text(ctx, label, r, MU_COLOR_TEXT, opt); }
  if (icon) { mu_draw_icon(ctx, icon, r, ctx->_style.colors[MU_COLOR_TEXT]); }
  return res;
}




static int header(mu_Context *ctx, const char *label, int opt) {
  mu_Rect r;
  int active, expanded;
  mu_Id id = mu_get_id(ctx, label, strlen(label));
  int width = -1;
  mu_layout_row(ctx, 1, &width, 0);

  expanded = (opt & MU_OPT_EXPANDED) ? !active : active;
  r = mu_layout_next(ctx);
  mu_update_control(ctx, id, r, 0);

  /* draw */
    mu_draw_control_frame(ctx, id, r, MU_COLOR_BUTTON, 0);
  mu_draw_icon(
    ctx, expanded ? MU_ICON_EXPANDED : MU_ICON_COLLAPSED,
    mu_rect(r.x, r.y, r.h, r.h), ctx->_style.colors[MU_COLOR_TEXT]);
  r.x += r.h - ctx->_style.padding;
  r.w -= r.h - ctx->_style.padding;
  mu_draw_control_text(ctx, label, r, MU_COLOR_TEXT, 0);

  return expanded ? MU_RES_ACTIVE : 0;
}


int mu_header_ex(mu_Context *ctx, const char *label, int opt) {
  return header(ctx, label, opt);
}


static void scrollbars(mu_Context *ctx, mu_Container *cnt, mu_Rect *body) {
  int sz = ctx->_style.scrollbar_size;
  mu_Vec2 cs = cnt->content_size;
  cs.x += ctx->_style.padding * 2;
  cs.y += ctx->_style.padding * 2;
  mu_push_clip_rect(ctx, *body);
  /* resize body to make room for scrollbars */
  if (cs.y > cnt->body.h) { body->w -= sz; }
  if (cs.x > cnt->body.w) { body->h -= sz; }
  /* to create a horizontal or vertical scrollbar almost-identical code is
  ** used; only the references to `x|y` `w|h` need to be switched */
  // scrollbar(ctx, cnt, body, cs, x, y, w, h);
  // scrollbar(ctx, cnt, body, cs, y, x, h, w);
  mu_pop_clip_rect(ctx);
}


static void push_container_body(
  mu_Context *ctx, mu_Container *cnt, mu_Rect body, int opt
) {
  // if (~opt & MU_OPT_NOSCROLL) { scrollbars(ctx, cnt, &body); }
  // push_layout(ctx, expand_rect(body, -ctx->_style.padding), cnt->scroll);
  mu_Vec2 scroll; scroll.x = scroll.y = 0;
  // push_layout(ctx, expand_rect(body, -ctx->_style.padding), cnt->scroll);
  push_layout(ctx, expand_rect(body, -ctx->_style.padding), scroll);
  cnt->body = body;
}


// helper for mu_begin_window to create a root container.
static void mu_begin_window_ex_begin_root_container(mu_Context *ctx, mu_Container *cnt) {
  /* push container into stack of containers */
  push(ctx->container_stack, cnt);
  /* push container to roots list and push head command */
  // TODO: what is jump?
  push(ctx->root_list, cnt);
  cnt->head = push_jump(ctx, NULL);
  /* clipping is reset here in case a root-container is made within
  ** another root-containers's begin/end block; this prevents the inner
  ** root-container being clipped to the outer.
  ** TODO: why would someone create a root container inside another root container?
  */
  // this resets the clip stack. `mu_push_clip_rect` pushes a sequence of clips..
  push(ctx->clip_stack, unclipped_rect);
}

// TODO: figure out the exact invariants
static void end_root_container(mu_Context *ctx) {
  /* push tail 'goto' jump command and set head 'skip' command. the final steps
  ** on initing these are done in mu_end() */
  mu_Container *cnt = mu_get_current_container(ctx);
  cnt->tail = push_jump(ctx, NULL);
  cnt->head->jump.dst = ctx->command_list.items + ctx->command_list.idx;
  /* pop base clip rect and container */
  mu_pop_clip_rect(ctx);
  pop_container(ctx);
}





// What is a panel, versus a window, versus a container?
void mu_begin_panel_ex(mu_Context *ctx, const char *name, int opt) {
  mu_Container *cnt;
  mu_push_id(ctx, name, strlen(name));
  cnt = get_container(ctx, ctx->last_id, opt);
  cnt->rect = mu_layout_next(ctx);
  if (~opt & MU_OPT_NOFRAME) {
    draw_frame(ctx, cnt->rect, MU_COLOR_PANELBG);
  }
  push(ctx->container_stack, cnt);
  push_container_body(ctx, cnt, cnt->rect, opt);
  mu_push_clip_rect(ctx, cnt->body);
}


void mu_end_panel(mu_Context *ctx) {
  mu_pop_clip_rect(ctx);
  pop_container(ctx);
}
