#include <graphics/drawing.h>
#include <graphics/color.h>
#include <graphics/text.h>
#include <graphics/lcdc.h>
#include <sh4a/input/keypad.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <syscalls/syscalls.h>
#include <libct/print.h>
#include <libct/fsc/fs-control.h>
#include <libct/input.h>
#include <libct/ui/dialog/user_input_dialog.h>
#include <libct/ui/dialog/user_select_dialog.h>
#include <libct/ui/dialog/memo_editor.h>
#include <version_generated.h>

#define SCREEN_WIDTH 528
#define SCREEN_HEIGHT 320
#define MAX_DISPLAY 12          // 画面に表示する最大メモ数
#define MEMO_DIR "MEMO"         // メモを保存するフォルダ名
#define MEMO_READ_BUF 2048      // メモ本文読み込み用バッファサイズ

struct file_list_result current_files = {0};
int scroll_offset = 0;
int selected_index = 0;
int prev_selected_index = 0;
int refresh_needed = 1;
static int current_drive = 0; // 0: 内蔵ドライブ, 1: SDカード
static char *drive[2] = {
    "\\\\drv0\\",  // 内蔵ドライブ
    "\\\\crd0\\"   // SDカード
};

// 現在のドライブ上のメモフォルダのパス ("\\drv0\MEMO\" 形式) を作る
static void build_memo_dir(char *out, size_t outsize) {
    sprintf(out, "%s%s\\", drive[current_drive], MEMO_DIR);
    (void)outsize;
}

// メモフォルダの検索パス ("\\drv0\MEMO\*" 形式) を作る
static void build_memo_search(char *out, size_t outsize) {
    sprintf(out, "%s%s\\*", drive[current_drive], MEMO_DIR);
    (void)outsize;
}

// 選択されたメモのファイル名を取得
char *get_selected_filename(void) {
    if (selected_index >= 0 && selected_index < current_files.count) {
        return current_files.entries[selected_index].name;
    }
    return NULL;
}

// 選択されたメモのフルパスを作成して返す（呼び出し側で解放すること）
char *get_selected_fullpath(void) {
    if (selected_index >= 0 && selected_index < current_files.count) {
        char dir[80];
        build_memo_dir(dir, sizeof(dir));
        const char *name = current_files.entries[selected_index].name;
        size_t need = strlen(dir) + strlen(name) + 1;
        char *buf = memmgr_alloc(need);
        if (!buf) return NULL;
        strcpy(buf, dir);
        strcat(buf, name);
        return buf;
    }
    return NULL;
}

// メモ一覧を表示するヘルパー関数
void display_file_list(int start_index, struct font *fnt, char *display_name) {
    ct_screen_rect(0, 30, SCREEN_WIDTH, MAX_DISPLAY * (fnt->height + 2), create_rgb16(0, 0, 0));

    for (int i = 0; i < MAX_DISPLAY && (start_index + i) < current_files.count; i++) {
        set_pen(create_rgb16(255, 255, 255));
        int idx = start_index + i;
        sprintf(display_name, "[MEMO] %s", current_files.entries[idx].name);
        render_text(10, 30 + i * (fnt->height + 2), display_name);
    }
}

// メモ本文をファイルから読み込む
static int memo_read(const char *path, char *out, int out_size) {
    int fd = sys_open(path, FILE_RD);
    if (fd < 0) {
        out[0] = '\0';
        return -1;
    }
    int size = sys_get_filesize(fd);
    if (size < 0) size = 0;
    if (size > out_size - 1) size = out_size - 1;

    int total = 0;
    while (total < size) {
        int r = sys_read(fd, out + total, size - total);
        if (r <= 0) break;
        total += r;
    }
    out[total] = '\0';
    sys_close(fd);
    return 0;
}

// メモ本文をファイルへ書き込む（既存があれば作り直す）
static int memo_write(const char *path, const char *text) {
    sys_delete(path); // 既存ファイルがあれば削除（無ければ失敗するが無視してよい）
    int c = sys_create(path, 1);
    if (c < 0) return c;

    int fd = sys_open(path, FILE_WR);
    if (fd < 0) return fd;

    int len = (int)strlen(text);
    int written = 0;
    while (written < len) {
        int w = sys_write(fd, text + written, len - written);
        if (w <= 0) {
            sys_close(fd);
            return -1;
        }
        written += w;
    }
    sys_close(fd);
    return 0;
}

