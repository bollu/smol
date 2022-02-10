// TODO: the art of multiprocessor programming.
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

const int TARGET_FRAMES_PER_SECOND = 15.0;
const clock_t TARGET_CLOCKS_PER_FRAME = CLOCKS_PER_SEC / TARGET_FRAMES_PER_SECOND;

// https://15721.courses.cs.cmu.edu/spring2018/papers/09-oltpindexes2/leis-icde2013.pdf
// Ukkonen


struct File {
    std::string path;
    char* buf = nullptr;
    int len = 0;
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
    int ix = -1;  // Loc points at file->buf[ix]
    int line = -1;
    int col = -1;

    Loc() {}
    // TODO: don't store the string.
    Loc(File *file, int ix, int line, int col) : file(file), ix(ix), line(line), col(col) {};

    bool operator == (const Loc &other) const {
        assert(other.file == this->file);
        bool eq =  this->ix == other.ix;
        if (eq) {
            assert(this->line == other.line);
            assert(this->col == other.col);
        }
        return eq;

    }
    bool valid() const {
        assert(file != nullptr);
        assert(ix >= 0);
        assert(line >= 0);
        assert(col >= 0);
        assert(ix <= file->len);
        // vv invariant for `col`.
        // this seems to suggest that file->buf[-1] = '\n', lmao.
        assert(ix >= col);
        assert(ix - col == 0 || is_newline(file->buf[ix - col - 1]));
        assert(line >= 0);
        // expensive invariant: # of newlines (\n)s upto ix equal line.
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
        assert(valid());
        assert(!eof());

        if (file->buf[this->ix] == '\r') {
            assert(ix + 1 < file->len);
            assert(file->buf[this->ix + 1] == '\n');
            return Loc(file, ix + 2, line + 1, 0);
        }
        else if (file->buf[this->ix] == '\n') {
            return Loc(file, ix + 1, line + 1, 0);
        }
        else {
            return Loc(file, ix + 1, line, col + 1);
        }
    }

    // vv NOTE: retreat() checks its correctness in terms of advance()
    // because advance() is the much simpler primitive.
    Loc retreat() const {
        assert(valid());
		Loc l = *this;
        if (l.ix == 0) { return l;  }

        // we need to move up a line
        if (l.col == 0) {
            const char c = l.file->buf[l.ix - 1];
            const char b = (l.ix - 2 >= 0) ? l.file->buf[l.ix - 2] : 0;
            assert(c == '\n');
            l.ix -= (b == '\r') ? 2 : 1;
            l.line -= 1;

            // we now need to fnd our new column.
            // col tells us how far back you need to go, to find a newline.
            l.col = 0;
            while (l.ix - l.col > 0 && !is_newline(l.file->buf[l.ix - l.col - 1])) {
                l.col++;
            }

            assert(l.advance() == *this);
			assert(l.valid());
            return l;
        }
        else {
            l.ix -= 1; l.col -= 1;
            assert(l.advance() == *this);
			assert(l.valid());
            return l;
        }
    }


    Loc start_of_cur_line() const {
        assert(valid());
        Loc l = *this;
        l.ix -= col; l.col = 0;
        assert(l.valid());
        return l;
    }

    Loc start_of_next_line() const {
        assert(valid());
        Loc l = *this;
        while (!l.eof() && this->line == l.line) { l = l.advance();  }
        assert(l.valid());
        return l;
    }

    Loc end_of_cur_line() const {
        assert(valid());
        Loc l = *this;
        while (!l.eof() && !is_newline(l.get())) { l = l.advance();  }
        assert(l.valid());
        return l;
    }

    Loc up() const {
        assert(valid());
        Loc l = *this;
        while (l.ix > 0) {
            l = l.retreat();
            if (l.line < this->line) {
                break;
            }
        }
        if (l.ix == 0) { return l;  }
        // l is now in a line before where we started.
        assert(l.line == this->line - 1);

        // l is at the rightmost position of the previous line.
        // Thus, to align cursors, we must pull the column
        // back towards us.
        // It may be the case that the line before us simply
        // does not have enough characters, which is why we do 
        // not loop for [l.col == this.col]. example:
        // b|     [l]
        // aaaa|  [this]
        while (l.col > this->col) {
            Loc lnext = l.retreat();
            if (lnext.line != l.line) { break;  }
            else { l = lnext;  }
        }
        assert(l.line == this->line - 1);
        assert(l.col <= this->col);
        assert(l.valid());
        return l;
    }

