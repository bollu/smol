#include <SDL.h>
#define main main
#include <stdio.h>
#include "renderer.h"
#include "microui/microui-header.h"
#include "string.h"
#include "sdl/include/SDL_keycode.h"
#include <string>
#include <vector>
#include <cassert>

static  char logbuf[64000];
static   int logbuf_updated = 0;
static float bg[3] = { 90, 95, 100 };


static void write_log(const char* text) {
    if (logbuf[0]) { strcat(logbuf, "\n"); }
    strcat(logbuf, text);
    logbuf_updated = 1;
}


static void test_window(mu_Context* ctx) {
    /* do window */
    if (mu_begin_window(ctx, "Demo Window", mu_rect(40, 40, 300, 450))) {
        mu_Container* win = mu_get_current_container(ctx);
        win->rect.w = mu_max(win->rect.w, 240);
        win->rect.h = mu_max(win->rect.h, 300);

        /* window info */
        if (mu_header(ctx, "Window Info")) {
            mu_Container* win = mu_get_current_container(ctx);
            char buf[64];
            int widths[] = { 54, -1 };
            mu_layout_row(ctx, 2, widths , 0);
            mu_label(ctx, "Position:");
            sprintf(buf, "%d, %d", win->rect.x, win->rect.y); mu_label(ctx, buf);
            mu_label(ctx, "Size:");
            sprintf(buf, "%d, %d", win->rect.w, win->rect.h); mu_label(ctx, buf);
        }

        /* labels + buttons */
        if (mu_header_ex(ctx, "Test Buttons", MU_OPT_EXPANDED)) {
            int widths[] = { 86, -110, -1 };
            mu_layout_row(ctx, 3, widths, 0);
            mu_label(ctx, "Test buttons 1:");
            if (mu_button(ctx, "Button 1")) { write_log("Pressed button 1"); }
            if (mu_button(ctx, "Button 2")) { write_log("Pressed button 2"); }
            mu_label(ctx, "Test buttons 2:");
            if (mu_button(ctx, "Button 3")) { write_log("Pressed button 3"); }
            if (mu_button(ctx, "Popup")) { mu_open_popup(ctx, "Test Popup"); }
            if (mu_begin_popup(ctx, "Test Popup")) {
                mu_button(ctx, "Hello");
                mu_button(ctx, "World");
                mu_end_popup(ctx);
            }
        }

        /* tree */
        if (mu_header_ex(ctx, "Tree and Text", MU_OPT_EXPANDED)) {
            int widths[] = { 140, -1 };
            mu_layout_row(ctx, 2, widths, 0);
            mu_layout_begin_column(ctx);
            if (mu_begin_treenode(ctx, "Test 1")) {
                if (mu_begin_treenode(ctx, "Test 1a")) {
                    mu_label(ctx, "Hello");
                    mu_label(ctx, "world");
                    mu_end_treenode(ctx);
                }
                if (mu_begin_treenode(ctx, "Test 1b")) {
                    if (mu_button(ctx, "Button 1")) { write_log("Pressed button 1"); }
                    if (mu_button(ctx, "Button 2")) { write_log("Pressed button 2"); }
                    mu_end_treenode(ctx);
                }
                mu_end_treenode(ctx);
            }
            if (mu_begin_treenode(ctx, "Test 2")) {
                int widths[] = { 54, 54 };
                mu_layout_row(ctx, 2, widths, 0);
                if (mu_button(ctx, "Button 3")) { write_log("Pressed button 3"); }
                if (mu_button(ctx, "Button 4")) { write_log("Pressed button 4"); }
                if (mu_button(ctx, "Button 5")) { write_log("Pressed button 5"); }
                if (mu_button(ctx, "Button 6")) { write_log("Pressed button 6"); }
                mu_end_treenode(ctx);
            }
            if (mu_begin_treenode(ctx, "Test 3")) {
                static int checks[3] = { 1, 0, 1 };
                mu_checkbox(ctx, "Checkbox 1", &checks[0]);
                mu_checkbox(ctx, "Checkbox 2", &checks[1]);
                mu_checkbox(ctx, "Checkbox 3", &checks[2]);
                mu_end_treenode(ctx);
            }
            mu_layout_end_column(ctx);

            mu_layout_begin_column(ctx);
            int widths_row[] = { -1 };
            mu_layout_row(ctx, 1, widths_row, 0);
            mu_text(ctx, "Lorem ipsum dolor sit amet, consectetur adipiscing "
                "elit. Maecenas lacinia, sem eu lacinia molestie, mi risus faucibus "
                "ipsum, eu varius magna felis a nulla.");
            mu_layout_end_column(ctx);
        }

        /* background color sliders */
        if (mu_header_ex(ctx, "Background Color", MU_OPT_EXPANDED)) {
            int widths_row[] = {-78, -1};
            mu_layout_row(ctx, 2, widths_row, 74);
            /* sliders */
            mu_layout_begin_column(ctx);
            int widths_layout_row[] = {46, -1};
            mu_layout_row(ctx, 2, widths_layout_row, 0);
            mu_label(ctx, "Red:");   mu_slider(ctx, &bg[0], 0, 255);
            mu_label(ctx, "Green:"); mu_slider(ctx, &bg[1], 0, 255);
            mu_label(ctx, "Blue:");  mu_slider(ctx, &bg[2], 0, 255);
            mu_layout_end_column(ctx);
            /* color preview */
            mu_Rect r = mu_layout_next(ctx);
            mu_draw_rect(ctx, r, mu_color(bg[0], bg[1], bg[2], 255));
            char buf[32];
            sprintf(buf, "#%02X%02X%02X", (int)bg[0], (int)bg[1], (int)bg[2]);
            mu_draw_control_text(ctx, buf, r, MU_COLOR_TEXT, MU_OPT_ALIGNCENTER);
        }

        mu_end_window(ctx);
    }
}


