#include "memo_editor.h"
#include "user_input_dialog.h"
#include <libct/print.h>
#include <libct/input.h>
#include <libct/fonts/jpn-font.h>
#include <graphics/drawing.h>
#include <graphics/color.h>
#include <graphics/text.h>
#include <graphics/lcdc.h>
#include <sh4a/input/keypad.h>
#include <string.h>
#include <stdio.h>

#define SCREEN_WIDTH 528
#define SCREEN_HEIGHT 320
#define MEMO_BUF_SIZE 2048
#define MEMO_MAX_LINES 64
#define MEMO_LINE_LEN 96
#define MEMO_HINT "F1:Save BACK:Cancel ENTER:NewLine SHIFT:EN/Kana"

/* buf にはASCII文字に加え、半角カタカナのバイト(0xA1-0xDF)も
 * そのまま1バイト文字として混在して格納する。
 * 表示には render_text_jpn_halfwidth() を使うことで両方を描画できる。 */
static char buf[MEMO_BUF_SIZE];
static char line_store[MEMO_MAX_LINES][MEMO_LINE_LEN];

/* --- ローマ字 -> 半角カタカナ 変換テーブル --- */
struct roma_entry {
    const char *roma;
    uint8_t out[3];
    uint8_t out_len;
};

static const struct roma_entry roma_table[] = {
    /* 母音 */
    {"A", {0xB1}, 1}, {"I", {0xB2}, 1}, {"U", {0xB3}, 1}, {"E", {0xB4}, 1}, {"O", {0xB5}, 1},
    /* K */
    {"KA", {0xB6}, 1}, {"KI", {0xB7}, 1}, {"KU", {0xB8}, 1}, {"KE", {0xB9}, 1}, {"KO", {0xBA}, 1},
    /* G(濁音) */
    {"GA", {0xB6, 0xDE}, 2}, {"GI", {0xB7, 0xDE}, 2}, {"GU", {0xB8, 0xDE}, 2}, {"GE", {0xB9, 0xDE}, 2}, {"GO", {0xBA, 0xDE}, 2},
    /* S */
    {"SA", {0xBB}, 1}, {"SI", {0xBC}, 1}, {"SHI", {0xBC}, 1}, {"SU", {0xBD}, 1}, {"SE", {0xBE}, 1}, {"SO", {0xBF}, 1},
    /* Z */
    {"ZA", {0xBB, 0xDE}, 2}, {"ZI", {0xBC, 0xDE}, 2}, {"JI", {0xBC, 0xDE}, 2}, {"ZU", {0xBD, 0xDE}, 2}, {"ZE", {0xBE, 0xDE}, 2}, {"ZO", {0xBF, 0xDE}, 2},
    /* T */
    {"TA", {0xC0}, 1}, {"TI", {0xC1}, 1}, {"CHI", {0xC1}, 1}, {"TU", {0xC2}, 1}, {"TSU", {0xC2}, 1}, {"TE", {0xC3}, 1}, {"TO", {0xC4}, 1},
    /* D */
    {"DA", {0xC0, 0xDE}, 2}, {"DI", {0xC1, 0xDE}, 2}, {"DU", {0xC2, 0xDE}, 2}, {"DE", {0xC3, 0xDE}, 2}, {"DO", {0xC4, 0xDE}, 2},
    /* N */
    {"NA", {0xC5}, 1}, {"NI", {0xC6}, 1}, {"NU", {0xC7}, 1}, {"NE", {0xC8}, 1}, {"NO", {0xC9}, 1},
    {"NN", {0xDD}, 1},
    /* H */
    {"HA", {0xCA}, 1}, {"HI", {0xCB}, 1}, {"HU", {0xCC}, 1}, {"FU", {0xCC}, 1}, {"HE", {0xCD}, 1}, {"HO", {0xCE}, 1},
    /* B(濁音) */
    {"BA", {0xCA, 0xDE}, 2}, {"BI", {0xCB, 0xDE}, 2}, {"BU", {0xCC, 0xDE}, 2}, {"BE", {0xCD, 0xDE}, 2}, {"BO", {0xCE, 0xDE}, 2},
    /* P(半濁音) */
    {"PA", {0xCA, 0xDF}, 2}, {"PI", {0xCB, 0xDF}, 2}, {"PU", {0xCC, 0xDF}, 2}, {"PE", {0xCD, 0xDF}, 2}, {"PO", {0xCE, 0xDF}, 2},
    /* M */
    {"MA", {0xCF}, 1}, {"MI", {0xD0}, 1}, {"MU", {0xD1}, 1}, {"ME", {0xD2}, 1}, {"MO", {0xD3}, 1},
    /* Y */
    {"YA", {0xD4}, 1}, {"YU", {0xD5}, 1}, {"YO", {0xD6}, 1},
    /* R */
    {"RA", {0xD7}, 1}, {"RI", {0xD8}, 1}, {"RU", {0xD9}, 1}, {"RE", {0xDA}, 1}, {"RO", {0xDB}, 1},
    /* W */
    {"WA", {0xDC}, 1}, {"WO", {0xA6}, 1},
    /* ヴ(V) */
    {"VU", {0xB3, 0xDE}, 2},
    /* 小文字(X接頭)・促音 */
    {"XA", {0xA7}, 1}, {"XI", {0xA8}, 1}, {"XU", {0xA9}, 1}, {"XE", {0xAA}, 1}, {"XO", {0xAB}, 1},
    {"XYA", {0xAC}, 1}, {"XYU", {0xAD}, 1}, {"XYO", {0xAE}, 1}, {"XTU", {0xAF}, 1},
    /* 長音符 */
    {"XX", {0xB0}, 1},
    /* 拗音(キャ行〜) */
    {"KYA", {0xB7, 0xAC}, 2}, {"KYU", {0xB7, 0xAD}, 2}, {"KYO", {0xB7, 0xAE}, 2},
    {"SYA", {0xBC, 0xAC}, 2}, {"SYU", {0xBC, 0xAD}, 2}, {"SYO", {0xBC, 0xAE}, 2},
    {"SHA", {0xBC, 0xAC}, 2}, {"SHU", {0xBC, 0xAD}, 2}, {"SHO", {0xBC, 0xAE}, 2},
    {"CYA", {0xC1, 0xAC}, 2}, {"CYU", {0xC1, 0xAD}, 2}, {"CYO", {0xC1, 0xAE}, 2},
    {"CHA", {0xC1, 0xAC}, 2}, {"CHU", {0xC1, 0xAD}, 2}, {"CHO", {0xC1, 0xAE}, 2},
    {"NYA", {0xC6, 0xAC}, 2}, {"NYU", {0xC6, 0xAD}, 2}, {"NYO", {0xC6, 0xAE}, 2},
    {"HYA", {0xCB, 0xAC}, 2}, {"HYU", {0xCB, 0xAD}, 2}, {"HYO", {0xCB, 0xAE}, 2},
    {"MYA", {0xD0, 0xAC}, 2}, {"MYU", {0xD0, 0xAD}, 2}, {"MYO", {0xD0, 0xAE}, 2},
    {"RYA", {0xD8, 0xAC}, 2}, {"RYU", {0xD8, 0xAD}, 2}, {"RYO", {0xD8, 0xAE}, 2},
    /* 拗音(濁音・半濁音) */
    {"GYA", {0xB7, 0xDE, 0xAC}, 3}, {"GYU", {0xB7, 0xDE, 0xAD}, 3}, {"GYO", {0xB7, 0xDE, 0xAE}, 3},
    {"ZYA", {0xBC, 0xDE, 0xAC}, 3}, {"ZYU", {0xBC, 0xDE, 0xAD}, 3}, {"ZYO", {0xBC, 0xDE, 0xAE}, 3},
    {"JA", {0xBC, 0xDE, 0xAC}, 3}, {"JU", {0xBC, 0xDE, 0xAD}, 3}, {"JO", {0xBC, 0xDE, 0xAE}, 3},
    {"BYA", {0xCB, 0xDE, 0xAC}, 3}, {"BYU", {0xCB, 0xDE, 0xAD}, 3}, {"BYO", {0xCB, 0xDE, 0xAE}, 3},
    {"PYA", {0xCB, 0xDF, 0xAC}, 3}, {"PYU", {0xCB, 0xDF, 0xAD}, 3}, {"PYO", {0xCB, 0xDF, 0xAE}, 3},
};
#define ROMA_TABLE_LEN (sizeof(roma_table) / sizeof(roma_table[0]))

