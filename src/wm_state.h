#ifndef MYWM_WM_STATE_H
#define MYWM_WM_STATE_H

#include <xcb/xcb.h>

/* ICCCM 4.1.3.1: WM_STATE — без него wmctrl, панели и сами клиенты,
   проверяющие своё состояние через это свойство, работают некорректно.
   icon_window всегда None — QSWM не поддерживает отдельные окна-иконки.
   Используется и window_client.c (manage/withdraw), и window_actions.c
   (force_minimize/toggle_minimize) — вынесено в общий внутренний хелпер,
   не часть публичного API window.h. */
#define WM_STATE_WITHDRAWN 0u
#define WM_STATE_NORMAL    1u
#define WM_STATE_ICONIC    3u

void wm_state_set(xcb_connection_t *conn, xcb_window_t win, uint32_t state);

#endif