// メモ一覧を再読み込みする
static void reload_memo_list(void) {
    char search[96];
    build_memo_search(search, sizeof(search));

    if (current_files.entries) {
        memmgr_free(current_files.entries);
        current_files.entries = NULL;
    }
    current_files = get_file_list(search);
    selected_index = 0;
    prev_selected_index = 0;
    scroll_offset = 0;
}

// 画面全体を描画する
static void redraw_screen(struct font *fnt, char *display_name) {
    ct_screen_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, create_rgb16(0, 0, 0));

    ct_print(10, 10, "=== Memo List ===", create_rgb16(255, 255, 0));

    sprintf(display_name, "[%s]", current_drive == 0 ? "Internal" : "SD Card");
    ct_print(SCREEN_WIDTH - (int)strlen(display_name) * fnt->width - 10, 10, display_name, create_rgb16(0, 255, 255));

    if (current_files.count > 0) {
        display_file_list(0, fnt, display_name);
        sprintf(display_name, "Total: %d memos", current_files.count);
        ct_print(10, 30 + MAX_DISPLAY * (fnt->height + 2) + 10, display_name, create_rgb16(0, 255, 255));
        ct_print(0, 30, ">", create_rgb16(0, 0, 255));
    } else {
        ct_print(10, 30, "No memos yet. Press RIGHT to create one.", create_rgb16(255, 0, 0));
    }

    ct_print((SCREEN_WIDTH - (int)strlen("ENTER:Open RIGHT:New BKSP:Del LEFT:Drive H:Help") * fnt->width) / 2,
        SCREEN_HEIGHT - fnt->height - 10,
        "ENTER:Open RIGHT:New BKSP:Del LEFT:Drive H:Help",
        create_rgb16(0, 255, 0));

    lcdc_copy_vram();
}