#define ROMA_BUF_LEN 4
static char roma_buf[ROMA_BUF_LEN];
static int roma_len = 0;
static int kana_mode = 0; /* 0:英字入力 1:カナ入力(ローマ字) */

static int is_vowel(char c) {
    return c == 'A' || c == 'I' || c == 'U' || c == 'E' || c == 'O';
}

/* このプロジェクトのstring.hにはstrncmp/size_tが無いため自前で実装する */
static int str_is_prefix_of(const char *table_str, const char *s) {
    while (*s != '\0') {
        if (*table_str != *s) return 0;
        table_str++;
        s++;
    }
    return 1;
}

static const struct roma_entry *roma_exact(const char *s) {
    for (int i = 0; i < (int)ROMA_TABLE_LEN; ++i) {
        if (strcmp(roma_table[i].roma, s) == 0) return &roma_table[i];
    }
    return NULL;
}

static int roma_is_prefix(const char *s) {
    for (int i = 0; i < (int)ROMA_TABLE_LEN; ++i) {
        if (str_is_prefix_of(roma_table[i].roma, s)) return 1;
    }
    return 0;
}

/* buf の末尾に1バイト追加する（境界チェック込み） */
static void emit_byte(int *idx, uint8_t b) {
    if (*idx < MEMO_BUF_SIZE - 2) {
        buf[(*idx)++] = (char)b;
        buf[*idx] = '\0';
    }
}

