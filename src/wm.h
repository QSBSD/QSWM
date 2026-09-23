#ifndef MYWM_WM_H
#define MYWM_WM_H

#include <xcb/xcb.h>

/* Актуальный размер экрана в пикселях. screen->width_in_pixels/
   height_in_pixels из xcb_screen_t фиксируются на старте X-сервера и не
   обновляются при горячей смене режима (xrandr --output ... --mode ...,
   докинг/отключение монитора) - используем их только как fallback,
   если xcb_get_geometry(root) вдруг не ответит. Было продублировано
   одинаковым 8-строчным блоком в window_fullscreen.c/window_maximize.c/
   window_snap_apply.c/altTab.c - вынесено сюда во избежание рассинхрона
   при будущих правках одной из копий. */
void screen_get_size(xcb_connection_t *conn, xcb_screen_t *screen,
                      uint16_t *out_w, uint16_t *out_h);

int  wm_init(xcb_connection_t *conn, xcb_screen_t *screen);
/* exit_fd - читающий конец self-pipe из main.c: как только сигнальный
   обработчик SIGTERM/SIGINT напишет туда байт, wm_run() выходит из цикла
   штатно (не из самого обработчика сигнала - см. комментарий в main.c). */
void wm_run(xcb_connection_t *conn, xcb_screen_t *screen, int exit_fd);
void wm_cleanup(xcb_connection_t *conn);

#endif
