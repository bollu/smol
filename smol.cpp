// TODO: the art of multiprocessor programming.
#include <SDL.h>
#include <SDL_opengl.h>
#include <assert.h>

#include <cstdlib>
#define main main
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#include "microui-header.h"
#include "renderer.h"
#include "sdl/include/SDL_keycode.h"
#include "string.h"
// #include <format>
#include <time.h>
#include <tree_sitter/api.h>

// TODO: move this into a struct.
int width = -1;
int height = -1;

const int TARGET_FRAMES_PER_SECOND = 15.0;
const clock_t TARGET_CLOCKS_PER_FRAME =
    CLOCKS_PER_SEC / TARGET_FRAMES_PER_SECOND;

// TODO: Need a language that can define projections into a large struct.
// Useful for having god objects, where you hand different views of the god
// objct to different people.

// https://15721.courses.cs.cmu.edu/spring2018/papers/09-oltpindexes2/leis-icde2013.pdf
// Ukkonen

struct File {
    std::string path;
    char *buf = nullptr;
    int len = 0;
    File(std::string path, int len) : path(path), len(len){};
};

using hash = long long;

bool is_newline(char c) { return c == '\r' || c == '\n'; }

bool is_whitespace(char c) {
    return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}

struct Loc {
    File *file = nullptr;
    int ix = -1;  // Loc points at file->buf[ix]
    int line = -1;
    int col = -1;

    Loc() {}
    // TODO: don't store the string.
    Loc(File *file, int ix, int line, int col)
        : file(file), ix(ix), line(line), col(col){};

    bool operator==(const Loc &other) const {
        // assert(other.file == this->file);
        if (other.file != this->file) {
            return false;
        }
        bool eq = this->ix == other.ix;
        if (eq) {
            assert(this->line == other.line);
            assert(this->col == other.col);
        }
        return eq;
    }

    bool operator!=(const Loc &other) const { return !(*this == other); }

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
        if (eof()) {
            return *this;
        }

        if (false && file->buf[this->ix] == '\r') {
            assert(ix + 1 < file->len);
            assert(file->buf[this->ix + 1] == '\n');
            return Loc(file, ix + 2, line + 1, 0);
        } else if (file->buf[this->ix] == '\n') {
            return Loc(file, ix + 1, line + 1, 0);
        } else {
            return Loc(file, ix + 1, line, col + 1);
        }
    }

    // vv NOTE: retreat() checks its correctness in terms of advance()
    // because advance() is the much simpler primitive.
    Loc retreat() const {
        assert(valid());
        Loc l = *this;
        if (l.ix == 0) {
            return l;
        }

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
            while (l.ix - l.col > 0 &&
                   !is_newline(l.file->buf[l.ix - l.col - 1])) {
                l.col++;
            }

            assert(l.advance() == *this);
            assert(l.valid());
            return l;
        } else {
            l.ix -= 1;
            l.col -= 1;
            assert(l.advance() == *this);
            assert(l.valid());
            return l;
        }
    }

    Loc start_of_cur_line() const {
        assert(valid());
        Loc l = *this;
        l.ix -= col;
        l.col = 0;
        assert(l.valid());
        return l;
    }

    Loc start_of_next_line() const {
        assert(valid());
        Loc l = *this;
        while (!l.eof() && this->line == l.line) {
            l = l.advance();
        }
        assert(l.valid());
        return l;
    }

    Loc end_of_cur_line() const {
        assert(valid());
        Loc l = *this;
        while (!l.eof() && !is_newline(l.get())) {
            l = l.advance();
        }
        assert(l.valid());
        return l;
    }

    Loc up() const {
        assert(valid());
        Loc l = *this;
        l = this->start_of_cur_line();
        l = l.retreat();
        assert(l.valid());
        return l;
    }

    // TODO: ask @codelegend for refactoring.
    Loc down() const {
        assert(valid());
        Loc l = this->end_of_cur_line();
        l = l.advance();
        assert(l.valid());
        return l;
    }
};

struct TrieNode;
struct TrieEdge {
    static int NUM_TRIE_EDGES;
    File *f = nullptr;
    int ix = -1;
    int len = 0;
    TrieNode *node = nullptr;
    TrieEdge() { NUM_TRIE_EDGES++; }
    TrieEdge(File *f, int ix, int len, TrieNode *node)
        : f(f), ix(ix), len(len), node(node) {
        NUM_TRIE_EDGES++;
    };
};

int TrieEdge::NUM_TRIE_EDGES = 0;

struct TrieNode {
    static int NUM_TRIE_NODES;
    std::unordered_map<int, TrieEdge> adj;
    std::vector<Loc> data;
    TrieNode() { NUM_TRIE_NODES++; }
};

int TrieNode::NUM_TRIE_NODES = 0;

void index_add(TrieNode *index, File *f, int ix, int totlen, Loc data) {
    assert(f);
    assert(ix >= 0);
    assert(ix <= f->len);
    assert(totlen >= 0);

    if (totlen == 0) {
        index->data.push_back(data);
        return;
    }
    assert(totlen > 0);

    const char ctip = f->buf[ix];
    if (!index->adj.count(ctip)) {
        TrieNode *n = new TrieNode;
        n->data.push_back(data);
        index->adj[ctip] = TrieEdge(f, ix, totlen, n);
        return;
    }
    assert(index->adj.count(ctip));
    const TrieEdge e = index->adj[ctip];

    int matchlen = 0;
    while (matchlen < totlen && matchlen < e.len &&
           f->buf[ix] == e.f->buf[e.ix + matchlen]) {
        matchlen++;
    }

    // we have matched along this edge fully, so we can go to
    // the next node.
    if (matchlen == e.len) {
        index = e.node;
        totlen -= matchlen;
        ix += matchlen;
        return index_add(e.node, f, ix, totlen, data);
    }

    // we have matched partially on the edge.
    // we need to split.
    assert(matchlen < e.len);
    TrieNode *leaf = new TrieNode();
    leaf->data.push_back(data);

    // [OLD] index ---ctip:e ---> rest
    // [NEW] index --ctip:e--> cur_leaf --crest:erest --> rest
    // create a new edge from leaf -> rest
    const char crest = e.f->buf[e.ix + matchlen];

    // (2) cur_leaf --crest:erest --> rest
    leaf->adj[crest] = e;
    leaf->adj[crest].len -= matchlen;
    leaf->adj[crest].ix += matchlen;

    // (1) index ---ctip:e ---> leaf
    index->adj[ctip].len = matchlen;
    index->adj[ctip].node = leaf;
    return;
};

