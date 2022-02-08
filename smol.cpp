#include <SDL.h>
#define main main
#include <unordered_map>
#include <map>
#include <iostream>
#include "renderer.h"
#include "microui/microui-header.h"
#include "string.h"
#include "sdl/include/SDL_keycode.h"
#include <string>
#include <vector>
#include <cassert>
#include <unordered_map>
#include <optional>
#include <filesystem>
#include <fstream>
#include <stack>
#include <format>
#include <time.h>


// https://15721.courses.cs.cmu.edu/spring2018/papers/09-oltpindexes2/leis-icde2013.pdf
// Ukkonen


struct File {
    std::string path;
    char* buf;
    int len;

    File(std::string path, int len) : path(path), len(len) {};

};

using hash = long long;


bool is_newline(char c) {
    return c == '\r' || c == '\n';
}

bool is_whitespace(char c) {
    return c == ' ' || c == '\n' || c == '\t' || c == '\r';

}

struct Loc {
    File* file = nullptr;
    int ix = -1; 
    int line = -1;
    int col = -1;

    Loc() {}
    // TODO: don't store the string.
    Loc(File *file, int ix, int line, int col) : file(file), ix(ix), line(line), col(col) {};

    bool valid() const {
        assert(file != nullptr);
        assert(ix >= 0);
        assert(line >= 1);
        assert(col >= 1);
        assert(ix <= file->len);
        return true;
    }

    bool eof() const {
        assert(valid());
        return ix == file->len;
    }

    char get() const {
        assert(!eof());
        return file->buf[this->ix];
    }

    Loc advance() const {
        assert(!eof());
        if (file->buf[this->ix] == '\r') {
            assert(ix + 1 < file->len);
            assert(file->buf[this->ix + 1] == '\n');
            return Loc(file, ix + 2, line + 1, 1);
        }
        else if (file->buf[this->ix] == '\n') {
            return Loc(file, ix + 1, line + 1, 1);
        }
        else {
            return Loc(file, ix + 1, line, col + 1);
        }
    }
};

struct trie_node {
    std::unordered_map<int, trie_node*> next;
    std::vector<Loc> data;
    trie_node* parent = nullptr;
    trie_node(trie_node* parent) : parent(parent) {};
} g_index(nullptr);

void index_add(trie_node* index, const char *key, int len, Loc data) {
    for(int i = 0; i < len; ++i) {
        assert(index);
        const char c = key[i];
        if (!index->next.count(c)) {
			index->next[c] = new trie_node(index);
        }
        index = index->next[c];
    }
    index->data.push_back(data);
};

trie_node* index_lookup(trie_node* index, const char* key, int len) {
    for(int i = 0; i < len; ++i) {
        assert(index);
        const char c = key[i];
        if (!index->next.count(c)) { return nullptr;  }
        else { index = index->next[c]; }
    }
    return index;
}


// returns number of leaves added to out.
int index_get_leaves(trie_node* index, std::vector<trie_node*>& out, int max_to_find) {
    assert(max_to_find >= 0);
    if (max_to_find == 0) { return 0; }
    assert(max_to_find > 0);

    if (index->data.size()) {
        out.push_back(index);
    }
    max_to_find -= 1;
    assert(max_to_find >= 0);
    if (max_to_find == 0) { return 1;  }

    int num_added = 0;
    for (auto it : index->next) {
        num_added += index_get_leaves(it.second, out, max_to_find - num_added);
        assert(num_added <= max_to_find);
        if (num_added == max_to_find) {
            break;
        }
    }

    return num_added;
}


const int TARGET_FRAMES_PER_SECOND = 30.0;
const clock_t TARGET_CLOCKS_PER_FRAME = CLOCKS_PER_SEC / TARGET_FRAMES_PER_SECOND;

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

struct BottomlineState {
    std::string info;
} g_bottom_line_state;

struct CommandPaletteState {
    bool open = true;
    std::string input;
    std::vector<trie_node*> matches;
    int sequence_number = 0;
} g_command_palette_state;