static void log_window(mu_Context* ctx) {
    if (mu_begin_window(ctx, "Log Window", mu_rect(350, 40, 300, 200))) {
        /* output text panel */
        int width_row[] = { -1 };
        mu_layout_row(ctx, 1, width_row, -25);
        mu_begin_panel(ctx, "Log Output");
        mu_Container* panel = mu_get_current_container(ctx);
        mu_layout_row(ctx, 1, width_row, -1);
        mu_text(ctx, logbuf);
        mu_end_panel(ctx);
        if (logbuf_updated) {
            panel->scroll.y = panel->content_size.y;
            logbuf_updated = 0;
        }

        /* input textbox + submit button */
        static char buf[128];
        int submitted = 0;
        int widths[] = {-70, -1};
        mu_layout_row(ctx, 2, widths, 0);
        if (mu_textbox(ctx, buf, sizeof(buf)) & MU_RES_SUBMIT) {
            assert(false && "text submitted");
            mu_set_focus(ctx, ctx->last_id);
            submitted = 1;
        }
        if (mu_button(ctx, "Submit")) { submitted = 1; }
        if (submitted) {
            write_log(buf);
            buf[0] = '\0';
        }

        mu_end_window(ctx);
    }
}


struct Cursor{
    int line; int col;
};

struct EditorState {
    std::vector<std::string> contents;
    Cursor loc;
    
} g_editor_state;


void editor_state_move_left(EditorState& s) {
    s.loc.col = std::max<int>(s.loc.col - 1, 0);
}


void editor_state_move_right(EditorState& s) {
    assert(s.loc.line <= s.contents.size());
    if (s.loc.line == s.contents.size()) { return; }
    const std::string& curline = s.contents[s.loc.line];
    s.loc.col = std::min<int>(s.loc.col + 1, curline.size());
}

void editor_state_move_up(EditorState& s) {
    s.loc.line = std::max<int>(s.loc.line - 1, 0);
}


void editor_state_move_down(EditorState& s) {
    s.loc.line = std::min<int>(s.loc.line + 1, s.contents.size());
}


void editor_state_enter_char(EditorState& s) {
    assert(s.loc.line <= s.contents.size());
    if (s.loc.line == s.contents.size()) {
        s.contents.push_back("");
        s.loc.line++;
    }

}