const TrieNode *index_lookup(const TrieNode *index, const char *key, int len) {
    assert(index);
    assert(len >= 0);
    if (len == 0) {
        return index;
    }
    const char c = key[0];
    auto it = index->adj.find(c);
    if (it == index->adj.end()) {
        return nullptr;
    }
    if (!index->adj.count(c)) {
        return nullptr;
    }

    const TrieEdge e = it->second;
    int i = 0;
    const char *estr = e.f->buf + e.ix;
    while (i < e.len && i < len && estr[i] == key[i]) {
        i++;
    }

    if (i < e.len) {
        // we haven't reached a node. quit.
        return nullptr;
    }

    assert(i == e.len);
    index = e.node;
    key += i;
    len -= i;
    return index_lookup(e.node, key, len);
}

enum {
    KEY_SHIFT = (1 << 0),
    KEY_CTRL = (1 << 1),
    KEY_ALT = (1 << 2),
    KEY_BACKSPACE = (1 << 3),
    KEY_RETURN = (1 << 4),
    KEY_LEFTARROW = (1 << 5),
    KEY_RIGHTARROW = (1 << 6),
    KEY_UPARROW = (1 << 7),
    KEY_DOWNARROW = (1 << 8),
    KEY_COMMAND_PALETTE = (1 << 9),
    KEY_TAB = (1 << 10),
    KEY_D = (1 << 11),
    KEY_U = (1 << 12),
    KEY_H = (1 << 13),
    KEY_J = (1 << 14),
    KEY_K = (1 << 15),
    KEY_L = (1 << 16),
    KEY_C = (1 << 17),
    KEY_A = (1 << 18),
    KEY_E = (1 << 19),
    KEY_I = (1 << 20),
};

struct EventState {
    int key_pressed;    // true if key was pressed this frame. reset each frame.
    int key_held_down;  // true if key was held down.
    char input_text[32];

    EventState() {
        this->key_pressed = 0;
        this->key_held_down = 0;
        this->input_text[0] = 0;
    }

    void start_frame() {
        this->key_pressed = 0;
        this->input_text[0] = 0;
    }

    void set_keydown(int key) {
        this->key_pressed |= key;
        this->key_held_down |= key;
    }

    void set_keyup(int key) { this->key_held_down &= ~key; }

    void set_input_text(const char *text) {
        printf("text: |%s|\n", text);
        int len = strlen(input_text);
        int size = strlen(text) + 1;
        assert(len + size <= (int)sizeof(input_text));
        memcpy(input_text + len, text, size);
    }
};

// TODO: rename to viewer.
static const int MAXLINELEN = 120;
static const int MAXLINES = 1e6;

struct Cursor {
    int line = 0;
    int col = 0;
};

// editor mode
enum EditMode { Insert, Normal, Visual };

// the state of the editor is a geodesic?
struct EditorState {
    EditMode mode = Normal;
    char text[MAXLINES][MAXLINELEN];
    int linelen[MAXLINES];
    EditorState() {
        for (int line = 0; line < MAXLINES; ++line) {
            linelen[line] = 0;
            for (int col = 0; col < MAXLINELEN; ++col) {
                text[line][col] = 0;
            }
        }
    }
};

Cursor cursor_up(EditorState *editor, Cursor cursor) {
    cursor.line = std::max<int>(0, cursor.line - 1);
    cursor.col = std::min<int>(editor->linelen[cursor.line], cursor.col);
    return cursor;
};

Cursor cursor_down(EditorState *editor, Cursor cursor) {
    cursor.line = std::min<int>(MAXLINES - 1, cursor.line + 1);
    cursor.col = std::min<int>(editor->linelen[cursor.line], cursor.col);
    return cursor;
};

Cursor cursor_dollar(EditorState *editor, Cursor cursor) {
    cursor.col = editor->linelen[cursor.line];
    return cursor;
}

Cursor cursor_hat(EditorState *editor, Cursor cursor) {
    cursor.col = 0;
    return cursor;
};

// insert code into editor at cursor, and move cursor by string length.
// TODO: refactor in terms of editor commands
Cursor cursor_insert_str(EditorState *editor, Cursor cursor, const char *buf,
                         int len) {
    int *linelen = &editor->linelen[cursor.line];
    char *line = editor->text[cursor.line];

    // printf("insert str %d:%d(%s)| old: %s:%d\n", editor->cursor.line,
    // editor->cursor.col,

    if (*linelen == MAXLINELEN) {
        assert(false && "unhandled line break");
    }
    assert(*linelen < MAXLINELEN);
    const int cur_begin = cursor.col;
    const int new_begin = cur_begin + len;
    const int movelen = *linelen - cur_begin;
    for (int i = movelen - 1; i >= 0; i--) {
        line[new_begin + i] = line[cur_begin + i];
    }
    for (int i = 0; i < len; ++i) {
        line[cur_begin + i] = buf[i];
    }
    *linelen += len;
    cursor.col += len;
    return cursor;
}

// TOOD: refactor in terms of editor commands.
Cursor cursor_delete_till_end_of_line(EditorState *editor, Cursor cursor) {
    // clear text.
    for (int i = cursor.col; i < editor->linelen[cursor.line]; ++i) {
        editor->text[cursor.line][i] = 0;
    }
    // adjust line length
    editor->linelen[cursor.line] = cursor.col;
    return cursor;
}

// TODO: refactor in terms of editor commands
Cursor cursor_delete_backward(EditorState *editor, Cursor cursor, int n) {
    assert(n >= 0);
    const int begin = std::max<int>(0, cursor.col - n);
    for (int i = begin; i < editor->linelen[cursor.line] - n; ++i) {
        editor->text[cursor.line][i] = editor->text[cursor.line][n + i];
    }
    for (int i = editor->linelen[cursor.line] - n;
         i < editor->linelen[cursor.line]; ++i) {
        editor->text[cursor.line][i] = 0;
    }

    editor->linelen[cursor.line] -= n;
    cursor.col -= n;
    return cursor;
}