mu_Id editor_state_mu_id(mu_Context *ctx, EditorState* ed) {
 return mu_get_id(ctx, &ed, sizeof(ed));
}

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
    if (s.loc.line < s.contents.size()) {
		 // can this be modeled as a connection on a bundle?
        s.loc.col = std::min<int>(s.loc.col, s.contents[s.loc.line].size());
    } else {
        assert(s.loc.line == s.contents.size());
        s.loc.col = 0;
    }
}


void editor_state_move_down(EditorState& s) {
    s.loc.line = std::min<int>(s.loc.line + 1, s.contents.size());
    if (s.loc.line < s.contents.size()) {
		 // can this be modeled as a connection on a bundle?
        s.loc.col = std::min<int>(s.loc.col, s.contents[s.loc.line].size());
    } else {
        assert(s.loc.line == s.contents.size());
        s.loc.col = 0;
    }
}


void editor_state_enter_char(EditorState& s) {
    assert(s.loc.line <= s.contents.size());
    if (s.loc.line == s.contents.size()) {
        s.contents.push_back("");
        s.loc.line++;
        s.loc.col = 0;
        return;
    }
    assert(s.loc.line < s.contents.size());
    std::string& curline = s.contents[s.loc.line];
    // think about [col=0].
    std::string nextline(curline.begin() + s.loc.col, curline.end());
    curline.resize(s.loc.col); 
    // TODO: fix --- this is broken.
    s.contents.push_back(std::string());
    for (int i = s.contents.size() - 1; i > s.loc.line + 1; i--) {
        s.contents[i] = s.contents[i - 1];
    }
    s.contents[s.loc.line+1] = nextline;
    s.loc.line++;
    s.loc.col = 0;
    return;
}

void editor_state_backspace_char(EditorState& s) {
    assert(s.loc.line <= s.contents.size());
    if (s.loc.line == s.contents.size()) { 
        if (s.loc.line == 0) { return; }
        s.loc.line--;
        s.loc.col = s.contents[s.loc.line].size();
        return;
    }

    std::string& curline = s.contents[s.loc.line];
    assert(s.loc.col <= curline.size());
    if (s.loc.col == 0) {
        if (s.loc.line == 0) {
            return;
        }
        // delete the line. think about [line=1]
        std::string& prevline = s.contents[s.loc.line - 1];
        const int new_cursor_col = prevline.size();
        const std::string& curline = s.contents[s.loc.line];
        prevline += curline;

        for (int i = s.loc.line; i < s.contents.size() - 2; i++) {
            s.contents[i] = s.contents[i + 1];
        }
        s.loc.line--;
        s.loc.col = new_cursor_col;
        s.contents.pop_back();
    }
    else {
        // think about what happens with [s.loc.col=1]. Rest will work.
        std::string tafter(curline.begin() + s.loc.col, curline.end());
        curline.resize(s.loc.col - 1); // need to remove col[0], so resize to length 0.
        curline += tafter;
        s.loc.col--;
    }
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
	// const int CURSOR_WIDTH = 3;
	mu_Rect cursor = *r;
	// cursor.w = CURSOR_WIDTH;
    mu_Font font = ctx->style->font;
    const int width = r_get_text_width("|", 1);
    mu_draw_text(ctx, font, "|", 1, mu_vec2(r->x - width/2, r->y), ctx->style->colors[MU_COLOR_TEXT]);
    r->w += width;
	// mu_draw_rect(ctx, cursor, ctx->style->colors[MU_COLOR_TEXT]);
}



