#ifndef MYWM_SNAP_H
#define MYWM_SNAP_H

#include <xcb/xcb.h>
#include "window.h"

/* Снэппинг окон по Meta+стрелки (левая/правая половина экрана, при
   зажатии второй стрелки — уточнение до четверти), Meta+Tab (свернуть
   все окна) и Ctrl+Tab (открепить текущее окно от снэппинга). Грабит
   свои комбинации клавиш напрямую на root, как altTab.c грабит Alt+Tab —
   в обход статической таблицы биндингов config.c, потому что четверть
   экрана определяется по двум одновременно зажатым стрелкам, а не по
   одному статичному сочетанию клавиш. */

void snap_init(xcb_connection_t *conn, xcb_screen_t *screen);

/* Обрабатывает KeyPress. Возвращает 1, если событие относилось к одной
   из наших комбинаций (и, значит, дальше его обрабатывать не нужно —
   в частности, не передавать в alttab_handle_key_press), иначе 0. */
int snap_handle_key_press(xcb_connection_t *conn, xcb_screen_t *screen,
                           xcb_key_press_event_t *e);

/* Обрабатывает KeyRelease стрелок — снимает бит с "зажатой" маски,
   чтобы следующее нажатие стрелки началось не с накопленного состояния
   уже отпущенных клавиш. Саму геометрию окна при отпускании не меняет. */
void snap_handle_key_release(xcb_key_release_event_t *e);

/* Сообщает snap.c, что client_t по адресу c только что освобождён
   (window_unmanage()/window_withdraw()). Если накопленная маска
   held_mask относилась именно к этому клиенту, обнуляет её вместе с
   held_client — без этого held_client остаётся висячим указателем,
   который сравнивается (хоть и не разыменовывается) в
   snap_handle_key_press() при следующем нажатии стрелки. */
void snap_client_removed(client_t *c);

#endif