/* pending中のromaji bufferを確定/破棄する。単独の "N" は ﾝ として確定する。 */
static void kana_flush(int *idx) {
    if (roma_len == 1 && roma_buf[0] == 'N') {
        emit_byte(idx, 0xDD);
    }
    roma_len = 0;
    roma_buf[0] = '\0';
}

/* ローマ字1文字を投入し、確定した分をbufへ書き込む */
static void kana_input(int *idx, char c) {
    /* 促音(っ): 子音の連続（N以外）で小さいツを自動挿入 */
    if (roma_len == 1 && roma_buf[0] == c && roma_buf[0] != 'N' && !is_vowel(c)) {
        emit_byte(idx, 0xAF);
        roma_len = 0;
        roma_buf[0] = '\0';
    }

    /* 単独の "N" の後に、N行として続けられない文字が来たら先に ﾝ を確定する */
    if (roma_len == 1 && roma_buf[0] == 'N') {
        char test[3] = { 'N', c, '\0' };
        if (!roma_exact(test) && !roma_is_prefix(test)) {
            emit_byte(idx, 0xDD);
            roma_len = 0;
            roma_buf[0] = '\0';
        }
    }

    if (roma_len < ROMA_BUF_LEN - 1) {
        roma_buf[roma_len++] = c;
        roma_buf[roma_len] = '\0';
    }

    const struct roma_entry *e = roma_exact(roma_buf);
    if (e != NULL) {
        for (int i = 0; i < e->out_len; ++i) emit_byte(idx, e->out[i]);
        roma_len = 0;
        roma_buf[0] = '\0';
        return;
    }

    if (!roma_is_prefix(roma_buf)) {
        /* どの候補にも当てはまらない: 直前までを破棄し、今の文字から再試行 */
        roma_buf[0] = c;
        roma_buf[1] = '\0';
        roma_len = 1;
        e = roma_exact(roma_buf);
        if (e != NULL) {
            for (int i = 0; i < e->out_len; ++i) emit_byte(idx, e->out[i]);
            roma_len = 0;
            roma_buf[0] = '\0';
        } else if (!roma_is_prefix(roma_buf)) {
            roma_len = 0;
            roma_buf[0] = '\0';
        }
    }
}