void editor_state_backspace_char(EditorState& s) {
    assert(s.loc.line <= s.contents.size());
    if (s.loc.line == s.contents.size()) { return; }
    std::string& curline = s.contents[s.loc.line];
    assert(s.loc.col <= curline.size());
    if (s.loc.col == 0) { return; }
    // think about what happens with [s.loc.col=1]. Rest will work.
    std::string tafter(curline.begin() + s.loc.col, curline.end());
    curline.resize(s.loc.col - 1); // need to remove col[0], so resize to length 0.
    curline += tafter;
    s.loc.col--;
}

void editor_state_insert_char(EditorState &s, char c) {
    assert(s.loc.line <= s.contents.size());

    if (s.loc.line == s.contents.size()) {
        s.contents.push_back(std::string());
    }
    std::string& curline = s.contents[s.loc.line];
    assert(s.loc.col <= curline.size());
    if (s.loc.col == curline.size()) {
        curline += c;
        s.loc.col += 1;
    } else {
        // think at [col=0]. will work everywhere else.
        std::string t(curline.begin() + s.loc.col, curline.end());
        curline.resize(s.loc.col);
        curline += c;
        curline += t;
        s.loc.col += 1;
    }
}

void mu_draw_cursor(mu_Context* ctx, mu_Rect* r) {
	const int CURSOR_WIDTH = 3;
	mu_Rect cursor = *r;
	cursor.w = CURSOR_WIDTH;
	mu_draw_rect(ctx, cursor, ctx->style->colors[MU_COLOR_TEXT]);
	r->w += CURSOR_WIDTH;
}
void mu_editor(mu_Context* ctx, EditorState *ed) {
    int width = -1;
    mu_Font font = ctx->style->font;
    mu_Color color = ctx->style->colors[MU_COLOR_TEXT];

    mu_Id id = mu_get_id(ctx, &ed, sizeof(ed));
    // hash the *pointer*.
    mu_Container* cnt = mu_get_current_container(ctx);
    assert(cnt && "must be within container");

    
	mu_update_control(ctx, id, cnt->body, MU_OPT_HOLDFOCUS);
	if (ctx->focus == id) {
        if (ctx->key_pressed & MU_KEY_RETURN) {
            editor_state_enter_char(*ed);
        }

        if (ctx->key_pressed & MU_KEY_LEFTARROW) {
            editor_state_move_left(*ed);
        }


        if (ctx->key_pressed & MU_KEY_RIGHTARROW) {
            editor_state_move_right(*ed);
        }

        if (ctx->key_pressed & MU_KEY_BACKSPACE) {
            editor_state_backspace_char(*ed); 
        }
		/* handle key press. stolen from mu_textbox_raw */
		for (int i = 0; i < strlen(ctx->input_text); ++i) {
			editor_state_insert_char(*ed, ctx->input_text[i]);
		}
	}

    mu_layout_begin_column(ctx);
    mu_layout_row(ctx, 1, &width, ctx->text_height(font));
	// mu_draw_control_frame(ctx, id, cnt->body, MU_COLOR_BASE, 0);
    const int MAX_LINES = 20;
    for (int l = 0; l < MAX_LINES; ++l) {
        mu_Rect r = mu_layout_next(ctx);

        char lineno_str[12];
        itoa(l, lineno_str, 10);
        const int total_len = 5;
        const int num_len = strlen(lineno_str);
        for (int i = num_len; i < total_len; ++i) {
            lineno_str[i] = ' ';
        }
        lineno_str[total_len] = 0;
        lineno_str[total_len + 1] = 0;
        mu_draw_text(ctx, font, lineno_str, strlen(lineno_str), mu_vec2(r.x, r.y), color);
        // line number width
        r.x += ctx->text_width(font, lineno_str, strlen(lineno_str));
        if (l >= ed->contents.size()) { continue; }
        const char* word = ed->contents[l].c_str();
        r.h = ctx->text_height(font);
        for (int i = 0; i < ed->contents[l].size(); ++i) {
			if (ctx->focus == id && 
                ed->loc.line == l && 
                ed->loc.col == i) {
				mu_draw_cursor(ctx, &r);
			}

            mu_draw_text(ctx, font, ed->contents[l].c_str()+ i, 1, mu_vec2(r.x, r.y), color);
            r.x += ctx->text_width(font, ed->contents[l].c_str() + i, 1);

		}

        // cursor
        if (ctx->focus == id && ed->loc.line == l && ed->loc.col == ed->contents[l].size()) {
            mu_draw_cursor(ctx, &r);
        }
    }

    mu_layout_end_column(ctx);
}


