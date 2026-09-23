#ifndef MYWM_CONFIG_KEYS_H
#define MYWM_CONFIG_KEYS_H

#include <xcb/xcb.h>
#include "config.h"

/* Регистрирует XGrabKey на root-окне для всех биндингов cfg, кроме
   ACTION_ALTTAB (Alt+Tab грабится отдельно в alttab_init, см. пункт 4).
   Как и в altTab.c, каждый биндинг грабится в 4 вариантах - с учётом
   NumLock/CapsLock как "лишних" модификаторов. */
void config_grab_keys(xcb_connection_t *conn, xcb_screen_t *screen, wm_config_t *cfg);

/* Ищет биндинг по событию KeyPress (state/keycode). state сравнивается
   с отброшенными битами Lock/Mod2. Возвращает NULL, если не найдено. */
const keybinding_t *config_find_binding(const wm_config_t *cfg, uint16_t state,
                                         xcb_keycode_t keycode);

#endif