// splice s of len `len` into line `l` beginning at col `col`. We must have
// 0 <= col <= editor->lineline[line].
void editor_splice_into_line(EditorState *editor, int line, int col, char *s,
                             int len) {
    assert(col >= 0);
    assert(col <= editor->linelen[line]);

    if (s == nullptr) {
        assert(len == 0);
    }

    int *linelen = &editor->linelen[line];
    char *linestr = editor->text[col];

    if (*linelen == MAXLINELEN) {
        assert(false && "unhandled line break");
    }
    assert(*linelen < MAXLINELEN);
    const int cur_begin = col;
    const int new_begin = cur_begin + len;
    const int movelen = *linelen - cur_begin;
    for (int i = movelen - 1; i >= 0; i--) {
        linestr[new_begin + i] = linestr[cur_begin + i];
    }
    for (int i = 0; i < len; ++i) {
        linestr[cur_begin + i] = s[i];
    }
    *linelen += len;
}

// TODO: rewrite in terms of editor_splice_into_line.
void editor_append_line(EditorState *editor, int line, char *s, int len) {
    if (s == nullptr) {
        assert(len == 0);
    }

    const int oldlen = editor->linelen[line];
    editor->linelen[line] += len;
    for (int i = 0; i < len; ++i) {
        editor->text[line][oldlen + i] = s[i];
    }
}

void editor_set_line(EditorState *editor, int line, char *s, int len) {
    if (s == nullptr) {
        assert(len == 0);
    }

    const int oldlen = editor->linelen[line];
    editor->linelen[line] = len;
    for (int i = 0; i < len; ++i) {
        editor->text[line][i] = s[i];
    }
    for (int i = editor->linelen[line]; i < oldlen; ++i) {
        editor->text[line][i] = 0;
    }
}

void editor_copy_line(EditorState *editor, int destix, int srcix) {
    editor_set_line(editor, destix, editor->text[srcix],
                    editor->linelen[srcix]);
}

void editor_remove_line(EditorState *editor, int line) {
    for (int l = line; l < MAXLINES - 1; ++l) {
        editor_copy_line(editor, l, l + 1);
    }
}

// create an empty line before line.
void editor_create_line_before(EditorState *editor, int line) {
    assert(line >= 0 && line < MAXLINES);
    for (int l = MAXLINES - 1; l > line; l--) {
        editor_copy_line(editor, l, l - 1);
    }
}

// create an empty line after line.
void editor_create_line_after(EditorState *editor, int line) {
    editor_create_line_before(editor, line + 1);
}

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
    // invariant: selected_ix <= matches.len(). Is equal to denote deselected
    // index.
    int selected_ix = 0;
};

mu_Id editor_state_mu_id(mu_Context *ctx, EditorState *editor) {
    return mu_get_id(ctx, &editor, sizeof(EditorState *));
}

void mu_draw_cursor(mu_Context *ctx, mu_Rect *r, EditMode mode) {
    mu_Rect cursor = *r;
    mu_Rect cursor_new = *r;

    cursor_new.h = r_get_text_height();
    cursor_new.w = mode == EditMode::Insert ? 3 : r_get_text_width(" ", 1);
    mu_draw_rect(ctx, cursor_new, mu_Color(0, 255, 0, 255));

    // mu_Font font = ctx->_style.font;
    // const char *c = mode == EditMode::Insert ? "|" : "-";
    // const int width = r_get_text_width(c, 1);
    // mu_draw_rect(ctx, 
    // mu_draw_text(ctx, font, c, 1, mu_vec2(r->x - width / 2, r->y),
    //              ctx->_style.colors[MU_COLOR_TEXT]);
    // r->w += width;
}