void mu_command_palette(mu_Context* ctx, CommandPaletteState* state) {
    if (!state->open) { return; }
    if (mu_begin_window(ctx, "CMD", mu_Rect(50, 50, 1100, 700))) {


		// assert(false && "open palette");
        mu_set_focus(ctx, ctx->last_id);

        if (ctx->key_pressed & MU_KEY_BACKSPACE) {
			state->sequence_number++;
            state->input.resize(std::max<int>(0, state->input.size() - 1));
        }

		// if (ctx->key_pressed & MU_KEY_COMMAND_PALETTE) {
        //     // TODO: need to debounce.
        //     g_command_palette_state.open = false;
        //     g_command_palette_state.input = "";
        // }
        
		if (ctx->key_pressed & MU_KEY_RETURN) {
            // TODO: need to debounce.
            g_command_palette_state.open = false;
            g_command_palette_state.input = "";
        }

        if (strlen(ctx->input_text)) {
			/* handle key press. stolen from mu_textbox_raw */
            state->sequence_number++;
			state->input += std::string(ctx->input_text);
        }

		mu_Font font = ctx->style->font;

		mu_layout_begin_column(ctx);
        const int width[] = { 800 };
		mu_layout_row(ctx, 1, width, ctx->text_height(font));
        mu_Rect r = mu_layout_next(ctx);
		// mu_draw_text(ctx, font, "Command Palette", strlen("Command Palette"), mu_vec2(r.x, r.y), ctx->style->colors[MU_COLOR_TEXT]);
		// mu_draw_text(ctx, font, state->input.c_str(), state->input.size(), mu_vec2(0, 20), ctx->style->colors[MU_COLOR_TEXT]);
        const mu_Color QUERY_COLOR = { .r = 187, .g = 222, .b = 251, .a = 255 };
        mu_draw_text(ctx, font, (state->input + "|").c_str(), state->input.size() + 1, mu_vec2(r.x, r.y), QUERY_COLOR);



        // TODO: make this C
        trie_node* t = index_lookup(&g_index, state->input.c_str(), state->input.size());

        if (t != nullptr) {
			std::vector<trie_node*> answers;
            static const int NUM_ANSWERS = 80;
			index_get_leaves(t, answers, NUM_ANSWERS);
            int tot = 0;
            for (int i = 0; i < answers.size() && tot < NUM_ANSWERS;++i) {
                // mu_layout_row(ctx, 1, width, ctx->text_height(font));
                for (int j = 0; j < answers[i]->data.size() && tot < NUM_ANSWERS; ++j, ++tot) {
                    const Loc l = answers[i]->data[j];
                    std::string out = l.file->path;
                    out += ":";

                    int ix_line_end = l.ix;
                    while (ix_line_end < l.file->len && !is_newline(l.file->buf[ix_line_end])) {
                        ix_line_end++;
                    }
                    int ix_line_begin = l.ix;

                    while (ix_line_begin > 0 && !is_newline(l.file->buf[ix_line_begin])) {
                        ix_line_begin--;
                    }

                    for (int k = ix_line_begin; k < ix_line_end; ++k) {
                        out += l.file->buf[k];
                    }

					mu_Rect r = mu_layout_next(ctx);
					const mu_Color PATH_COLOR = { .r = 100, .g = 100, .b = 100, .a = 255 };
                    mu_draw_text(ctx, font, l.file->path.c_str(), l.file->path.size(), mu_vec2(r.x, r.y), PATH_COLOR);
                    r.x += r_get_text_width(l.file->path.c_str(), l.file->path.size());

                    mu_draw_text(ctx, font, ":", 1, mu_vec2(r.x, r.y), PATH_COLOR);
                    r.x += r_get_text_width(":", 1);


					const mu_Color CODE_NO_HIGHLIGHT_COLOR = { .r = 180, .g = 180, .b = 180, .a = 255 };
                    // [ix_line_begin, ix)
                    mu_draw_text(ctx, font, l.file->buf + ix_line_begin, l.ix - ix_line_begin, mu_vec2(r.x, r.y), CODE_NO_HIGHLIGHT_COLOR);
                    r.x += r_get_text_width(l.file->buf + ix_line_begin, l.ix - ix_line_begin);

                    // [ix, ix_str_end)
                    const int ix_str_end = l.ix + state->input.size();
                    const mu_Color CODE_WITH_HIGHLIGHT_COLOR = QUERY_COLOR;
                    mu_draw_text(ctx, font, l.file->buf + l.ix, ix_str_end - l.ix, mu_vec2(r.x, r.y), CODE_WITH_HIGHLIGHT_COLOR);
                    r.x += r_get_text_width(l.file->buf + l.ix, ix_str_end - l.ix);


                    // [ix+search str, ix end)
                    mu_draw_text(ctx, font, l.file->buf + ix_str_end, ix_line_end - ix_str_end, mu_vec2(r.x, r.y), CODE_NO_HIGHLIGHT_COLOR);
                    r.x += r_get_text_width(l.file->buf + ix_str_end, ix_line_end - ix_str_end);

                }
            }
        }

        mu_layout_end_column(ctx);

        mu_end_window(ctx);

    }

}