static void editor_window(mu_Context* ctx) {
    if (mu_begin_window(ctx, "Editor", mu_rect(0, 0, (1400 * 2) / 3, 768))) {
        /* output text panel */
        int width_row[] = { -1 };
        mu_layout_row(ctx, 1, width_row, -25);
        mu_begin_panel(ctx, "Log Output");
        mu_Container* panel = mu_get_current_container(ctx);
        mu_layout_row(ctx, 1, width_row, -1);
        // TODO: look at mu_textbox to see how to handle input.
        mu_editor(ctx, &g_editor_state);
        mu_end_panel(ctx);
        //if (logbuf_updated) {
        //    panel->scroll.y = panel->content_size.y;
        //    logbuf_updated = 0;
        //}

        ///* input textbox + submit button */
        //static char buf[128];
        //int submitted = 0;
        //int widths[] = { -70, -1 };
        //mu_layout_row(ctx, 2, widths, 0);
        //if (mu_textbox(ctx, buf, sizeof(buf)) & MU_RES_SUBMIT) {
        //    mu_set_focus(ctx, ctx->last_id);
        //    submitted = 1;
        //}
        //if (mu_button(ctx, "Submit")) { submitted = 1; }
        //if (submitted) {
        //    write_log(buf);
        //    buf[0] = '\0';
        //}

        mu_end_window(ctx);
    }
}


static int uint8_slider(mu_Context* ctx, unsigned char* value, int low, int high) {
    static float tmp;
    mu_push_id(ctx, &value, sizeof(value));
    tmp = *value;
    int res = mu_slider_ex(ctx, &tmp, low, high, 0, "%.0f", MU_OPT_ALIGNCENTER);
    *value = tmp;
    mu_pop_id(ctx);
    return res;
}


static void style_window(mu_Context* ctx) {
    static struct { const char* label; int idx; } colors[] = {
      { "text:",         MU_COLOR_TEXT        },
      { "border:",       MU_COLOR_BORDER      },
      { "windowbg:",     MU_COLOR_WINDOWBG    },
      { "titlebg:",      MU_COLOR_TITLEBG     },
      { "titletext:",    MU_COLOR_TITLETEXT   },
      { "panelbg:",      MU_COLOR_PANELBG     },
      { "button:",       MU_COLOR_BUTTON      },
      { "buttonhover:",  MU_COLOR_BUTTONHOVER },
      { "buttonfocus:",  MU_COLOR_BUTTONFOCUS },
      { "base:",         MU_COLOR_BASE        },
      { "basehover:",    MU_COLOR_BASEHOVER   },
      { "basefocus:",    MU_COLOR_BASEFOCUS   },
      { "scrollbase:",   MU_COLOR_SCROLLBASE  },
      { "scrollthumb:",  MU_COLOR_SCROLLTHUMB },
      { NULL }
    };

    if (mu_begin_window(ctx, "Style Editor", mu_rect(350, 250, 300, 240))) {
        int sw = mu_get_current_container(ctx)->body.w * 0.14;
        int widths[] = { 80, sw, sw, sw, sw, -1 };
        mu_layout_row(ctx, 6, widths, 0);
        for (int i = 0; colors[i].label; i++) {
            mu_label(ctx, colors[i].label);
            uint8_slider(ctx, &ctx->style->colors[i].r, 0, 255);
            uint8_slider(ctx, &ctx->style->colors[i].g, 0, 255);
            uint8_slider(ctx, &ctx->style->colors[i].b, 0, 255);
            uint8_slider(ctx, &ctx->style->colors[i].a, 0, 255);
            mu_draw_rect(ctx, mu_layout_next(ctx), ctx->style->colors[i]);
        }
        mu_end_window(ctx);
    }
}


static void process_frame(mu_Context* ctx) {
    // fprintf(stderr, "mu_begin?\n");
    mu_finalize_events_begin_draw(ctx);
    // style_window(ctx);
    log_window(ctx);
    // test_window(ctx);
    editor_window(ctx);
    mu_end(ctx);
}