    // TODO: ask @codelegend for refactoring.
    Loc down() const {
        assert(valid());
        Loc l = *this;
        while (!l.eof()) {
            l = l.advance();
            if (l.line > this->line) {
                break;
            }
        }
        if (l.eof()) { return l; }
        // l is now in a line before where we started.
        assert(l.line == this->line + 1);

        // note that l is at the beginning of the next
        // line. Thus, we must push l forward to align with us.
        // we must take care to not skip into the next line accidentally.
        // It may be that the next line does not have enough characters.
        // So we check that we don't run over to the next line.
        // aaaa|  [this]
        // b|     [l]
        while (l.col < this->col) {
            Loc lnext = l.advance();
            if (lnext.line != l.line) { break;  }
            else { l = lnext;  }
        }
        return l;
        assert(l.line == this->line + 1);
        assert(l.col <= this->col);
        assert(l.valid());
    }

};


struct TrieNode {
    std::unordered_map<int, TrieNode*> next;
    std::vector<Loc> data;
    TrieNode* parent = nullptr;
    TrieNode(TrieNode* parent) : parent(parent) {};
};

void index_add(TrieNode* index, const char *key, int len, Loc data) {
    for(int i = 0; i < len; ++i) {
        assert(index);
        const char c = key[i];
        if (!index->next.count(c)) {
			index->next[c] = new TrieNode(index);
        }
        index = index->next[c];
    }
    index->data.push_back(data);
};

TrieNode* index_lookup(TrieNode* index, const char* key, int len) {
    for(int i = 0; i < len; ++i) {
        assert(index);
        const char c = key[i];
        if (!index->next.count(c)) { return nullptr;  }
        else { index = index->next[c]; }
    }
    return index;
}