void mu_bottom_line(mu_Context* ctx, BottomlineState* s) {
    mu_Font font = ctx->style->font;
    mu_Rect parent_body = mu_get_current_container(ctx)->body;
    int y = parent_body.y + parent_body.h - r_get_text_height();
    mu_draw_text(ctx, font, s->info.c_str(), s->info.size(), mu_vec2(0, y), ctx->style->colors[MU_COLOR_TEXT]);
}



void mu_editor(mu_Context* ctx, EditorState *ed) {
    int width = -1;
    mu_Font font = ctx->style->font;
    mu_Color color = ctx->style->colors[MU_COLOR_TEXT];

    mu_Id id = editor_state_mu_id(ctx, ed); 
    mu_Container* cnt = mu_get_current_container(ctx);
    assert(cnt && "must be within container");


	mu_update_control(ctx, id, cnt->body, MU_OPT_HOLDFOCUS);
    const bool focused = ctx->focus == id;
	if (focused) {

        if (ctx->key_pressed & MU_KEY_RETURN) {
            editor_state_enter_char(*ed);
        }

        if (ctx->key_pressed & MU_KEY_UPARROW) {
            editor_state_move_up(*ed);
        }

        if (ctx->key_pressed & MU_KEY_DOWNARROW) {
            editor_state_move_down(*ed);
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
        
		if (ctx->key_down & MU_KEY_CTRL && ctx->key_pressed & MU_KEY_COMMAND_PALETTE) {
            // mu_open_popup(ctx, "CMD");
            g_command_palette_state.open = true;
            g_command_palette_state.input = "";
        }
		/* handle key press. stolen from mu_textbox_raw */
		for (int i = 0; i < strlen(ctx->input_text); ++i) {
			editor_state_insert_char(*ed, ctx->input_text[i]);
		}
	}

    mu_layout_begin_column(ctx);
    mu_layout_row(ctx, 1, &width, ctx->text_height(font));
	// mu_draw_control_frame(ctx, id, cnt->body, MU_COLOR_BASE, 0);
    const int MAX_LINES = 100;
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
        if (l > ed->contents.size()) { continue; }
        if (l == ed->contents.size()) {
            if (ed->loc.line == l && focused) {
                mu_draw_cursor(ctx, &r);
            }
            continue;
        }
        assert(l < ed->contents.size());
        const char* word = ed->contents[l].c_str();
        r.h = ctx->text_height(font);
        for (int i = 0; i < ed->contents[l].size(); ++i) {
			if (focused && 
                ed->loc.line == l && 
                ed->loc.col == i) {
				mu_draw_cursor(ctx, &r);
			}

            mu_draw_text(ctx, font, ed->contents[l].c_str()+ i, 1, mu_vec2(r.x, r.y), color);
            r.x += ctx->text_width(font, ed->contents[l].c_str() + i, 1);

		}

        // cursor
        if (ed->loc.line == l && ed->loc.col == ed->contents[l].size()) {
            mu_draw_cursor(ctx, &r);
        }
    }

    mu_layout_end_column(ctx);
}