void mu_command_palette(mu_Context *ctx, EventState *event, EditorState *editor,
                        CommandPaletteState *pal, FocusState *focus) {
    assert(pal->selected_ix <= (int)pal->matches.size());

    const bool focused = *focus == FocusState::FSK_Palette;
    if (mu_begin_window(ctx, "ERROR", mu_Rect(0, 0, width, 0))) {
        if (focused) {
            if (event->key_pressed & KEY_BACKSPACE) {
                pal->sequence_number++;
                pal->input.resize(std::max<int>(0, pal->input.size() - 1));
            }

            // TODO: Ask @codelegend for clean way to handle this.
            if (event->key_pressed & KEY_DOWNARROW) {
                pal->selected_ix = std::min<int>(pal->selected_ix + 1,
                                                 pal->matches.size() - 1);
            }

            if (event->key_pressed & KEY_UPARROW) {
                pal->selected_ix = std::max<int>(0, pal->selected_ix - 1);
            }

            if (event->key_pressed & KEY_RETURN) {
                *focus = FocusState::FSK_Viewer;
            }

            if (strlen(event->input_text)) {
                /* handle key press. stolen from mu_textbox_raw */
                pal->sequence_number++;
                pal->input += std::string(event->input_text);
                pal->matches = {};
                pal->selected_ix = 0;
            }

            if (pal->selected_ix < pal->matches.size()) {
                assert(false && "do not know how to focus");
                // editor->set_focus(pal->matches[pal->selected_ix]);
            }
        }

        mu_Font font = ctx->_style.font;

        mu_layout_begin_column(ctx);
        const int width[] = {800};
        mu_layout_row(ctx, 1, width, ctx->text_height(font));
        mu_Rect r = mu_layout_next(ctx);
        // mu_draw_text(ctx, font, "Command Palette", strlen("Command Palette"),
        // mu_vec2(r.x, r.y), ctx->_style.colors[MU_COLOR_TEXT]);
        // mu_draw_text(ctx, font, state->input.c_str(), state->input.size(),
        // mu_vec2(0, 20), ctx->_style.colors[MU_COLOR_TEXT]);
        const mu_Color BLUE_COLOR = {.r = 187, .g = 222, .b = 251, .a = 255};
        mu_draw_text(ctx, font, pal->input.c_str(), pal->input.size(),
                     mu_vec2(r.x, r.y), BLUE_COLOR);
        r.x += r_get_text_width(pal->input.c_str(), pal->input.size());
        if (focused) {
            mu_draw_text(ctx, font, "|", 1, mu_vec2(r.x, r.y), BLUE_COLOR);
        }

        static const int NUM_ANSWERS = 80;
        static const int TOP_OFFSET = 3;
        for (int i = std::max<int>(0, pal->selected_ix - TOP_OFFSET);
             i < pal->matches.size() && i < NUM_ANSWERS; ++i) {
            const Loc l = pal->matches[i];
            int ix_line_end = l.ix;
            while (ix_line_end < l.file->len &&
                   !is_newline(l.file->buf[ix_line_end])) {
                ix_line_end++;
            }
            int ix_line_begin = l.ix;

            while (ix_line_begin > 0 &&
                   !is_newline(l.file->buf[ix_line_begin])) {
                ix_line_begin--;
            }

            mu_Rect r = mu_layout_next(ctx);

            const bool SELECTED = focused && (i == pal->selected_ix);
            const mu_Color WHITE_COLOR = {
                .r = 255, .g = 255, .b = 255, .a = 255};
            const mu_Color GRAY_COLOR = {
                .r = 100, .g = 100, .b = 100, .a = 255};
            const char selection = SELECTED ? '>' : ' ';
            mu_draw_text(ctx, font, &selection, 1, mu_vec2(r.x, r.y),
                         WHITE_COLOR);
            r.x += r_get_text_width(&selection, 1);

            std::string istr = std::to_string(i + 1);  // TODO: right-pad.
            istr += ".";
            mu_draw_text(ctx, font, istr.c_str(), istr.size(),
                         mu_vec2(r.x, r.y),
                         SELECTED ? WHITE_COLOR : GRAY_COLOR);
            r.x += r_get_text_width(istr.c_str(), istr.size());

            mu_draw_text(ctx, font, l.file->path.c_str(), l.file->path.size(),
                         mu_vec2(r.x, r.y),
                         SELECTED ? WHITE_COLOR : GRAY_COLOR);
            r.x += r_get_text_width(l.file->path.c_str(), l.file->path.size());

            mu_draw_text(ctx, font, ":", 1, mu_vec2(r.x, r.y), GRAY_COLOR);
            r.x += r_get_text_width(":", 1);

            std::string linenostr = std::to_string(l.line + 1);
            mu_draw_text(ctx, font, linenostr.c_str(), linenostr.size(),
                         mu_vec2(r.x, r.y), GRAY_COLOR);
            r.x += r_get_text_width(linenostr.c_str(), linenostr.size());

            mu_draw_text(ctx, font, " ", 1, mu_vec2(r.x, r.y), GRAY_COLOR);
            r.x += r_get_text_width(" ", 1);

            // [ix_line_begin, ix)
            mu_draw_text(ctx, font, l.file->buf + ix_line_begin,
                         l.ix - ix_line_begin, mu_vec2(r.x, r.y),
                         SELECTED ? WHITE_COLOR : GRAY_COLOR);
            r.x += r_get_text_width(l.file->buf + ix_line_begin,
                                    l.ix - ix_line_begin);

            // [ix, ix_str_end)
            const int ix_str_end = l.ix + pal->input.size();
            mu_draw_text(ctx, font, l.file->buf + l.ix, ix_str_end - l.ix,
                         mu_vec2(r.x, r.y), BLUE_COLOR);
            r.x += r_get_text_width(l.file->buf + l.ix, ix_str_end - l.ix);

            // [ix+search str, ix end)
            mu_draw_text(ctx, font, l.file->buf + ix_str_end,
                         ix_line_end - ix_str_end, mu_vec2(r.x, r.y),
                         SELECTED ? WHITE_COLOR : GRAY_COLOR);
            r.x += r_get_text_width(l.file->buf + ix_str_end,
                                    ix_line_end - ix_str_end);

        }  // end i
        mu_layout_end_column(ctx);
        mu_end_window(ctx);
    }  // end mu_begin_window
}

void mu_bottom_line(mu_Context *ctx, BottomlineState *s) {
    mu_Font font = ctx->_style.font;
    mu_Rect parent_body = mu_get_current_container(ctx)->body;
    int y = parent_body.y + parent_body.h - r_get_text_height();
    mu_draw_text(ctx, font, s->info.c_str(), s->info.size(), mu_vec2(0, y),
                 ctx->_style.colors[MU_COLOR_TEXT]);
}