// returns number of leaves added to out.
int index_get_leaves(TrieNode* index, std::vector<TrieNode*>& out, int max_to_find) {
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
    if (mu_begin_window(ctx, "Log Window", mu_rect(0, 720/2, 0, 720/2))) {
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


// TODO: rename to viewer.
struct ViewerState {
    // at file focus
    Loc focus;
};

struct BottomlineState {
    std::string info;
};

enum class FocusState {
    FSK_Palette,
    FSK_Viewer,

};
struct CommandPaletteState {
    std::string input;
    std::vector<Loc> matches;
    int sequence_number = 0;
	// index of selected option.
   // invariant: selected_ix <= matches.len(). Is equal to denote deselected index.
	int selected_ix =0;
};

mu_Id editor_state_mu_id(mu_Context *ctx, ViewerState* view) {
 return mu_get_id(ctx, &view, sizeof(view));
}

/*
void editor_state_move_left(ViewerState& s) {
    s.cursor.col = std::max<int>(s.cursor.col - 1, 0);
}


void editor_state_move_right(ViewerState& s) {
    assert(s.cursor.valid());
    if (s.cursor.advance().eof()) { return;  }
    if (is_newline(s.cursor.advance().get())) { return; }
    s.cursor = s.cursor.advance();
}

void editor_state_move_up(ViewerState& s) {
    s.cursor = s.cursor.up();
    s.render_begin_line = std::min<int>(s.cursor.line, s.render_begin_line);
}


void editor_state_move_down(ViewerState& s) {
    s.cursor = s.cursor.down();
}
*/

void mu_draw_cursor(mu_Context* ctx, mu_Rect* r) {
	mu_Rect cursor = *r;
    mu_Font font = ctx->style->font;
    const int width = r_get_text_width("|", 1);
    mu_draw_text(ctx, font, "|", 1, mu_vec2(r->x - width/2, r->y), ctx->style->colors[MU_COLOR_TEXT]);
    r->w += width;
}



void mu_command_palette(mu_Context* ctx, ViewerState *view, CommandPaletteState* pal, FocusState *focus) {
	assert(pal->selected_ix <= (int)pal->matches.size());

    const bool focused = *focus == FocusState::FSK_Palette;
    if (mu_begin_window(ctx, "CMD", mu_Rect(0, 0, 1400, 720/2))) {
        if (focused) {
            if (ctx->key_pressed & MU_KEY_BACKSPACE) {
                pal->sequence_number++;
                pal->input.resize(std::max<int>(0, pal->input.size() - 1));
            }

            // TODO: Ask @codelegend for clean way to handle this.
            if (ctx->key_pressed & MU_KEY_DOWNARROW) {
                pal->selected_ix = std::min<int>(pal->selected_ix + 1, pal->matches.size() - 1);
            }

            if (ctx->key_pressed & MU_KEY_UPARROW) {
                pal->selected_ix = std::max<int>(0, pal->selected_ix - 1);
            }

            if (ctx->key_pressed & MU_KEY_RETURN) {
                *focus = FocusState::FSK_Viewer;
            }

            if (strlen(ctx->input_text)) {
                /* handle key press. stolen from mu_textbox_raw */
                pal->sequence_number++;
                pal->input += std::string(ctx->input_text);
                pal->matches = {};
                pal->selected_ix = 0;
            }

            if (pal->selected_ix < pal->matches.size()) {
                view->focus = pal->matches[pal->selected_ix];
            }

        }

        mu_Font font = ctx->style->font;

        mu_layout_begin_column(ctx);
        const int width[] = { 800 };
        mu_layout_row(ctx, 1, width, ctx->text_height(font));
        mu_Rect r = mu_layout_next(ctx);
        // mu_draw_text(ctx, font, "Command Palette", strlen("Command Palette"), mu_vec2(r.x, r.y), ctx->style->colors[MU_COLOR_TEXT]);
        // mu_draw_text(ctx, font, state->input.c_str(), state->input.size(), mu_vec2(0, 20), ctx->style->colors[MU_COLOR_TEXT]);
        const mu_Color BLUE_COLOR = { .r = 187, .g = 222, .b = 251, .a = 255 };
        mu_draw_text(ctx, font, pal->input .c_str(), pal->input.size(), mu_vec2(r.x, r.y), BLUE_COLOR);
        r.x += r_get_text_width(pal->input.c_str(), pal->input.size());
        if (focused) {
			mu_draw_text(ctx, font, "|", 1, mu_vec2(r.x, r.y), BLUE_COLOR);

        }



        static const int NUM_ANSWERS = 80;
        static const int TOP_OFFSET = 3;
		for (int i = std::max<int>(0, pal->selected_ix - TOP_OFFSET); i < pal->matches.size() && i < NUM_ANSWERS; ++i) {
			const Loc l = pal->matches[i];
			int ix_line_end = l.ix;
			while (ix_line_end < l.file->len && !is_newline(l.file->buf[ix_line_end])) {
				ix_line_end++;
			}
			int ix_line_begin = l.ix;

			while (ix_line_begin > 0 && !is_newline(l.file->buf[ix_line_begin])) {
				ix_line_begin--;
			}

			mu_Rect r = mu_layout_next(ctx);

            const bool SELECTED = focused && (i == pal->selected_ix);
			const mu_Color WHITE_COLOR = { .r = 255, .g = 255, .b = 255, .a = 255 };
			const mu_Color GRAY_COLOR = { .r = 100, .g = 100, .b = 100, .a = 255 };
            const char selection = SELECTED ? '>' : ' ';
			mu_draw_text(ctx, font, &selection, 1, mu_vec2(r.x, r.y), WHITE_COLOR);
            r.x += r_get_text_width(&selection, 1);

            std::string istr = std::to_string(i + 1); // TODO: right-pad.
            istr += ".";
			mu_draw_text(ctx, font, istr.c_str(), istr.size(), mu_vec2(r.x, r.y), 
                SELECTED ? WHITE_COLOR : GRAY_COLOR);
            r.x += r_get_text_width(istr.c_str(), istr.size());

			mu_draw_text(ctx, font, l.file->path.c_str(), l.file->path.size(), mu_vec2(r.x, r.y), 
                SELECTED ? WHITE_COLOR : GRAY_COLOR);
			r.x += r_get_text_width(l.file->path.c_str(), l.file->path.size());


			mu_draw_text(ctx, font, ":", 1, mu_vec2(r.x, r.y), GRAY_COLOR);
			r.x += r_get_text_width(":", 1);


            std::string linenostr = std::to_string(l.line + 1);
			mu_draw_text(ctx, font, linenostr.c_str(), linenostr.size(), mu_vec2(r.x, r.y), GRAY_COLOR);
			r.x += r_get_text_width(linenostr.c_str(), linenostr.size());

			mu_draw_text(ctx, font, " ", 1, mu_vec2(r.x, r.y), GRAY_COLOR);
			r.x += r_get_text_width(" ", 1);


			// [ix_line_begin, ix)
			mu_draw_text(ctx, font, l.file->buf + ix_line_begin, l.ix - ix_line_begin, mu_vec2(r.x, r.y), 
                SELECTED ? WHITE_COLOR : GRAY_COLOR);
			r.x += r_get_text_width(l.file->buf + ix_line_begin, l.ix - ix_line_begin);

			// [ix, ix_str_end)
			const int ix_str_end = l.ix + pal->input.size();
            mu_draw_text(ctx, font, l.file->buf + l.ix, ix_str_end - l.ix, mu_vec2(r.x, r.y),
                BLUE_COLOR);
			r.x += r_get_text_width(l.file->buf + l.ix, ix_str_end - l.ix);


			// [ix+search str, ix end)
			mu_draw_text(ctx, font, l.file->buf + ix_str_end, ix_line_end - ix_str_end, mu_vec2(r.x, r.y), 
                SELECTED ? WHITE_COLOR : GRAY_COLOR);
			r.x += r_get_text_width(l.file->buf + ix_str_end, ix_line_end - ix_str_end);

		} // end i
		mu_layout_end_column(ctx);
		mu_end_window(ctx);
    }// end mu_begin_window
}

void mu_bottom_line(mu_Context* ctx, BottomlineState* s) {
    mu_Font font = ctx->style->font;
    mu_Rect parent_body = mu_get_current_container(ctx)->body;
    int y = parent_body.y + parent_body.h - r_get_text_height();
    mu_draw_text(ctx, font, s->info.c_str(), s->info.size(), mu_vec2(0, y), ctx->style->colors[MU_COLOR_TEXT]);
}



void mu_viewer(mu_Context* ctx, ViewerState *view, FocusState *focus, const CommandPaletteState *pal) {

    if (view->focus.ix == -1) {
        return;
    }
    assert(view->focus.ix != -1);

    int width = -1;
    mu_Font font = ctx->style->font;

    mu_Id id = editor_state_mu_id(ctx, view); 
    mu_Container* cnt = mu_get_current_container(ctx);
    assert(cnt && "must be within container");


	mu_update_control(ctx, id, cnt->body, MU_OPT_HOLDFOCUS);
    const bool focused = *focus == FocusState::FSK_Viewer;
	if (focused) {

        /*
        if (ctx->key_pressed & MU_KEY_UPARROW) {
            editor_state_move_up(*view);
        }

        if (ctx->key_pressed & MU_KEY_DOWNARROW) {
            editor_state_move_down(*view);
        }

        if (ctx->key_pressed & MU_KEY_LEFTARROW) {
            editor_state_move_left(*view);
        }

        if (ctx->key_pressed & MU_KEY_RIGHTARROW) {
            editor_state_move_right(*view);
        }
        */

		if (ctx->key_pressed & MU_KEY_TAB) {
            *focus = FocusState::FSK_Palette;
        }
	}



    mu_layout_begin_column(ctx);
    mu_layout_row(ctx, 1, &width, ctx->text_height(font));
	// mu_draw_control_frame(ctx, id, cnt->body, MU_COLOR_BASE, 0);

    const int START_LINES_UP = 3;
    const int NLINES = 40;
    Loc left = view->focus;
    for (int i = 0; i < START_LINES_UP; ++i) { left = left.up(); }
    left = left.start_of_cur_line();
    assert(left.line == 0 || view->focus.line - left.line == START_LINES_UP);

	const mu_Color GRAY_COLOR = { .r = 180, .g = 180, .b = 180, .a = 255 };
	const mu_Color WHITE_COLOR = { .r = 255, .g = 255, .b = 255, .a = 255 };
	const mu_Color BLUE_COLOR = { .r = 187, .g = 222, .b = 251, .a = 255 };

    for (int line_offset = 0; line_offset < NLINES; ++line_offset) {
        mu_Rect r = mu_layout_next(ctx);

        // have exhausted text.
        if (left.eof()) { break; }


        const bool SELECTED = view->focus.line == left.line;

        // 1. draw line number
        char lineno_str[12];
        itoa(left.line, lineno_str, 10);
        const int total_len = 5;
        const int num_len = strlen(lineno_str);
        assert(num_len < total_len);
        for (int i = num_len; i < total_len; ++i) {
            lineno_str[i] = ' ';
        }
        lineno_str[total_len] = 0;
        lineno_str[total_len + 1] = 0;

        mu_draw_text(ctx, font, lineno_str, strlen(lineno_str), mu_vec2(r.x, r.y), 
           SELECTED ? WHITE_COLOR : GRAY_COLOR);
        r.x += ctx->text_width(font, lineno_str, strlen(lineno_str));

        // line number width
        assert(!left.eof());

        const Loc right = left.end_of_cur_line();
        assert(right.line == left.line);
        assert(right.eof() || is_newline(right.get()));


        r.h = ctx->text_height(font);
        // draw text.
        for (Loc cur = left; cur.ix < right.ix; cur = cur.advance()) {
			if (focused && 
                view->focus.line == cur.line && 
                view->focus.col == cur.col) {
				mu_draw_cursor(ctx, &r);
			}

            // the character is inside the query bounds.
            bool AT_QUERY = 
              pal->selected_ix < pal->matches.size() &&
                cur.ix >= pal->matches[pal->selected_ix].ix &&
                cur.ix < pal->matches[pal->selected_ix].ix + pal->input.size();
            const char c = cur.get();
            mu_draw_text(ctx, font, &c, 1, mu_vec2(r.x, r.y), 
              AT_QUERY ? BLUE_COLOR : SELECTED ? WHITE_COLOR : GRAY_COLOR);
            r.x += ctx->text_width(font, &c, 1);
		}

        // cursor
        if (view->focus.line == line_offset && view->focus.col == right.col) {
            mu_draw_cursor(ctx, &r);
        }

        // next line;
        assert(right.advance().eof() || left.line + 1 == right.advance().line);
        left = right.advance();
    }

    mu_layout_end_column(ctx);
}


static void viewer_window(mu_Context* ctx, ViewerState *ed, BottomlineState *bot, FocusState *focus, const CommandPaletteState *pal) {
    const int window_opts =  MU_OPT_NOTITLE | MU_OPT_NOCLOSE | MU_OPT_NORESIZE;
    // if (mu_begin_window_ex(ctx, "Editor", mu_rect(0, 0, 1400, 768), window_opts)) {
    if (mu_begin_window(ctx, "Editor", mu_rect(0, 720/2, 1400, 720/2))) { 
        int width_row[] = { -1 };
        mu_layout_row(ctx, 1, width_row, -25);
        mu_layout_row(ctx, 1, width_row, -1);
        mu_set_focus(ctx, editor_state_mu_id(ctx, ed));
        mu_viewer(ctx, ed, focus, pal);
		mu_bottom_line(ctx, bot);
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
    if (sdl_key == SDLK_TAB) {
        return MU_KEY_TAB;
    }
    return 0;

}


static int text_width(mu_Font font, const char* text, int len) {
    if (len == -1) { len = strlen(text); }
    return r_get_text_width(text, len);
}

static int text_height(mu_Font font) {
    return r_get_text_height();
}


struct TaskManager {
    bool indexing; // tracks whether index is being built.
    std::filesystem::recursive_directory_iterator ix_it; // iterator to index walker.
    std::optional<Loc> index_loc; // current location being read by index.

    int query_sequence_number;

    // pairs of trie nodes, and how much of the query length they match.
    std::stack<TrieNode*> query_walk_stack;
};


void task_manager_explore_directory_timeslice(TaskManager *s, BottomlineState *bot) {
	assert(s->indexing);
    if (s->ix_it == std::filesystem::end(s->ix_it)) {
        s->indexing = false;
        return;
    };

    std::filesystem::path curp = *s->ix_it;
    s->ix_it++;
	bot->info = "walking: " + curp.string();
    if (!std::filesystem::is_regular_file(curp)) {
        return;
    }

    assert(std::filesystem::is_regular_file(curp));
	FILE* fp = _wfopen(curp.c_str(), L"rb");
	assert(fp && "unable to open file");

	fseek(fp, 0, SEEK_END);
	const int total_size = ftell(fp);

	s->index_loc = Loc();
	s->index_loc->file = new File(curp.string(), total_size);
	s->index_loc->file->buf = new char[total_size];
	s->index_loc->ix = 0;
	s->index_loc->line = 0;
	s->index_loc->col = 0;

	fseek(fp, 0, SEEK_SET);
	const int nread = fread(s->index_loc->file->buf, 1, total_size, fp);
	fclose(fp);
	assert(nread == total_size && "unable to read file");
	bot->info = "ix: " + curp.string() + " | 0%";
}

void task_manager_index_file_timeslice(TaskManager *s, BottomlineState *bot, TrieNode *g_index) {
    assert(s->index_loc);
    assert(s->index_loc->valid());

	while (!s->index_loc->eof()  && is_whitespace(s->index_loc->get())) {
        s->index_loc = s->index_loc->advance();
	}

	assert(s->index_loc->valid());
	if (s->index_loc->eof()) {
        bot->info = "DONE indexing " + s->index_loc->file->path;
		s->index_loc = {};
		return;
	}
	assert(!is_whitespace(s->index_loc->get()));

	Loc eol = *s->index_loc;
	static const int MAX_SUFFIX_LEN = 80;
	static const int MAX_NGRAMS = 3;
	int ngrams = 0;
	while (!eol.eof() && !is_newline(eol.get()) && ngrams < MAX_NGRAMS && (eol.ix - s->index_loc->ix) <= MAX_SUFFIX_LEN) {
		if (is_whitespace(eol.get())) { ngrams++; }
		eol = eol.advance();
	}

	for (Loc sufloc = *s->index_loc; sufloc.ix < eol.ix; sufloc = sufloc.advance()) {
		index_add(g_index,
			s->index_loc->file->buf + sufloc.ix,
			eol.ix - sufloc.ix, sufloc);
		assert(!sufloc.eof());
	}

	const float percent = 100.0 * ((float)s->index_loc->ix / s->index_loc->file->len);
	bot->info = "ix: ";
    bot->info += s->index_loc->file->path;
	bot->info += " | ";
	bot->info += std::to_string(percent);
	bot->info += " | ";
	for (int i = s->index_loc->ix; i < eol.ix; ++i) {
		bot->info += s->index_loc->file->buf[i];
	}

	s->index_loc = eol;
}


// TODO: I need some way to express that TaskManager is only alowed to insert into pal->matches.
// must be monotonic.
void task_manager_query_timeslice(TaskManager* s, CommandPaletteState *pal, TrieNode *g_index) {
    assert(s->query_sequence_number <= pal->sequence_number);
    if (s->query_sequence_number < pal->sequence_number) {
        s->query_sequence_number = pal->sequence_number;
        s->query_walk_stack = std::stack<TrieNode* >();

        if (pal->input.size() == 0) { 
            return;
        }

        TrieNode* cur = index_lookup(g_index, pal->input.c_str(), pal->input.size());
        if (!cur) {
            return;
        }
		s->query_walk_stack.push(cur);
    }

    if (s->query_walk_stack.empty()) {
        return;
    }

    TrieNode* top = s->query_walk_stack.top();
    s->query_walk_stack.pop();
    pal->matches.insert(pal->matches.end(), top->data.begin(), top->data.end());
    for (auto it : top->next) {
        s->query_walk_stack.push(it.second);
    }

}

void task_manager_run_timeslice(TaskManager *s, CommandPaletteState *pal, BottomlineState *bot, TrieNode *g_index) {
    if (s->indexing) {
        if (!s->index_loc) {
            task_manager_explore_directory_timeslice(s, bot);
        }
        else {
            task_manager_index_file_timeslice(s, bot, g_index);
        }
    }
	task_manager_query_timeslice(s, pal, g_index);
}

int main(int argc, char** argv) {
    setlocale(LC_ALL, "");
    
    const std::filesystem::path root_path(argc == 1 ? "C:\\Users\\bollu\\phd\\lean4\\src\\" : argv[1]);
    std::cout << "root_path: " << root_path << "\n";

    TaskManager g_task_manager;
    g_task_manager.indexing = true;
    g_task_manager.ix_it = std::filesystem::recursive_directory_iterator(root_path);

    TrieNode g_index(nullptr);
    g_index.parent = &g_index; // root node points to itself.

    BottomlineState g_bottom_line_state;
    g_bottom_line_state.info = "WELCOME";

    CommandPaletteState g_command_palette_state;
    ViewerState g_viewer_state;

    FocusState g_focus_state = FocusState::FSK_Palette;

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
		const clock_t clock_begin = clock();
		do {
			task_manager_run_timeslice(&g_task_manager, &g_command_palette_state, &g_bottom_line_state, &g_index);
		} while (clock() - clock_begin < TARGET_CLOCKS_PER_FRAME * 0.5);

        /* process frame */
		mu_finalize_events_begin_draw(ctx);
		viewer_window(ctx, &g_viewer_state, &g_bottom_line_state, &g_focus_state, &g_command_palette_state);
		mu_command_palette(ctx, &g_viewer_state, &g_command_palette_state, &g_focus_state);
		mu_end(ctx);

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