/* buf の内容を chars_per_line 文字で折り返して line_store に格納し、行数を返す */
static int wrap_lines(const char *text, int chars_per_line) {
    int line_count = 0;
    int col = 0;

    if (chars_per_line < 1) chars_per_line = 1;
    if (chars_per_line > MEMO_LINE_LEN - 1) chars_per_line = MEMO_LINE_LEN - 1;

    line_store[0][0] = '\0';

    for (const char *p = text; *p != '\0'; ++p) {
        if (*p == '\n') {
            line_store[line_count][col] = '\0';
            if (line_count < MEMO_MAX_LINES - 1) {
                line_count++;
                col = 0;
                line_store[line_count][0] = '\0';
            }
            continue;
        }
        if (col >= chars_per_line) {
            line_store[line_count][col] = '\0';
            if (line_count < MEMO_MAX_LINES - 1) {
                line_count++;
                col = 0;
                line_store[line_count][0] = '\0';
            } else {
                break;
            }
        }
        line_store[line_count][col++] = *p;
        line_store[line_count][col] = '\0';
    }
    line_count++;
    if (line_count > MEMO_MAX_LINES) line_count = MEMO_MAX_LINES;
    return line_count;
}

/* 編集領域を再描画する（末尾に簡易カーソルと入力中のローマ字を表示） */
static void redraw_editor(struct font *fnt, int cell_width, int chars_per_line, int visible_lines) {
    int total = wrap_lines(buf, chars_per_line);
    int start = (total > visible_lines) ? (total - visible_lines) : 0;
    int shown = total - start;

    ct_screen_rect(0, 30, SCREEN_WIDTH, SCREEN_HEIGHT - 30 - fnt->height - 30, create_rgb16(0, 0, 0));

    set_pen(create_rgb16(255, 255, 255));
    for (int i = 0; i < shown; ++i) {
        render_text_jpn_halfwidth(10, 30 + i * (fnt->height + 2), (const uint8_t *)line_store[start + i]);
    }

    int cursor_row = shown - 1;
    if (cursor_row < 0) cursor_row = 0;
    int cursor_len = (total > 0) ? (int)strlen(line_store[total - 1]) : 0;
    set_pen(create_rgb16(0, 255, 0));
    render_text(10 + cursor_len * cell_width, 30 + cursor_row * (fnt->height + 2), "_");

    /* モード表示 + 入力中のローマ字 */
    ct_screen_rect(0, SCREEN_HEIGHT - fnt->height - 26, SCREEN_WIDTH, fnt->height + 2, create_rgb16(0, 0, 0));
    char status[64];
    if (kana_mode) {
        sprintf(status, "[Kana] %s", roma_buf);
        ct_print(10, SCREEN_HEIGHT - fnt->height - 26, status, create_rgb16(0, 255, 255));
    } else {
        ct_print(10, SCREEN_HEIGHT - fnt->height - 26, "[EN]", create_rgb16(0, 255, 255));
    }

    lcdc_copy_vram();
}

static void draw_chrome(struct font *fnt) {
    ct_screen_clear(create_rgb16(0, 0, 0));
    ct_print(10, 10, "=== Memo Editor ===", create_rgb16(255, 255, 0));
    ct_print((SCREEN_WIDTH - (int)strlen(MEMO_HINT) * fnt->width) / 2,
              SCREEN_HEIGHT - fnt->height - 10,
              MEMO_HINT,
              create_rgb16(0, 255, 0));
}