void mu_editor(mu_Context *ctx, EventState *event, EditorState *editor,
               FocusState *focus, const CommandPaletteState *pal) {
    static Cursor cursor;

    mu_Font font = ctx->_style.font;

    mu_Id id = editor_state_mu_id(ctx, editor);
    mu_Container *cnt = mu_get_current_container(ctx);
    assert(cnt && "must be within container");

    const int N_SCROLL_STEPS = 15;
    const bool focused = true;
    if (focused) {
        for (int i = 0; i < strlen(event->input_text); ++i) {
            const char c = event->input_text[i];

            // if (event->key_held_down & KEY_CTRL && c == 'c') {
            //     editor->mode = EditMode::Normal;
            // }

            if (!(event->key_held_down & KEY_CTRL) &&
                editor->mode == EditMode::Insert) {
                cursor = cursor_insert_str(editor, cursor, &c, 1);
            } 
        }

        if (editor->mode == EditMode::Normal && event->key_held_down & KEY_I) {
            editor->mode = EditMode::Insert;
        }

        if (event->key_held_down & KEY_CTRL && event->key_pressed & KEY_A) {
            cursor.col = 0;
        }

        if (event->key_held_down & KEY_CTRL && event->key_pressed & KEY_E) {
            cursor = cursor_dollar(editor, cursor);
        }


        if (event->key_held_down & KEY_CTRL && event->key_pressed & KEY_C) {
            editor->mode = EditMode::Normal;
        }

        if (event->key_held_down & KEY_CTRL && event->key_pressed & KEY_D) {
            for (int i = 0; i < N_SCROLL_STEPS; ++i) {
                cursor = cursor_down(editor, cursor);
            }
        }

        if (event->key_held_down & KEY_CTRL && event->key_pressed & KEY_U) {
            for (int i = 0; i < N_SCROLL_STEPS; ++i) {
                cursor = cursor_up(editor, cursor);
            }
        }

        if (event->key_pressed & KEY_UPARROW || (editor->mode == EditMode::Normal && event->key_pressed & KEY_K)) {
            cursor = cursor_up(editor, cursor);
        }

        if (event->key_pressed & KEY_DOWNARROW || (editor->mode == EditMode::Normal && event->key_pressed & KEY_J)) {
            cursor = cursor_down(editor, cursor);
        }

        if (event->key_pressed & KEY_LEFTARROW || (editor->mode == EditMode::Normal && event->key_pressed & KEY_H)) {
            cursor.col = std::max<int>(cursor.col - 1, 0);
        }

        if (event->key_pressed & KEY_RIGHTARROW || (editor->mode == EditMode::Normal && event->key_pressed & KEY_L)) {
            cursor.col =
                std::min<int>(cursor.col + 1, editor->linelen[cursor.line]);
        }

        if (editor->mode == EditMode::Insert && event->key_pressed & KEY_BACKSPACE) {
            if (cursor.col == 0) {
                // TODO: think of how this codde can be cleaned up!
                // In terms of differential geometry :)
                // If we attach the space back on itself twisted (ala mobius
                // strip), does that give us the correct implementation of
                // cursors? I think so!
                if (cursor.line == 0) {
                    return;
                }
                assert(cursor.line > 0);
                // join previous line into currentline.
                cursor = cursor_dollar(editor, cursor_up(editor, cursor));
                editor_append_line(editor, cursor.line,
                                   editor->text[cursor.line + 1],
                                   editor->linelen[cursor.line + 1]);
                editor_remove_line(editor, cursor.line + 1);

            } else {
                cursor = cursor_delete_backward(editor, cursor, 1);
            }
        }

        if (editor->mode==EditMode::Insert && event->key_pressed & KEY_RETURN) {
            int len = editor->linelen[cursor.line] - cursor.col;
            char *s = editor->text[cursor.line] + cursor.col;
            // copy text to next line
            Cursor new_cursor = cursor_down(editor, cursor);
            new_cursor.col = 0;
            cursor_insert_str(editor, new_cursor, s, len);
            cursor_delete_till_end_of_line(editor, cursor);
            cursor = new_cursor;
        }
    }

    mu_layout_begin_column(ctx);
    int width = -1;
    mu_layout_row(ctx, 1, &width, ctx->text_height(font));
    // mu_draw_control_frame(ctx, id, cnt->body, MU_COLOR_BASE, 0);

    const int NLINES = height / ctx->text_height(font);

    const mu_Color DARKGRAY_COLOR = {.r = 100, .g = 100, .b = 100, .a = 255};
    const mu_Color GRAY_COLOR = {.r = 180, .g = 180, .b = 180, .a = 255};
    const mu_Color WHITE_COLOR = {.r = 255, .g = 255, .b = 255, .a = 255};
    const mu_Color BLUE_COLOR = {.r = 187, .g = 222, .b = 251, .a = 255};

    const int line_begin = std::max<int>(0, cursor.line - NLINES / 2);
    for (int line = line_begin; line < line_begin + NLINES; ++line) {
        mu_Rect r = mu_layout_next(ctx);

        const bool SELECTED = cursor.line == line;

        // 1. draw line number
        static const int MAX_LINE_STRLEN = 5;
        char lineno_str[MAX_LINE_STRLEN];
        sprintf(lineno_str, "%d", line);
        const int num_len = strlen(lineno_str);
        assert(strlen(lineno_str) < MAX_LINE_STRLEN - 1);
        for (int i = num_len; i < MAX_LINE_STRLEN; ++i) {
            lineno_str[i] = ' ';
        }
        lineno_str[MAX_LINE_STRLEN - 1] = 0;

        mu_draw_text(ctx, font, lineno_str, strlen(lineno_str),
                     mu_vec2(r.x, r.y), SELECTED ? WHITE_COLOR : GRAY_COLOR);
        r.x += ctx->text_width(font, lineno_str, strlen(lineno_str));
        r.h = ctx->text_height(font);
        // draw text. yes less than or equals to enable writing of cursor.
        for (int col = 0; col <= editor->linelen[line]; ++col) {
            if (focused && line == cursor.line && col == cursor.col) {
                mu_draw_cursor(ctx, &r, editor->mode);
            }

            // the character is within [NSCROLL] distance from cursor
            bool IN_SCROLL_RANGE = abs(line - cursor.line) <= N_SCROLL_STEPS;
            // the character is inside the query bounds.
            bool AT_QUERY = false;
            // bool AT_QUERY =
            //     pal->selected_ix < pal->matches.size() &&
            //     cur.ix >= pal->matches[pal->selected_ix].ix &&
            //     cur.ix < pal->matches[pal->selected_ix].ix +
            //     pal->input.size();
            const char c = editor->text[line][col];
            mu_draw_text(ctx, font, &c, 1, mu_vec2(r.x, r.y),
                         AT_QUERY   ? BLUE_COLOR
                         : SELECTED ? WHITE_COLOR
                         : IN_SCROLL_RANGE ? GRAY_COLOR : DARKGRAY_COLOR);
            r.x += ctx->text_width(font, &c, 1);
        }

        // cursor
        // if (editor->cursor.line == line && editor->focus.col ==
        // right.col) {
        //     mu_draw_cursor(ctx, &r);
        // }

        // next line;
        // assert(right.advance().eof() || left.line + 1 ==
        // right.advance().line); left = right.advance();
    }

    mu_layout_end_column(ctx);
}

static void editor_window(mu_Context *ctx, EventState *event,
                          EditorState *editor, BottomlineState *bot,
                          FocusState *focus, const CommandPaletteState *pal) {
    if (mu_begin_window(ctx, "Editor", mu_rect(0, 0, width, height))) {
        int width_row[] = {-1};
        mu_layout_row(ctx, 1, width_row, -25);
        mu_layout_row(ctx, 1, width_row, -1);
        mu_editor(ctx, event, editor, focus, pal);
        mu_bottom_line(ctx, bot);
        mu_end_window(ctx);
    }
}

