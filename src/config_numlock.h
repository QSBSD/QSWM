#ifndef MYWM_CONFIG_NUMLOCK_H
#define MYWM_CONFIG_NUMLOCK_H

#include <xcb/xcb.h>

/* X11 не гарантирует, что NumLock замаплен именно на Mod2 — это лишь
   конвенция большинства раскладок, а не спецификация. Раньше везде, где
   граб делался в 4 варианта "лишних" модификаторов (config_keys.c,
   altTab.c, snap.c) и при сравнении состояния клавиши с биндингом, NumLock
   был жёстко приравнен к XCB_MOD_MASK_2 — на нестандартных раскладках
   хоткеи и Alt-Tab могли не сработать при включённом NumLock.
   config_detect_numlock() один раз спрашивает реальную modifier map
   через xcb_get_modifier_mapping() и кэширует результат; config_numlock_mask()
   отдаёт закэшированное значение (XCB_MOD_MASK_2 как запасной вариант,
   если Num_Lock не нашёлся ни в одном модификаторе). Вызывается один раз
   из wm_init(), до config_grab_keys()/alttab_init()/snap_init(). */
void config_detect_numlock(xcb_connection_t *conn);
uint16_t config_numlock_mask(void);

#endif