char *memo_editor(const char *initial_text) {
    struct font *fnt = get_font();
    struct font *jfnt = get_jpn_font();

    /* 半角カナと英字が混在してもはみ出さないよう、広い方の幅を基準にする */
    int cell_width = (jfnt->width > fnt->width) ? jfnt->width : fnt->width;
    int chars_per_line = (SCREEN_WIDTH - 20) / cell_width;
    if (chars_per_line > MEMO_LINE_LEN - 1) chars_per_line = MEMO_LINE_LEN - 1;
    if (chars_per_line < 1) chars_per_line = 1;

    int visible_lines = (SCREEN_HEIGHT - 30 - fnt->height - 30) / (fnt->height + 2);
    if (visible_lines < 1) visible_lines = 1;

    int idx = 0;
    buf[0] = '\0';
    if (initial_text != NULL) {
        strncpy(buf, initial_text, MEMO_BUF_SIZE - 1);
        buf[MEMO_BUF_SIZE - 1] = '\0';
        idx = (int)strlen(buf);
    }
    kana_mode = 0;
    roma_len = 0;
    roma_buf[0] = '\0';

    draw_chrome(fnt);
    redraw_editor(fnt, cell_width, chars_per_line, visible_lines);

    while (1) {
        keypad_read();

        if (get_key_state(KEY_FUNC_1)) {
            while (get_key_state(KEY_FUNC_1)) keypad_read();
            kana_flush(&idx);
            return buf;
        }
        if (get_key_state(KEY_POWER)) {
            /* 電源キーはデータ消失防止のため保存して終了する */
            while (get_key_state(KEY_POWER)) keypad_read();
            kana_flush(&idx);
            return buf;
        }
        if (get_key_state(KEY_BACK)) {
            while (get_key_state(KEY_BACK)) keypad_read();
            int yn = yes_or_no_dialog("Discard changes and cancel?", create_rgb16(255, 0, 0));
            if (yn == 0) {
                return NULL; /* Yes: キャンセル確定 */
            }
            /* No: 編集画面へ復帰 */
            draw_chrome(fnt);
            redraw_editor(fnt, cell_width, chars_per_line, visible_lines);
            continue;
        }
        if (get_key_state(KEY_SHIFT)) {
            while (get_key_state(KEY_SHIFT)) keypad_read();
            kana_flush(&idx); /* モード切替時に入力中のローマ字は確定/破棄する */
            kana_mode = !kana_mode;
            redraw_editor(fnt, cell_width, chars_per_line, visible_lines);
            continue;
        }
        if (get_key_state(KEY_BACKSPACE)) {
            if (roma_len > 0) {
                /* 入力中のローマ字があれば、まずそちらを1文字消す */
                roma_len--;
                roma_buf[roma_len] = '\0';
            } else if (idx > 0) {
                buf[--idx] = '\0';
            }
            redraw_editor(fnt, cell_width, chars_per_line, visible_lines);
            while (get_key_state(KEY_BACKSPACE)) keypad_read();
            continue;
        }
        if (get_key_state(KEY_ENTER)) {
            kana_flush(&idx);
            if (idx < MEMO_BUF_SIZE - 2) {
                buf[idx++] = '\n';
                buf[idx] = '\0';
            }
            redraw_editor(fnt, cell_width, chars_per_line, visible_lines);
            while (get_key_state(KEY_ENTER)) keypad_read();
            continue;
        }
        if (get_key_state(KEY_SYMBOL)) {
            kana_flush(&idx);
            if (idx < MEMO_BUF_SIZE - 2) {
                buf[idx++] = ' ';
                buf[idx] = '\0';
            }
            redraw_editor(fnt, cell_width, chars_per_line, visible_lines);
            while (get_key_state(KEY_SYMBOL)) keypad_read();
            continue;
        }

        int keycode = ct_get_current_keycode();
        if (keycode == -1) continue;

        char ch = keycode_to_text(keycode);
        if (ch != 0) {
            if (kana_mode) {
                kana_input(&idx, ch);
            } else {
                emit_byte(&idx, (uint8_t)ch);
            }
            redraw_editor(fnt, cell_width, chars_per_line, visible_lines);
        }

        while (get_key_state(keycode)) keypad_read();
    }
}