int button_map(int sdl_key) {
    if (sdl_key == SDL_BUTTON_LEFT) {
        return MU_MOUSE_LEFT;
    } else if (sdl_key == SDL_BUTTON_RIGHT) {
        return MU_MOUSE_RIGHT;
    } else if (sdl_key == SDL_BUTTON_MIDDLE) {
        return MU_MOUSE_MIDDLE;
    }
    return 0;
}

int key_map(int sdl_key) {
    if (sdl_key == SDLK_LSHIFT) return KEY_SHIFT;
    if (sdl_key == SDLK_RSHIFT) return KEY_SHIFT;
    if (sdl_key == SDLK_LCTRL || sdl_key == SDLK_RCTRL) {
        return KEY_CTRL;
    }
    if (sdl_key == SDLK_RCTRL) return KEY_CTRL;
    if (sdl_key == SDLK_LALT) return KEY_ALT;
    if (sdl_key == SDLK_RALT) return KEY_ALT;
    if (sdl_key == SDLK_RETURN) return KEY_RETURN;
    if (sdl_key == SDLK_UP) return KEY_UPARROW;
    if (sdl_key == SDLK_DOWN) return KEY_DOWNARROW;
    if (sdl_key == SDLK_LEFT) return KEY_LEFTARROW;
    if (sdl_key == SDLK_RIGHT) return KEY_RIGHTARROW;
    if (sdl_key == SDLK_BACKSPACE) return KEY_BACKSPACE;
    if (sdl_key == SDLK_p) return KEY_COMMAND_PALETTE;
    if (sdl_key == SDLK_d) return KEY_D;
    if (sdl_key == SDLK_c) return KEY_C;
    if (sdl_key == SDLK_u) return KEY_U;
    if (sdl_key == SDLK_h) return KEY_H;
    if (sdl_key == SDLK_j) return KEY_J;
    if (sdl_key == SDLK_k) return KEY_K;
    if (sdl_key == SDLK_l) return KEY_L;
    if (sdl_key == SDLK_i) return KEY_I;
    if (sdl_key == SDLK_a) return KEY_A;
    if (sdl_key == SDLK_e) return KEY_E;
    if (sdl_key == SDLK_TAB) {
        return KEY_TAB;
    }
    return 0;
}

static int text_width(mu_Font font, const char *text, int len) {
    if (len == -1) {
        len = strlen(text);
    }
    return r_get_text_width(text, len);
}

static int text_height(mu_Font font) { return r_get_text_height(); }

struct TaskManager {
    int query_sequence_number = 0;
    // pairs of trie nodes, and how much of the query length they match.
    std::stack<const TrieNode *> query_walk_stack;
};

// void task_manager_explore_directory_timeslice(TaskManager* s,
//                                               BottomlineState* bot) {
//     assert(s->indexing);
//     if (s->ix_it == std::filesystem::end(s->ix_it)) {
//         bot->info = "DONE indexing;";
//         bot->info += "#nodes: " +
//         std::to_string(TrieNode::NUM_TRIE_NODES); bot->info += " #edges:
//         " + std::to_string(TrieEdge::NUM_TRIE_EDGES); s->indexing =
//         false; return;
//     };
//
//     const std::filesystem::path curp = *s->ix_it;
//     if ((curp.string().find(".git") != std::string::npos) ||
//         (curp.string().find("test") != std::string::npos)) {
//         s->ix_it++;
//         return;
//     }
//
//     s->ix_it++;  // next file.
//     bot->info = "walking: " + curp.string();
//     if (!std::filesystem::is_regular_file(curp)) {
//         return;
//     }
//
//     assert(std::filesystem::is_regular_file(curp));
//     FILE* fp = fopen(curp.c_str(), "rb");
//     assert(fp && "unable to open file");
//
//     fseek(fp, 0, SEEK_END);
//     const int total_size = ftell(fp);
//
//     s->index_loc = Loc();
//     s->index_loc->file = new File(curp.string(), total_size);
//     s->index_loc->file->buf = new char[total_size];
//     s->index_loc->ix = 0;
//     s->index_loc->line = 0;
//     s->index_loc->col = 0;
//
//     fseek(fp, 0, SEEK_SET);
//     const int nread = fread(s->index_loc->file->buf, 1, total_size, fp);
//     fclose(fp);
//     assert(nread == total_size && "unable to read file");
//     bot->info = "ix: " + curp.string() + " | 0%";
// }
//
// void task_manager_index_file_timeslice(TaskManager* s, BottomlineState*
// bot,
//                                        TrieNode* g_index) {
//     assert(s->index_loc);
//     assert(s->index_loc->valid());
//
//     while (!s->index_loc->eof() && is_whitespace(s->index_loc->get())) {
//         s->index_loc = s->index_loc->advance();
//     }
//
//     assert(s->index_loc->valid());
//     if (s->index_loc->eof()) {
//         bot->info = "DONE indexing " + s->index_loc->file->path;
//         s->index_loc = {};
//         return;
//     }
//     assert(!is_whitespace(s->index_loc->get()));
//
//     Loc eol = *s->index_loc;
//     static const int MAX_SUFFIX_LEN = 80;
//     static const int MAX_NGRAMS = 3;
//     int ngrams = 0;
//     while (!eol.eof() && !is_newline(eol.get()) && ngrams < MAX_NGRAMS &&
//            (eol.ix - s->index_loc->ix) <= MAX_SUFFIX_LEN) {
//         if (is_whitespace(eol.get())) {
//             ngrams++;
//         }
//         eol = eol.advance();
//     }
//
//     for (Loc sufloc = *s->index_loc; sufloc.ix < eol.ix;
//          sufloc = sufloc.advance()) {
//         const int len = eol.ix - sufloc.ix;
//         // TODO: this seems stupid, what additional data does sufloc even
//         // provide? (line, col) info? the edge seems to contain most of
//         the
//         // info?
//         index_add(g_index, sufloc.file, sufloc.ix, len, sufloc);
//         assert(!sufloc.eof());
//     }
//
//     const float percent =
//         100.0 * ((float)s->index_loc->ix / s->index_loc->file->len);
//     bot->info = "ix: ";
//     bot->info += s->index_loc->file->path;
//     bot->info += " | ";
//     bot->info += std::to_string(percent);
//     bot->info += " | ";
//     for (int i = s->index_loc->ix; i < eol.ix; ++i) {
//         bot->info += s->index_loc->file->buf[i];
//     }
//
//     s->index_loc = eol;
// }