static void editor_window(mu_Context* ctx) {
    const int window_opts =  MU_OPT_NOTITLE | MU_OPT_NOCLOSE | MU_OPT_NORESIZE;
    // if (mu_begin_window_ex(ctx, "Editor", mu_rect(0, 0, 1400, 768), window_opts)) {
    if (mu_begin_window(ctx, "Editor", mu_rect(0, 0, 1400, 768))) { 
        /* output text panel */
        int width_row[] = { -1 };
        mu_layout_row(ctx, 1, width_row, -25);
        // mu_begin_panel(ctx, "Log Output");
        // mu_Container* panel = mu_get_current_container(ctx);
        mu_layout_row(ctx, 1, width_row, -1);
        // TODO: look at mu_textbox to see how to handle input.
        mu_editor(ctx, &g_editor_state);
        // find a better way to make this the focus?
        // TODO: find less jank approach to make this focused at start time.
        mu_set_focus(ctx, editor_state_mu_id(ctx, &g_editor_state));
        // mu_end_panel(ctx);

		mu_bottom_line(ctx, &g_bottom_line_state);
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
    // log_window(ctx);
    // test_window(ctx);
    editor_window(ctx);
	mu_command_palette(ctx, &g_command_palette_state);
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
    if (sdl_key == SDLK_LCTRL) { return MU_KEY_CTRL; }
    if (sdl_key == SDLK_RCTRL) return MU_KEY_CTRL;
    if (sdl_key == SDLK_LALT) return MU_KEY_ALT;
    if (sdl_key == SDLK_RALT) return MU_KEY_ALT;
    if (sdl_key == SDLK_RETURN) return MU_KEY_RETURN;
    if (sdl_key == SDLK_UP) return MU_KEY_UPARROW;
    if (sdl_key == SDLK_DOWN) return MU_KEY_DOWNARROW;
    if (sdl_key == SDLK_LEFT) return MU_KEY_LEFTARROW;
    if (sdl_key == SDLK_RIGHT) return MU_KEY_RIGHTARROW;
    if (sdl_key == SDLK_BACKSPACE) return MU_KEY_BACKSPACE;
    if (sdl_key == SDLK_p) return MU_KEY_COMMAND_PALETTE;
    return 0;

}


static int text_width(mu_Font font, const char* text, int len) {
    if (len == -1) { len = strlen(text); }
    return r_get_text_width(text, len);
}

static int text_height(mu_Font font) {
    return r_get_text_height();
}



enum class TaskStateKind {
    TSK_ERROR,
    TSK_CONTINUE, // This objct continues to live on the stack.
    TSK_DONE // This object can be freed.
};

// lol, do I use a stack because queueing theory tells me to?
struct Task {
    // enqueue more tasks as necessary.
    // when a task is run, the task has been *popped off* the stack.
    // so the runner is:
    // t = tasks.top(); t->pop(); t->run(tasks);
    // return fals
    virtual TaskStateKind run(std::stack<Task*>& tasks) = 0;
    virtual ~Task() {};
};



struct TaskIndexFile : public Task {
    std::filesystem::path p;
    trie_node* index;
    Loc loc;
	TaskIndexFile(trie_node* index, std::filesystem::path p) : index(index), p(p) {}

    TaskStateKind run(std::stack<Task*>& tasks) {
        if (!loc.file) {
            FILE *fp = _wfopen(p.c_str(), L"rb");
            assert(fp && "unable to open file");

            fseek(fp, 0, SEEK_END);
            const int total_size = ftell(fp);

            loc.file = new File(p.string(), total_size);
            loc.ix = 0;
            loc.line = 1;
            loc.col = 1;

            fseek(fp, 0, SEEK_SET);
            loc.file->buf = new char[total_size];
            const int nread = fread(loc.file->buf, 1, total_size, fp);
            fclose(fp);
            assert(nread == total_size && "unable to read file");

            g_bottom_line_state.info = "ix: " + p.string() + " | 0%";
            tasks.push(this);
			return TaskStateKind::TSK_CONTINUE;
        }

        assert(loc.valid());
        while (loc.ix < loc.file->len && is_whitespace(loc.get())) { 
            loc = loc.advance();
        }

        assert(loc.valid());
        if (loc.eof()) {
			g_bottom_line_state.info = "DONE indexing " + p.string();
            return TaskStateKind::TSK_DONE;
        }
        assert(!is_whitespace(loc.get()));

        Loc eol = loc;
        static const int MAX_SUFFIX_LEN = 100;
        while (!eol.eof() && !is_newline(eol.get()) && (eol.ix - loc.ix) <= MAX_SUFFIX_LEN) {
            eol = eol.advance();
        }

        for (Loc sufloc = loc; sufloc.ix < eol.ix; sufloc = sufloc.advance()) {
            index_add(&g_index,
                loc.file->buf + sufloc.ix,
                eol.ix - sufloc.ix, sufloc);
			assert(!sufloc.eof());
        }

        const float percent = 100.0 * ((float)loc.ix / loc.file->len);
        g_bottom_line_state.info = "ix: ";
        g_bottom_line_state.info += p.string();
        g_bottom_line_state.info += " | ";
        g_bottom_line_state.info += std::to_string(percent);
        g_bottom_line_state.info += " | ";
        for (int i = loc.ix; i < eol.ix; ++i) {
            g_bottom_line_state.info += loc.file->buf[i];
        }

        loc = eol;
		tasks.push(this);
		return TaskStateKind::TSK_CONTINUE;
    }
};

struct TaskWalkDirectory : public Task {
    trie_node* index;
    std::filesystem::recursive_directory_iterator it;

    TaskWalkDirectory(trie_node *index, std::filesystem::path root) : index(index), it(root) { }
	TaskStateKind run(std::stack<Task*>& tasks) {
        if (it == std::filesystem::end(it)) {
            return TaskStateKind::TSK_DONE;
        } 
        std::filesystem::path curp = *it;
        g_bottom_line_state.info = "walking: " + curp.string();
        if (std::filesystem::is_regular_file(curp)) {
            tasks.push(new TaskIndexFile(index, curp));
        }
        ++it;

        tasks.push(this);
        return TaskStateKind::TSK_CONTINUE;
    }
};

struct TaskCommandPaletteMatch : public Task {
	// used by the task to check if it is stale.
    int sequence_number; 
    CommandPaletteState* state;
    std::stack<trie_node*> dfs;
    TaskCommandPaletteMatch(int sequence_number, CommandPaletteState *state) : sequence_number(sequence_number), state(state) {
        dfs.push(&g_index);
    }
    TaskStateKind run(std::stack<Task*> &tasks) {
        if (sequence_number != state->sequence_number) {
            return TaskStateKind::TSK_DONE;
        }

    }
};

void process_tasks(std::stack<Task*>& tasks) {
    const clock_t clock_begin = clock();

    do {
        if (!tasks.size()) {
            return;
        }
        Task* t = tasks.top(); tasks.pop();
        TaskStateKind k = t->run(tasks);
        if (k == TaskStateKind::TSK_DONE) {
            delete t;
        } else if (k == TaskStateKind::TSK_ERROR) {
            assert(false && "error when task was run.");
        }
    } while (clock() - clock_begin < TARGET_CLOCKS_PER_FRAME * 0.5);
}

int main(int argc, char** argv) {
    setlocale(LC_ALL, "");
    std::stack<Task*> g_tasks;
    
    const std::filesystem::path root_path(argc == 1 ? "C:\\Users\\bollu\\phd\\lean4\\src\\" : argv[1]);
    std::cout << "root_path: " << root_path << "\n";

    g_index.parent = &g_index;
    g_tasks.push(new TaskWalkDirectory(&g_index, root_path));
    g_bottom_line_state.info = "FOO BAR";

    SDL_Init(SDL_INIT_EVERYTHING);
    r_init();

    /* init microui */
    mu_Context* ctx = new mu_Context;
    mu_init(ctx, text_width, text_height);
    ctx->text_width = text_width;
    ctx->text_height = text_height;

    // vv this does not work to set focus at beginning :( 
    // mu_set_focus(ctx, editor_state_mu_id(ctx, &g_editor_state));

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
            // case SDL_TEXTEDITING: mu_input_text(ctx, e.edit.text); break;

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
        
        // Handle tasks.
        process_tasks(g_tasks);

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

