#define _GNU_SOURCE
#include <stdlib.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include "keysyms_min.h"
#include "snap.h"
#include "window.h"
#include "config.h"
#include "config_numlock.h"

static xcb_keycode_t kc_left  = 0;
static xcb_keycode_t kc_right = 0;
static xcb_keycode_t kc_up    = 0;
static xcb_keycode_t kc_down  = 0;
static xcb_keycode_t kc_tab   = 0;

/* Битовая маска стрелок (SNAP_LEFT/RIGHT/TOP/BOTTOM из window.h),
   зажатых вместе с Meta прямо сейчас. Обновляется по KeyPress/KeyRelease:
   вторая одновременно нажатая стрелка "уточняет" уже применённую
   половину экрана до четверти (см. snap_handle_key_press). */
static int held_mask = 0;
/* Клиент, к которому относится накопленный held_mask (баг №3). Если
   фокус сменился (например, через Alt+Tab) между двумя нажатиями
   стрелок, накопленная маска от предыдущего окна больше не имеет
   смысла для нового — раньше она применялась к новому фокусу целиком,
   и окно сразу прыгало в четверть экрана, минуя стадию "половина". */
static client_t *held_client = NULL;

/* Грабим комбинацию mods+keycode в 4 вариантах — NumLock/CapsLock не
   должны мешать срабатыванию, как и в altTab.c/config.c. */
static void grab_combo(xcb_connection_t *conn, xcb_screen_t *screen,
                        uint16_t mods, xcb_keycode_t keycode) {
    if (!keycode) return;
    uint16_t numlock = config_numlock_mask();
    uint16_t extra[] = {0, XCB_MOD_MASK_LOCK, numlock,
                         (uint16_t)(XCB_MOD_MASK_LOCK | numlock)};
    for (size_t i = 0; i < sizeof(extra) / sizeof(extra[0]); i++) {
        xcb_grab_key(conn, 1, screen->root, (uint16_t)(mods | extra[i]), keycode,
                     XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    }
}

void snap_init(xcb_connection_t *conn, xcb_screen_t *screen) {
    xcb_key_symbols_t *keysyms = xcb_key_symbols_alloc(conn);
    if (!keysyms) return;

    xcb_keycode_t *k;
    k = xcb_key_symbols_get_keycode(keysyms, XK_Left);  if (k) { kc_left  = k[0]; free(k); }
    k = xcb_key_symbols_get_keycode(keysyms, XK_Right); if (k) { kc_right = k[0]; free(k); }
    k = xcb_key_symbols_get_keycode(keysyms, XK_Up);    if (k) { kc_up    = k[0]; free(k); }
    k = xcb_key_symbols_get_keycode(keysyms, XK_Down);  if (k) { kc_down  = k[0]; free(k); }
    k = xcb_key_symbols_get_keycode(keysyms, XK_Tab);   if (k) { kc_tab   = k[0]; free(k); }

    xcb_key_symbols_free(keysyms);

    grab_combo(conn, screen, XCB_MOD_MASK_4, kc_left);
    grab_combo(conn, screen, XCB_MOD_MASK_4, kc_right);
    grab_combo(conn, screen, XCB_MOD_MASK_4, kc_up);
    grab_combo(conn, screen, XCB_MOD_MASK_4, kc_down);
    grab_combo(conn, screen, XCB_MOD_MASK_4, kc_tab);       /* Meta+Tab — свернуть всё */
    grab_combo(conn, screen, XCB_MOD_MASK_CONTROL, kc_tab); /* Ctrl+Tab — открепить */

    xcb_flush(conn);
}

int snap_handle_key_press(xcb_connection_t *conn, xcb_screen_t *screen,
                           xcb_key_press_event_t *e) {
    uint16_t clean = (uint16_t)(e->state & ~(XCB_MOD_MASK_LOCK | config_numlock_mask()));

    if (kc_tab && e->detail == kc_tab) {
        if (clean & XCB_MOD_MASK_4) {
            window_minimize_all(conn, screen);
            return 1;
        }
        if (clean & XCB_MOD_MASK_CONTROL) {
            client_t *c = window_focused_client(conn);
            if (c) window_apply_snap(conn, screen, c, 0); /* открепить от снэппинга */
            return 1;
        }
        return 0;
    }

    if (!(clean & XCB_MOD_MASK_4)) return 0;

    int bit = 0;
    if (kc_left && e->detail == kc_left)        bit = SNAP_LEFT;
    else if (kc_right && e->detail == kc_right) bit = SNAP_RIGHT;
    else if (kc_up && e->detail == kc_up)       bit = SNAP_TOP;
    else if (kc_down && e->detail == kc_down)   bit = SNAP_BOTTOM;
    else return 0;

    client_t *c = window_focused_client(conn);

    /* Фокус сменился с прошлого удержания стрелки — накопленная маска
       относилась к другому окну, обнуляем её (баг №3). */
    if (c != held_client) {
        held_mask = 0;
        held_client = c;
    }

    /* Left/Right и Top/Bottom попарно взаимоисключающие — новая стрелка
       заменяет противоположную вместо того, чтобы складываться с ней
       (иначе Left, затем Right без отпускания Left дал бы бессмысленную
       маску LEFT|RIGHT). */
    if (bit == SNAP_LEFT)        held_mask &= ~SNAP_RIGHT;
    else if (bit == SNAP_RIGHT)  held_mask &= ~SNAP_LEFT;
    else if (bit == SNAP_TOP)    held_mask &= ~SNAP_BOTTOM;
    else if (bit == SNAP_BOTTOM) held_mask &= ~SNAP_TOP;
    held_mask |= bit;

    if (c) window_apply_snap(conn, screen, c, held_mask);
    return 1;
}

void snap_client_removed(client_t *c) {
    if (held_client == c) {
        held_client = NULL;
        held_mask = 0;
    }
}

void snap_handle_key_release(xcb_key_release_event_t *e) {
    if (kc_left && e->detail == kc_left)        held_mask &= ~SNAP_LEFT;
    else if (kc_right && e->detail == kc_right) held_mask &= ~SNAP_RIGHT;
    else if (kc_up && e->detail == kc_up)       held_mask &= ~SNAP_TOP;
    else if (kc_down && e->detail == kc_down)   held_mask &= ~SNAP_BOTTOM;
    /* Геометрию окна намеренно не трогаем: отпускание одной из двух
       зажатых стрелок должно оставить уже применённую раскладку как
       есть, а не откатывать четверть обратно в половину. */
}