// TODO: I need some way to express that TaskManager is only alowed to
// insert into pal->matches. must be monotonic.
void task_manager_query_timeslice(TaskManager *s, CommandPaletteState *pal,
                                  TrieNode *g_index) {
    assert(s->query_sequence_number <= pal->sequence_number);
    if (s->query_sequence_number < pal->sequence_number) {
        s->query_sequence_number = pal->sequence_number;
        s->query_walk_stack = std::stack<const TrieNode *>();

        if (pal->input.size() == 0) {
            return;
        }

        const TrieNode *cur =
            index_lookup(g_index, pal->input.c_str(), pal->input.size());
        if (!cur) {
            return;
        }
        // we need to explore the full subtree under `cur`.
        s->query_walk_stack.push(cur);
    }

    if (s->query_walk_stack.empty()) {
        return;
    }

    const TrieNode *top = s->query_walk_stack.top();
    s->query_walk_stack.pop();
    pal->matches.insert(pal->matches.end(), top->data.begin(), top->data.end());
    for (auto it : top->adj) {
        s->query_walk_stack.push(it.second.node);
    }
}

void task_manager_run_timeslice(TaskManager *s, CommandPaletteState *pal,
                                BottomlineState *bot, TrieNode *g_index) {
    // if (s->indexing) {
    //     if (!s->index_loc) {
    //         task_manager_explore_directory_timeslice(s, bot);
    //     } else {
    //         task_manager_index_file_timeslice(s, bot, g_index);
    //     }
    // }
    // task_manager_query_timeslice(s, pal, g_index);
}

// ===RENDERER===
#define BUFFER_SIZE 16384

static GLfloat tex_buf[BUFFER_SIZE * 8];
static GLfloat vert_buf[BUFFER_SIZE * 8];
static GLubyte color_buf[BUFFER_SIZE * 16];
static GLuint index_buf[BUFFER_SIZE * 6];

static int buf_idx;  // WTF?

SDL_Window *window;

extern "C" {
extern const int ATLAS_WIDTH;
extern const int ATLAS_HEIGHT;
extern const int ATLAS_WHITE;
extern const int ATLAS_FONT;
extern const int atlas_text_width;
extern const int atlas_text_height;
extern unsigned char atlas_texture[];
extern mu_Rect atlas[];
}

void r_init(void) {
    SDL_DisplayMode DM;
    SDL_GetCurrentDisplayMode(0, &DM);
    width = DM.w;
    height = DM.h;

    assert(width >= 0);
    assert(height >= 0);

    /* init SDL window */
    window =
        SDL_CreateWindow(NULL, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                         width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GL_CreateContext(window);

    /* init gl */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    /* init font texture */
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    /*
    // vvvNEW CODE
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    assert(glGetError() == 0);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, ATLAS_WIDTH, ATLAS_HEIGHT, 0,
    GL_ALPHA, GL_UNSIGNED_BYTE, 0);

    assert(glGetError() == 0);

    for (int i = 32; i < 127; ++i) {
        //
    https://en.wikibooks.org/wiki/OpenGL_Programming/Modern_OpenGL_Tutorial_Text_Rendering_02
        mu_Rect r = atlas[ATLAS_FONT + i];
            glTexSubImage2D(GL_TEXTURE_2D, 0,
            r.x, r.y,
            r.w, r.h,
            GL_ALPHA, GL_UNSIGNED_BYTE,
            atlas_offsets[ATLAS_FONT + i] + atlas_texture);
           assert(glGetError() == 0);
    }
    */
    // ^^^^^^END^^^^^^^^
    // vvvvvvOLD CODEvvv
    // /* new addition to old code: */ glPixelStorei(GL_UNPACK_ALIGNMENT,
    // 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, ATLAS_WIDTH, ATLAS_HEIGHT, 0,
                 GL_ALPHA, GL_UNSIGNED_BYTE, atlas_texture);
    // ^^^^
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    assert(glGetError() == 0);
}

static void flush(void) {
    if (buf_idx == 0) {
        return;
    }

    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0f, width, height, 0.0f, -1.0f, +1.0f);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glTexCoordPointer(2, GL_FLOAT, 0, tex_buf);
    glVertexPointer(2, GL_FLOAT, 0, vert_buf);
    glColorPointer(4, GL_UNSIGNED_BYTE, 0, color_buf);
    glDrawElements(GL_TRIANGLES, buf_idx * 6, GL_UNSIGNED_INT, index_buf);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    buf_idx = 0;
}

static void push_quad(mu_Rect dst, mu_Rect src, mu_Color color) {
    if (buf_idx == BUFFER_SIZE) {
        flush();
    }

    int texvert_idx = buf_idx * 8;
    int color_idx = buf_idx * 16;
    int element_idx = buf_idx * 4;
    int index_idx = buf_idx * 6;
    buf_idx++;

    /* update texture buffer */
    float x = src.x / (float)ATLAS_WIDTH;
    float y = src.y / (float)ATLAS_HEIGHT;
    float w = src.w / (float)ATLAS_WIDTH;
    float h = src.h / (float)ATLAS_HEIGHT;
    tex_buf[texvert_idx + 0] = x;
    tex_buf[texvert_idx + 1] = y;
    tex_buf[texvert_idx + 2] = x + w;
    tex_buf[texvert_idx + 3] = y;
    tex_buf[texvert_idx + 4] = x;
    tex_buf[texvert_idx + 5] = y + h;
    tex_buf[texvert_idx + 6] = x + w;
    tex_buf[texvert_idx + 7] = y + h;

    /* update vertex buffer */
    vert_buf[texvert_idx + 0] = dst.x;
    vert_buf[texvert_idx + 1] = dst.y;
    vert_buf[texvert_idx + 2] = dst.x + dst.w;
    vert_buf[texvert_idx + 3] = dst.y;
    vert_buf[texvert_idx + 4] = dst.x;
    vert_buf[texvert_idx + 5] = dst.y + dst.h;
    vert_buf[texvert_idx + 6] = dst.x + dst.w;
    vert_buf[texvert_idx + 7] = dst.y + dst.h;

    /* update color buffer */
    memcpy(color_buf + color_idx + 0, &color, 4);
    memcpy(color_buf + color_idx + 4, &color, 4);
    memcpy(color_buf + color_idx + 8, &color, 4);
    memcpy(color_buf + color_idx + 12, &color, 4);

    /* update index buffer */
    index_buf[index_idx + 0] = element_idx + 0;
    index_buf[index_idx + 1] = element_idx + 1;
    index_buf[index_idx + 2] = element_idx + 2;
    index_buf[index_idx + 3] = element_idx + 2;
    index_buf[index_idx + 4] = element_idx + 3;
    index_buf[index_idx + 5] = element_idx + 1;
}