int main(void) {
    memmgr_init();

    // メモ保存用フォルダが無ければ作成しておく（既に存在する場合のエラーは無視してよい）
    directory_create(drive[current_drive], MEMO_DIR);

    struct font *fnt = get_font();
    char display_name[100];

    reload_memo_list();
    redraw_screen(fnt, display_name);

    // 入力待機ループ
    while (1) {
        keypad_read();

        if (get_key_state(KEY_POWER)) { // プログラム終了
            if (current_files.entries) memmgr_free(current_files.entries);
            return -2;
        }

        if (get_key_state(KEY_BACK)) { // 終了（一覧画面がトップのため）
            if (current_files.entries) memmgr_free(current_files.entries);
            return 0;
        }

        if (get_key_state(KEY_ENTER)) { // 選択したメモを開いて編集
            while (get_key_state(KEY_ENTER)) keypad_read();

            char *sel_name = get_selected_filename();
            if (sel_name != NULL) {
                char *fullpath = get_selected_fullpath();
                if (fullpath != NULL) {
                    static char text_buf[MEMO_READ_BUF];
                    memo_read(fullpath, text_buf, sizeof(text_buf));

                    char *result = memo_editor(text_buf);
                    if (result != NULL) {
                        int wr = memo_write(fullpath, result);
                        if (wr != 0) {
                            popup_dialog("Failed to save memo.", create_rgb16(255, 0, 0));
                        }
                    }
                    memmgr_free(fullpath);
                }
            }
            reload_memo_list();
            redraw_screen(fnt, display_name);
            return 0;
        }

        if (get_key_state(KEY_RIGHT)) { // 新規メモ作成
            while (get_key_state(KEY_RIGHT)) keypad_read();

            char *name = user_input_dialog();
            if (name == NULL || name[0] == '\0') {
                redraw_screen(fnt, display_name);
                return 0;
            }

            char safe_name[80];
            strncpy(safe_name, name, sizeof(safe_name) - 5);
            safe_name[sizeof(safe_name) - 5] = '\0';
            if (strchr(safe_name, '.') == NULL) {
                strcat(safe_name, ".TXT");
            }

            char *body = memo_editor("");
            if (body != NULL) {
                char dir[80];
                build_memo_dir(dir, sizeof(dir));
                char fullpath[128];
                sprintf(fullpath, "%s%s", dir, safe_name);

                int wr = memo_write(fullpath, body);
                if (wr != 0) {
                    popup_dialog("Failed to create memo.", create_rgb16(255, 0, 0));
                }
            }
            reload_memo_list();
            redraw_screen(fnt, display_name);
            return 0;
        }

        if (get_key_state(KEY_BACKSPACE)) { // 選択したメモを削除
            while (get_key_state(KEY_BACKSPACE)) keypad_read();

            char *sel_name = get_selected_filename();
            if (sel_name == NULL) {
                popup_dialog("No memo selected!", create_rgb16(255, 0, 0));
                reload_memo_list();
                redraw_screen(fnt, display_name);
                return 0;
            }

            char *fullpath = get_selected_fullpath();
            if (fullpath != NULL) {
                popup_dialog(fullpath, create_rgb16(255, 255, 0));
                int yn = yes_or_no_dialog("Delete this memo?", create_rgb16(255, 0, 0));
                if (yn == 0) {
                    int dr = sys_delete(fullpath);
                    if (dr == 0) {
                        popup_dialog("Memo deleted.", create_rgb16(0, 255, 0));
                    } else {
                        popup_dialog("Failed to delete memo.", create_rgb16(255, 0, 0));
                    }
                }
                memmgr_free(fullpath);
            }
            reload_memo_list();
            redraw_screen(fnt, display_name);
            return 0;
        }

        if (get_key_state(KEY_LEFT)) { // 内蔵ドライブ/SDカード切り替え
            while (get_key_state(KEY_LEFT)) keypad_read();

            current_drive = (current_drive == 0) ? 1 : 0;
            directory_create(drive[current_drive], MEMO_DIR); // フォルダが無ければ作成
            reload_memo_list();
            redraw_screen(fnt, display_name);
            return 0;
        }

        if (get_key_state(KEY_CHAR_H)) { // ヘルプ表示
            while (get_key_state(KEY_CHAR_H)) keypad_read();
            const char *items[] = {
                "=== Help ===",
                "[UP/DOWN] Move Cursor",
                "[ENTER] Open / Edit Memo",
                "[RIGHT] Create New Memo",
                "[BACKSPACE] Delete Selected Memo",
                "[LEFT] Switch Internal/SD Card",
                "[BACK] Exit App",
                "[V] Version Info",
                "-- In Editor --",
                "[A-Z] Type   [SYMBOL] Space",
                "[SHIFT] Toggle EN / Kana input",
                "[Kana] Type romaji (KA->ka SHI->shi etc.)",
                "[ENTER] New Line   [F1] Save",
                "[BACK] Cancel(confirm) [POWER] Save&Exit",
            };
            info_list(items, 14);
            refresh_needed = 1;
            redraw_screen(fnt, display_name);
            return 0;
        }

        if (get_key_state(KEY_CHAR_V)) { // バージョン情報表示
            while (get_key_state(KEY_CHAR_V)) keypad_read();
            const char *items[] = {
                "=== Version Info ===",
                "Memo App",
                "Version:",
                BUILD_VERSION,
                "Git Hash:",
                BUILD_GIT_HASH,
            };
            info_list(items, 6);
            refresh_needed = 1;
            redraw_screen(fnt, display_name);
            return 0;
        }

        if (get_key_state(KEY_UP)) { // 上キーで選択を移動
            if (selected_index > 0) {
                prev_selected_index = selected_index;
                selected_index--;

                if (selected_index < scroll_offset) {
                    scroll_offset = selected_index;
                    display_file_list(scroll_offset, fnt, display_name);
                }

                refresh_needed = 1;
                while (get_key_state(KEY_UP)) keypad_read();
            }
        }

        if (get_key_state(KEY_DOWN)) { // 下キーで選択を移動
            if (selected_index < current_files.count - 1) {
                prev_selected_index = selected_index;
                selected_index++;

                if (selected_index >= scroll_offset + MAX_DISPLAY) {
                    scroll_offset = selected_index - MAX_DISPLAY + 1;
                    display_file_list(scroll_offset, fnt, display_name);
                }

                refresh_needed = 1;
                while (get_key_state(KEY_DOWN)) keypad_read();
            }
        }

        if (refresh_needed) { // カーソル位置などを更新
            int prev_screen_pos = prev_selected_index - scroll_offset;
            if (prev_screen_pos >= 0 && prev_screen_pos < MAX_DISPLAY) {
                ct_screen_rect(0, 30 + prev_screen_pos * (fnt->height + 2), 10, fnt->height + 2, create_rgb16(0, 0, 0));
            }

            int screen_pos = selected_index - scroll_offset;
            if (screen_pos >= 0 && screen_pos < MAX_DISPLAY) {
                set_pen(create_rgb16(0, 0, 255));
                render_text(0, 30 + screen_pos * (fnt->height + 2), ">");
            }

            ct_screen_rect(400, 10, 120, 20, create_rgb16(0, 0, 0));
            set_pen(create_rgb16(255, 255, 255));
            sprintf(display_name, "Sel: %d/%d", selected_index + 1, current_files.count);
            render_text(400, 10, display_name);

            lcdc_copy_vram();
            refresh_needed = 0;
        }
    }
}
