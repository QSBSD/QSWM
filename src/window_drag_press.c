#include <stdlib.h>
#include <xcb/xcb.h>
#include "window.h"
#include "window_drag.h"
#include "window_drag_priv.h"
#include "ewmh.h"

static int in_rect(const xcb_rectangle_t *r, int16_t x, int16_t y) {
    return x >= r->x && x < r->x + r->width && y >= r->y && y < r->y + r->height;
}

/* Определяет, за какие края frame схватил указатель (в координатах frame).
   Верхний край не обрабатывается — там titlebar, отвечающий за перетаскивание. */
static int detect_resize_edges(client_t *c, int16_t x, int16_t y) {
    uint16_t frame_w = c->width;
    uint16_t frame_h = (uint16_t)(c->height + window_titlebar_height());
    int edges = EDGE_NONE;

    if (x < RESIZE_MARGIN)
        edges |= EDGE_LEFT;
    else if (x >= frame_w - RESIZE_MARGIN)
        edges |= EDGE_RIGHT;

    if (y >= frame_h - RESIZE_MARGIN)
        edges |= EDGE_BOTTOM;

    return edges;
}

void window_button_press(xcb_connection_t *conn, xcb_screen_t *screen, xcb_button_press_event_t *e) {
    client_t *c = client_find_by_frame(e->event);
    if (!c) {
        /* Событие не по frame, а по клиентскому окну c->win — приходит
           благодаря пассивному xcb_grab_button(), поставленному в
           window_manage() на каждое управляемое окно (см. комментарий
           там). Раньше клик по рабочей области неактивного окна вообще
           не долетал до WM, и оно не фокусировалось/не поднималось.
           Grab сделан с owner_events=1 и async-режимами без freeze,
           поэтому клиент сам получит тот же клик как обычно — здесь
           только фокус и подъём, move/resize тут не начинаем. */
        c = client_find_by_window(e->event);
        if (!c) return;
        uint32_t stack_mode = XCB_STACK_MODE_ABOVE;
        xcb_configure_window(conn, c->frame, XCB_CONFIG_WINDOW_STACK_MODE, &stack_mode);
        window_set_focus(conn, screen, c);
        return;
    }

    /* клик по декорации (titlebar/рамка захвата ресайза) */
    uint32_t stack_mode = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(conn, c->frame, XCB_CONFIG_WINDOW_STACK_MODE, &stack_mode);
    window_set_focus(conn, screen, c);

    uint16_t tb = window_titlebar_height();
    if (e->event_y < tb) {
        xcb_rectangle_t rc, rm, rn;
        window_drag_button_rects(c->width, &rc, &rm, &rn);

        if (in_rect(&rc, e->event_x, e->event_y)) {
            window_close(conn, c);
            return;
        }
        if (in_rect(&rm, e->event_x, e->event_y)) {
            window_toggle_maximize(conn, screen, c);
            return;
        }
        if (in_rect(&rn, e->event_x, e->event_y)) {
            window_toggle_minimize(conn, screen, c);
            return;
        }
    }

    if (c->maximized || c->minimized || c->fullscreen || c->snapped) return; /* не двигаем/не ресайзим */

    /* detect_resize_edges() смотрит только на x для LEFT/RIGHT, не зная,
       что клик пришёлся в titlebar — раньше клик у бокового края
       titlebar (в пределах RESIZE_MARGIN от края, но не по кнопке)
       ошибочно запускал горизонтальный DRAG_RESIZE вместо перетаскивания
       окна. Titlebar (кроме кнопок, уже обработанных выше) — всегда move;
       боковые/нижняя зоны ресайза действуют только в клиентской области. */
    int edges;
    if (e->event_y < tb) {
        edges = EDGE_NONE;
    } else {
        edges = detect_resize_edges(c, e->event_x, e->event_y);
        if (edges == EDGE_NONE) return; /* клик в клиентской области вне зон захвата */
    }

    xcb_grab_pointer_cookie_t gc = xcb_grab_pointer(
        conn, 0, screen->root,
        XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION,
        XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
        XCB_NONE, XCB_NONE, XCB_CURRENT_TIME);
    xcb_grab_pointer_reply_t *gr = xcb_grab_pointer_reply(conn, gc, NULL);
    if (!gr || gr->status != XCB_GRAB_STATUS_SUCCESS) {
        free(gr);
        return;
    }
    free(gr);

    g_window_drag.client = c;
    g_window_drag.pointer_start_x = e->root_x;
    g_window_drag.pointer_start_y = e->root_y;
    g_window_drag.frame_start_x = c->x;
    g_window_drag.frame_start_y = c->y;
    g_window_drag.frame_start_w = c->width;
    g_window_drag.frame_start_h = c->height;

    if (edges != EDGE_NONE) {
        g_window_drag.mode = DRAG_RESIZE;
        g_window_drag.edges = edges;
    } else {
        g_window_drag.mode = DRAG_MOVE;
        g_window_drag.edges = EDGE_NONE;
    }
}