void r_draw_rect(mu_Rect rect, mu_Color color) {
    push_quad(rect, atlas[ATLAS_WHITE], color);
}

void r_draw_text(const char *text, mu_Vec2 pos, mu_Color color) {
    mu_Rect dst = {pos.x, pos.y, 0, 0};
    for (const char *p = text; *p; p++) {
        if ((*p & 0xc0) == 0x80) {
            continue;
        }
        int chr = mu_min((unsigned char)*p, 127);
        mu_Rect src = atlas[ATLAS_FONT + chr];
        dst.w = src.w;
        dst.h = src.h;
        push_quad(dst, src, color);
        dst.x += dst.w;
    }
}

void r_draw_icon(int id, mu_Rect rect, mu_Color color) {
    mu_Rect src = atlas[id];
    int x = rect.x + (rect.w - src.w) / 2;
    int y = rect.y + (rect.h - src.h) / 2;
    push_quad(mu_rect(x, y, src.w, src.h), src, color);
}

int r_get_text_width(const char *text, int len) {
    int res = 0;
    for (const char *p = text; *p && len--; p++) {
        if ((*p & 0xc0) == 0x80) {
            continue;
        }
        int chr = mu_min((unsigned char)*p, 127);
        res += atlas[ATLAS_FONT + chr].w;
    }
    return res;
}

int r_get_text_height(void) { return atlas_text_height; }

void r_set_clip_rect(mu_Rect rect) {
    flush();
    glScissor(rect.x, height - (rect.y + rect.h), rect.w, rect.h);
}

void r_clear(mu_Color clr) {
    flush();
    glClearColor(clr.r / 255., clr.g / 255., clr.b / 255., clr.a / 255.);
    glClear(GL_COLOR_BUFFER_BIT);
}

void r_present(void) {
    flush();
    SDL_GL_SwapWindow(window);
}

// === MAIN====
// TODO: add open/save/load.

int main(int argc, char **argv) {
    setlocale(LC_ALL, "");

    // TODO: figure out the grammar.
    TSParser *parser = ts_parser_new();

    TaskManager g_task_manager;

    TrieNode g_index;

    BottomlineState g_bottom_line_state;
    g_bottom_line_state.info = "WELCOME";

    CommandPaletteState g_command_palette_state;
    EditorState *g_editor_state = new EditorState();

    FocusState g_focus_state = FocusState::FSK_Palette;

    EventState g_event_state;

    SDL_Init(SDL_INIT_EVERYTHING);
    r_init();

    /* init microui */
    mu_Context *ctx = new mu_Context;
    mu_init(ctx, text_width, text_height);
    ctx->text_width = text_width;
    ctx->text_height = text_height;

    /* main loop */
    for (;;) {
        /* handle SDL events */
        g_event_state.start_frame();
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_WINDOWEVENT: 
                    {
                        if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
                            width = e.window.data1;
                            height = e.window.data2;
                            printf("window resized. width: %d | height: %d\n", width, height);
                            fflush(stdout);
                        }
                    }
                    break;
                case SDL_QUIT:
                    exit(0);
                    break;
                case SDL_TEXTINPUT:
                    g_event_state.set_input_text(e.text.text);
                    break;
                    // case SDL_TEXTEDITING: mu_input_text(ctx,
                    // e.edit.text); break;

                case SDL_KEYDOWN:
                case SDL_KEYUP: {
                    // vv bollu: I don't know why this was masked.
                    // int c = key_map(e.key.keysym.sym & 0xff);
                    int c = key_map(e.key.keysym.sym);
                    if (c && e.type == SDL_KEYDOWN) {
                        g_event_state.set_keydown(c);
                    }
                    if (c && e.type == SDL_KEYUP) {
                        g_event_state.set_keyup(c);
                    }
                    break;
                }
            }
        }

        // Handle tasks.
        const clock_t clock_begin = clock();
        do {
            task_manager_run_timeslice(&g_task_manager,
                                       &g_command_palette_state,
                                       &g_bottom_line_state, &g_index);
        } while (clock() - clock_begin < TARGET_CLOCKS_PER_FRAME * 0.1);

        /* process frame */
        mu_finalize_events_begin_draw(ctx);
        editor_window(ctx, &g_event_state, g_editor_state, &g_bottom_line_state,
                      &g_focus_state, &g_command_palette_state);
        // mu_command_palette(ctx, &g_event_state, &g_editor_state,
        // &g_command_palette_state,
        //                    &g_focus_state);
        mu_end(ctx);

        /* render */
        static float bg[3] = {0, 0, 0};
        r_clear(mu_color(bg[0], bg[1], bg[2], 255));
        mu_Command *cmd = NULL;
        while (mu_next_command(ctx, &cmd)) {
            switch (cmd->type) {
                case MU_COMMAND_TEXT:
                    r_draw_text(cmd->text.str, cmd->text.pos, cmd->text.color);
                    break;
                case MU_COMMAND_RECT:
                    r_draw_rect(cmd->rect.rect, cmd->rect.color);
                    break;
                case MU_COMMAND_ICON:
                    r_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color);
                    break;
                case MU_COMMAND_CLIP:
                    r_set_clip_rect(cmd->clip.rect);
                    break;
            }
        }
        r_present();
    }

    return 0;
}
