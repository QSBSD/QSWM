#include <xcb/xcb.h>
#include "window.h"

/* ICCCM 4.1.5: если WM отказывает клиенту в его ConfigureRequest (не
   меняя геометрию), клиент всё равно должен получить ConfigureNotify —
   иначе часть тулкитов (GTK) считает запрос "потерянным" и повторяет
   его в цикле. border_width всегда 0 - декорация рисуется отдельно,
   реальной X-рамки у клиентского окна нет (см. window_manage()). */
void window_send_synthetic_configure(xcb_connection_t *conn, client_t *c) {
    uint16_t tb = window_titlebar_height();
    xcb_configure_notify_event_t ce = {0};
    ce.response_type    = XCB_CONFIGURE_NOTIFY;
    ce.event            = c->win;
    ce.window           = c->win;
    ce.above_sibling    = XCB_NONE;
    ce.x                = c->x;
    ce.y                = (int16_t)(c->y + tb);
    ce.width            = c->width;
    ce.height           = c->height;
    ce.border_width     = 0;
    ce.override_redirect = 0;
    xcb_send_event(conn, 0, c->win, XCB_EVENT_MASK_STRUCTURE_NOTIFY, (const char *)&ce);
    xcb_flush(conn);
}

void window_configure(xcb_connection_t *conn, xcb_screen_t *screen, xcb_configure_request_event_t *e) {
    client_t *c = client_find_by_window(e->window);

    if (!c) {
        /* окно ещё не управляется (обычный путь ICCCM для немапнутых окон) —
           просто пропускаем запрос как есть */
        uint32_t values[7];
        uint16_t mask = 0;
        int i = 0;

        if (e->value_mask & XCB_CONFIG_WINDOW_X) { values[i++] = (uint32_t)e->x; mask |= XCB_CONFIG_WINDOW_X; }
        if (e->value_mask & XCB_CONFIG_WINDOW_Y) { values[i++] = (uint32_t)e->y; mask |= XCB_CONFIG_WINDOW_Y; }
        if (e->value_mask & XCB_CONFIG_WINDOW_WIDTH) { values[i++] = e->width; mask |= XCB_CONFIG_WINDOW_WIDTH; }
        if (e->value_mask & XCB_CONFIG_WINDOW_HEIGHT) { values[i++] = e->height; mask |= XCB_CONFIG_WINDOW_HEIGHT; }
        if (e->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) { values[i++] = e->border_width; mask |= XCB_CONFIG_WINDOW_BORDER_WIDTH; }
        if (e->value_mask & XCB_CONFIG_WINDOW_SIBLING) { values[i++] = e->sibling; mask |= XCB_CONFIG_WINDOW_SIBLING; }
        if (e->value_mask & XCB_CONFIG_WINDOW_STACK_MODE) { values[i++] = e->stack_mode; mask |= XCB_CONFIG_WINDOW_STACK_MODE; }

        xcb_configure_window(conn, e->window, mask, values);
        return;
    }

    /* Окно в снэпе/максимизации/фуллскрине не должно менять геометрию по
       собственному ConfigureRequest — иначе клиент (Electron/GTK и т.п.,
       которые сами шлют ConfigureRequest после мапа или на ресайз)
       молча уводит frame из снэп-позиции, а c->snapped/maximized при
       этом остаётся выставленным: декорация и кнопки уезжают от
       видимого края экрана. Подтверждаем клиенту его же текущую
       геометрию (synthetic ConfigureNotify) и выходим. */
    if (c->snapped || c->maximized || c->fullscreen) {
        window_send_synthetic_configure(conn, c);
        return;
    }

    /* окно уже управляется: применяем запрошенную геометрию и к client_t,
       и к frame (раньше frame оставался прежнего размера, и декорация
       переставала совпадать с реальным размером клиентской области) */
    if (e->value_mask & XCB_CONFIG_WINDOW_WIDTH)  c->width  = e->width;
    if (e->value_mask & XCB_CONFIG_WINDOW_HEIGHT) c->height = e->height;
    if (e->value_mask & XCB_CONFIG_WINDOW_X)      c->x      = e->x;
    if (e->value_mask & XCB_CONFIG_WINDOW_Y)      c->y      = e->y;

    if (c->width  < MIN_WIDTH)  c->width  = MIN_WIDTH;
    if (c->height < MIN_HEIGHT) c->height = MIN_HEIGHT;
    window_clamp_size(c, &c->width, &c->height); /* WM_NORMAL_HINTS PMinSize/PMaxSize */

    uint16_t tb = window_titlebar_height();
    uint32_t frame_values[4] = {
        (uint32_t)c->x, (uint32_t)c->y, c->width, (uint32_t)(c->height + tb)
    };
    xcb_configure_window(conn, c->frame,
        XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
        XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, frame_values);

    uint32_t child_values[2] = { c->width, c->height };
    xcb_configure_window(conn, c->win,
        XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, child_values);

    if (e->value_mask & XCB_CONFIG_WINDOW_STACK_MODE) {
        uint32_t sm = e->stack_mode;
        xcb_configure_window(conn, c->frame, XCB_CONFIG_WINDOW_STACK_MODE, &sm);
    }

    xcb_flush(conn);
    if (!c->fullscreen) window_draw_decoration(conn, screen, c);
}
