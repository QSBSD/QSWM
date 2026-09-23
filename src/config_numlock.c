#include <stdlib.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include "keysyms_min.h"
#include "config_numlock.h"

/* XCB_MOD_MASK_2 — запасной вариант на случай, если определить
   настоящую маску не удалось (совпадает со старым жёстко зашитым
   поведением). */
static uint16_t g_numlock_mask = XCB_MOD_MASK_2;

void config_detect_numlock(xcb_connection_t *conn) {
    xcb_key_symbols_t *ks = xcb_key_symbols_alloc(conn);
    if (!ks) return;

    xcb_keycode_t *kc = xcb_key_symbols_get_keycode(ks, XK_Num_Lock);
    xcb_keycode_t numlock_kc = kc ? kc[0] : 0;
    free(kc);
    xcb_key_symbols_free(ks);
    if (!numlock_kc) return;

    xcb_get_modifier_mapping_cookie_t mc = xcb_get_modifier_mapping(conn);
    xcb_get_modifier_mapping_reply_t *mr = xcb_get_modifier_mapping_reply(conn, mc, NULL);
    if (!mr) return;

    xcb_keycode_t *keycodes = xcb_get_modifier_mapping_keycodes(mr);
    int per_mod = mr->keycodes_per_modifier;

    /* Индексы 0..7 — это Shift, Lock, Control, Mod1..Mod5 (порядок
       фиксирован спецификацией X11), т.е. бит модификатора = 1 << mod. */
    for (int mod = 0; mod < 8; mod++) {
        for (int j = 0; j < per_mod; j++) {
            if (keycodes[mod * per_mod + j] == numlock_kc) {
                g_numlock_mask = (uint16_t)(1u << mod);
                free(mr);
                return;
            }
        }
    }
    free(mr);
}

uint16_t config_numlock_mask(void) {
    return g_numlock_mask;
}
