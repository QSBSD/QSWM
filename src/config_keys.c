#include <stdlib.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include "config_keys.h"
#include "config_numlock.h"

/* Грабим один биндинг в 4 вариантах (см. grab_tab() в altTab.c) -
   NumLock/CapsLock не должны мешать срабатыванию хоткея. */
static void grab_one(xcb_connection_t *conn, xcb_screen_t *screen, keybinding_t *kb) {
    uint16_t numlock = config_numlock_mask();
    uint16_t extra[] = {0, XCB_MOD_MASK_LOCK, numlock,
                         (uint16_t)(XCB_MOD_MASK_LOCK | numlock)};
    for (size_t i = 0; i < sizeof(extra) / sizeof(extra[0]); i++) {
        xcb_grab_key(conn, 1, screen->root, (uint16_t)(kb->modifiers | extra[i]),
                     kb->keycode, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    }
}

void config_grab_keys(xcb_connection_t *conn, xcb_screen_t *screen, wm_config_t *cfg) {
    xcb_key_symbols_t *keysyms = xcb_key_symbols_alloc(conn);
    if (!keysyms) return;

    for (int i = 0; i < cfg->binding_count; i++) {
        keybinding_t *kb = &cfg->bindings[i];
        if (kb->action == ACTION_ALTTAB) continue; /* грабится в altTab.c */

        xcb_keycode_t *kc = xcb_key_symbols_get_keycode(keysyms, kb->keysym);
        if (!kc) continue;
        kb->keycode = kc[0];
        free(kc);

        if (kb->keycode) grab_one(conn, screen, kb);
    }

    xcb_key_symbols_free(keysyms);
    xcb_flush(conn);
}

const keybinding_t *config_find_binding(const wm_config_t *cfg, uint16_t state,
                                         xcb_keycode_t keycode) {
    uint16_t clean_state = (uint16_t)(state & ~(XCB_MOD_MASK_LOCK | config_numlock_mask()));

    for (int i = 0; i < cfg->binding_count; i++) {
        const keybinding_t *kb = &cfg->bindings[i];
        if (kb->action == ACTION_ALTTAB) continue;
        if (kb->keycode == keycode && kb->modifiers == clean_state)
            return kb;
    }
    return NULL;
}
