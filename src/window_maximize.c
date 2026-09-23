#include <stdlib.h>
#include <xcb/xcb.h>
#include "window.h"
#include "ewmh.h"
#include "wm.h"

/* Переключает окно между обычным и развёрнутым (maximize/restore) —
   вызывается по клику на кнопку ▢ (пункт 5) и по хоткею maximize (пункт 6). */
void window_toggle_maximize(xcb_connection_t *conn, xcb_screen_t *screen, client_t *c) {
    if (c->minimized) return; /* свёрнутое окно не разворачиваем напрямую */
    c->snapped = 0; /* maximize отменяет снэппинг (Meta+стрелки) без права на "открепить" назад */
    uint16_t tb = window_titlebar_height();
    if (!c->maximized) {
        c->saved_x = c->x;
        c->saved_y = c->y;
        c->saved_width = c->width;
        c->saved_height = c->height;

        /* screen->width_in_pixels/height_in_pixels — снимок с момента
           xcb_connect() в main.c; WM не следит за RandR-уведомлениями
           о смене разрешения, поэтому эти поля могут быть устаревшими.
           screen_get_size() берёт актуальную геометрию root-окна прямо
           сейчас, с фолбэком на эти поля. */
        uint16_t scr_w, scr_h;
        screen_get_size(conn, screen, &scr_w, &scr_h);

        c->x = 0;
        c->y = 0;
        c->width = scr_w;
        c->height = (uint16_t)(scr_h - tb);
        window_clamp_size(c, &c->width, &c->height); /* PMaxSize - не растягивать окно с фиксированным размером */
        c->maximized = 1;
    } else {
        c->x = c->saved_x;
        c->y = c->saved_y;
        c->width = c->saved_width;
        c->height = c->saved_height;
        c->maximized = 0;
    }

    uint32_t frame_values[4] = {
        (uint32_t)c->x, (uint32_t)c->y,
        c->width, (uint32_t)(c->height + tb)
    };
    xcb_configure_window(conn, c->frame,
        XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
        XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, frame_values);

    /* Если до этого окно было в снэпе (window_snap_apply.c ставит
       child_y=0, т.к. снэп рисуется без titlebar) и maximize вызван
       напрямую по кнопке □ (window_drag_press.c пропускает клик по
       кнопкам раньше проверки c->snapped), child мог остаться на
       y=0 — сбрасываем позицию явно, иначе контент заезжает под
       titlebar, хотя frame стоит в правильном maximized-прямоугольнике. */
    uint32_t child_pos[2] = { 0, tb };
    xcb_configure_window(conn, c->win,
        XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, child_pos);
    uint32_t child_values[2] = { c->width, c->height };
    xcb_configure_window(conn, c->win,
        XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, child_values);

    xcb_flush(conn);
    window_draw_decoration(conn, screen, c);
    ewmh_update_wm_state(conn, c);
}
