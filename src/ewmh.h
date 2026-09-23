#ifndef MYWM_EWMH_H
#define MYWM_EWMH_H

#include <xcb/xcb.h>
#include "window.h"

/* Минимальный набор EWMH (freedesktop.org wm-spec), достаточный, чтобы
   сторонние инструменты (wmctrl, xdotool, панели/пейджеры вроде polybar,
   tint2) видели список окон QSWM и его текущее состояние. ICCCM
   (WM_STATE, WM_DELETE_WINDOW - см. wm_state.c/window_actions.c) сам по
   себе для этого недостаточен: это более старый и куда более скудный
   протокол, ориентированный на связь клиент<->WM, а не WM<->внешние
   инструменты.

   Не реализовано намеренно (за пределами разумного объёма для этого WM):
   несколько виртуальных рабочих столов (_NET_NUMBER_OF_DESKTOPS всегда 1).
   ClientMessage от клиентов поддержан частично - см.
   ewmh_handle_client_message(): _NET_ACTIVE_WINDOW (нужен приложениям
   с режимом "один инстанс", напр. Geany) и _NET_WM_STATE с
   _NET_WM_STATE_FULLSCREEN (add/remove/toggle) - нужен Chromium,
   VirtualBox и любому клиенту, который сам просит у WM полноэкранный
   режим вместо хоткея. Остальные состояния (maximized_*, hidden) через
   ClientMessage по-прежнему не принимаются как команды. */

void ewmh_init(xcb_connection_t *conn, xcb_screen_t *screen);
void ewmh_cleanup(xcb_connection_t *conn);

/* Перестраивает _NET_CLIENT_LIST/_NET_CLIENT_LIST_STACKING из текущего
   window_get_clients(). Вызывается только из ewmh_init() (список ещё
   пуст) - для точечных добавлений/удалений одного окна используйте
   ewmh_client_added()/ewmh_client_removed() ниже, они не требуют
   пересборки всего списка на каждое такое событие. */
void ewmh_update_client_list(xcb_connection_t *conn, xcb_screen_t *screen);

/* Точечно добавляет/удаляет одно окно в кэш _NET_CLIENT_LIST/
   _NET_CLIENT_LIST_STACKING и сразу переотправляет оба свойства.
   Вызываются из window_manage()/window_unmanage()/window_withdraw()
   вместо полного ewmh_update_client_list() - раньше там на каждое
   такое событие заново обходился весь связный список клиентов ради
   изменения одного элемента. */
void ewmh_client_added(xcb_connection_t *conn, xcb_screen_t *screen, xcb_window_t win);
void ewmh_client_removed(xcb_connection_t *conn, xcb_screen_t *screen, xcb_window_t win);

/* win == XCB_NONE - фокус ни на одном управляемом окне (например, все
   окна закрыты). Вызывается везде, где меняется xcb_set_input_focus(). */
void ewmh_set_active_window(xcb_connection_t *conn, xcb_screen_t *screen, xcb_window_t win);

/* Пересчитывает и записывает _NET_WM_STATE клиента c из его текущих
   c->maximized/c->fullscreen/c->minimized. Вызывается везде, где меняется
   любой из этих флагов (toggle_maximize/toggle_fullscreen/toggle_minimize). */
void ewmh_update_wm_state(xcb_connection_t *conn, client_t *c);

/* Обрабатывает входящий ClientMessage к root-окну. Поддерживает
   _NET_ACTIVE_WINDOW (wm-spec): приложения с режимом "один инстанс"
   (Geany, Firefox и т.п.) шлют его существующему окну вместо запуска
   нового процесса, ожидая, что WM поднимет и сфокусирует это окно; и
   _NET_WM_STATE с _NET_WM_STATE_FULLSCREEN - клиент сам просит войти/
   выйти из полноэкранного режима (data32[0] = 0 remove/1 add/2 toggle,
   data32[1..2] = запрашиваемые атомы состояния). Событие передаётся
   из wm.c (case XCB_CLIENT_MESSAGE) как есть. */
void ewmh_handle_client_message(xcb_connection_t *conn, xcb_screen_t *screen,
                                 xcb_client_message_event_t *e);

#endif
