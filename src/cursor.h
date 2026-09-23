#ifndef MYWM_CURSOR_H
#define MYWM_CURSOR_H

#include <xcb/xcb.h>

/* Ставит курсор root-окна: пытается загрузить "left_ptr" из темы,
   уже действующей в системе (Xcursor.theme в RESOURCE_MANAGER —
   обычно её выставляет xrdb/сессионный менеджер/DE, а не сам WM),
   либо, если тема не найдена, откатывается на базовый X-шрифтовый
   курсор-стрелку. Без этого курсор над пустым столом невидим.
   Вызывается один раз из wm_init(). */
void cursor_set_default(xcb_connection_t *conn, xcb_screen_t *screen);

#endif
