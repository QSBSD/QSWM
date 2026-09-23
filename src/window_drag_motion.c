#include <xcb/xcb.h>
#include "window.h"
#include "window_drag_priv.h"

void window_motion_notify(xcb_connection_t *conn, xcb_motion_notify_event_t *e) {
    if (g_window_drag.mode == DRAG_NONE || !g_window_drag.client) return;
    client_t *c = g_window_drag.client;

    int16_t dx = (int16_t)(e->root_x - g_window_drag.pointer_start_x);
    int16_t dy = (int16_t)(e->root_y - g_window_drag.pointer_start_y);

    if (g_window_drag.mode == DRAG_MOVE) {
        c->x = (int16_t)(g_window_drag.frame_start_x + dx);
        c->y = (int16_t)(g_window_drag.frame_start_y + dy);

        uint32_t values[2] = { (uint32_t)c->x, (uint32_t)c->y };
        xcb_configure_window(conn, c->frame,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
        return;
    }

    /* DRAG_RESIZE — считаем в int и клампим ДО каста в uint16_t: иначе
       если тянуть край мимо противоположного (frame_start_w ± dx уходит
       в отрицательные значения), каст отрицательного int в uint16_t
       заворачивается в огромное положительное число (напр. -5 -> 65531),
       и проверка "< MIN_WIDTH/MIN_HEIGHT" ниже эту обёртку не ловит —
       окно раздувается почти до 65535px вместо остановки на минимуме. */
    int32_t new_x = g_window_drag.frame_start_x;
    int32_t new_w = g_window_drag.frame_start_w;
    int32_t new_h = g_window_drag.frame_start_h;

    if (g_window_drag.edges & EDGE_RIGHT)
        new_w = (int32_t)g_window_drag.frame_start_w + dx;
    if (g_window_drag.edges & EDGE_LEFT) {
        new_w = (int32_t)g_window_drag.frame_start_w - dx;
        new_x = (int32_t)g_window_drag.frame_start_x + dx;
    }
    if (g_window_drag.edges & EDGE_BOTTOM)
        new_h = (int32_t)g_window_drag.frame_start_h + dy;

    if (new_w < c->min_width) {
        if (g_window_drag.edges & EDGE_LEFT)
            new_x = (int32_t)g_window_drag.frame_start_x + g_window_drag.frame_start_w - c->min_width;
        new_w = c->min_width;
    }
    if (new_h < c->min_height)
        new_h = c->min_height;

    /* WM_NORMAL_HINTS PMaxSize (window_hints.c): некоторые приложения
       (калькуляторы, диалоги, ...) объявляют жёсткий верхний предел
       размера - раньше это никак не проверялось, и такое окно можно было
       растянуть мышью сколько угодно, хотя само приложение всё равно не
       перерисовалось бы за пределами заявленного максимума. */
    if (c->max_width && new_w > c->max_width) {
        if (g_window_drag.edges & EDGE_LEFT)
            new_x = (int32_t)g_window_drag.frame_start_x + g_window_drag.frame_start_w - c->max_width;
        new_w = c->max_width;
    }
    if (c->max_height && new_h > c->max_height)
        new_h = c->max_height;

    c->x = (int16_t)new_x;
    c->width = (uint16_t)new_w;
    c->height = (uint16_t)new_h;

    uint32_t frame_values[3] = {
        (uint32_t)new_x, (uint32_t)new_w, (uint32_t)(new_h + window_titlebar_height())
    };
    xcb_configure_window(conn, c->frame,
        XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
        frame_values);

    uint32_t child_values[2] = { (uint32_t)new_w, (uint32_t)new_h };
    xcb_configure_window(conn, c->win,
        XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, child_values);
}