int button_map(int sdl_key) {
    if (sdl_key == SDL_BUTTON_LEFT) {
        return MU_MOUSE_LEFT;
    }
    else if (sdl_key == SDL_BUTTON_RIGHT) {
        return MU_MOUSE_RIGHT;
    }
    else if (sdl_key == SDL_BUTTON_MIDDLE) {
        return MU_MOUSE_MIDDLE;
    }
    return 0;
}

int key_map(int sdl_key) {
    if (sdl_key == SDLK_LSHIFT) return MU_KEY_SHIFT;
    if (sdl_key == SDLK_RSHIFT) return MU_KEY_SHIFT;
    if (sdl_key == SDLK_LCTRL) return MU_KEY_CTRL;
    if (sdl_key == SDLK_RCTRL) return MU_KEY_CTRL;
    if (sdl_key == SDLK_LALT) return MU_KEY_ALT;
    if (sdl_key == SDLK_RALT) return MU_KEY_ALT;
    if (sdl_key == SDLK_RETURN) return MU_KEY_RETURN;
    if (sdl_key == SDLK_UP) return MU_KEY_UPARROW;
    if (sdl_key == SDLK_DOWN) return MU_KEY_DOWNARROW;
    if (sdl_key == SDLK_LEFT) return MU_KEY_LEFTARROW;
    if (sdl_key == SDLK_RIGHT) return MU_KEY_RIGHTARROW;

    if (sdl_key == SDLK_BACKSPACE) return MU_KEY_BACKSPACE;
    return 0;

}


static int text_width(mu_Font font, const char* text, int len) {
    if (len == -1) { len = strlen(text); }
    return r_get_text_width(text, len);
}

static int text_height(mu_Font font) {
    return r_get_text_height();
}


int main(int argc, char** argv) {
    /* init SDL and renderer */
    SDL_Init(SDL_INIT_EVERYTHING);
    r_init();

    /* init microui */
    mu_Context* ctx = new mu_Context;
    mu_init(ctx, text_width, text_height);
    ctx->text_width = text_width;
    ctx->text_height = text_height;

    /* main loop */
    for (;;) {
        /* handle SDL events */
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT: exit(0); break;
            case SDL_MOUSEMOTION: mu_input_mousemove(ctx, e.motion.x, e.motion.y); break;
            case SDL_MOUSEWHEEL: mu_input_scroll(ctx, 0, e.wheel.y * -30); break;
            case SDL_TEXTINPUT: mu_input_text(ctx, e.text.text); break;

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP: {
                int b = button_map(e.button.button);
                if (b && e.type == SDL_MOUSEBUTTONDOWN) { mu_input_mousedown(ctx, e.button.x, e.button.y, b); }
                if (b && e.type == SDL_MOUSEBUTTONUP) { mu_input_mouseup(ctx, e.button.x, e.button.y, b); }
                break;
            }

            case SDL_KEYDOWN:
            case SDL_KEYUP: {
                // vv bollu: I don't know why this was masked.
                // int c = key_map(e.key.keysym.sym & 0xff);
                int c = key_map(e.key.keysym.sym);
                if (c && e.type == SDL_KEYDOWN) { mu_input_keydown(ctx, c); }
                if (c && e.type == SDL_KEYUP) { mu_input_keyup(ctx, c); }
                break;
            }
            }
        }

        /* process frame */
        process_frame(ctx);

        /* render */
        r_clear(mu_color(bg[0], bg[1], bg[2], 255));
        mu_Command* cmd = NULL;
        while (mu_next_command(ctx, &cmd)) {
            switch (cmd->type) {
            case MU_COMMAND_TEXT: r_draw_text(cmd->text.str, cmd->text.pos, cmd->text.color); break;
            case MU_COMMAND_RECT: r_draw_rect(cmd->rect.rect, cmd->rect.color); break;
            case MU_COMMAND_ICON: r_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color); break;
            case MU_COMMAND_CLIP: r_set_clip_rect(cmd->clip.rect); break;
            }
        }
        r_present();
    }

    return 0;
}

