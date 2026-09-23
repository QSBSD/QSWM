#include <stdlib.h>
#include <xcb/xcb.h>
#include "window.h"
#include "ewmh.h"
#include "wm.h"

/* Переключает истинный полноэкранный режим (хоткей fullscreen, по
   умолчанию F11) — в отличие от maximize, titlebar тоже скрывается и
   клиентская область растягивается на весь экран без отступа TB(). */
void window_toggle_fullscreen(xcb_connection_t *conn, xcb_screen_t *screen, client_t *c) {
    if (c->minimized) return;
    c->snapped = 0; /* fullscreen отменяет снэппинг, как и maximize выше */
    uint16_t tb = window_titlebar_height();

    if (!c->fullscreen) {
        /* Всегда запоминаем ТЕКУЩУЮ геометрию и текущий флаг maximized -
           независимо от того, maximized сейчас окно или нет (см. разбор
           бага в window.h у prefs_x/y/width/height). Раньше здесь была
           условная запись в saved_* (общие с window_maximize.c поля),
           из-за чего выход из fullscreen у уже maximized окна возвращал
           его к "естественному" домаксимизационному размеру вместо
           maximized-размера. */
        c->prefs_x = c->x;
        c->prefs_y = c->y;
        c->prefs_width = c->width;
        c->prefs_height = c->height;
        c->prefs_maximized = c->maximized;

        uint16_t scr_w, scr_h;
        screen_get_size(conn, screen, &scr_w, &scr_h);

        c->x = 0;
        c->y = 0;
        c->width = scr_w;
        c->height = scr_h;
        window_clamp_size(c, &c->width, &c->height); /* PMaxSize - не растягивать окно с фиксированным размером */
        c->fullscreen = 1;

        /* frame растягиваем на весь экран и кладём дочернее окно вровень
           с ним (0,0, во весь размер) — titlebar физически не рисуется
           поверх клиентской области, т.к. её нечем перекрыть: window_draw_decoration()
           ниже пропускает отрисовку, пока c->fullscreen == 1. */
        uint32_t frame_values[4] = {
            (uint32_t)c->x, (uint32_t)c->y, c->width, c->height
        };
        xcb_configure_window(conn, c->frame,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, frame_values);

        uint32_t child_pos[2] = { 0, 0 };
        xcb_configure_window(conn, c->win,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, child_pos);
        uint32_t child_size[2] = { c->width, c->height };
        xcb_configure_window(conn, c->win,
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, child_size);

        uint32_t stack_mode = XCB_STACK_MODE_ABOVE;
        xcb_configure_window(conn, c->frame, XCB_CONFIG_WINDOW_STACK_MODE, &stack_mode);
    } else {
        c->x = c->prefs_x;
        c->y = c->prefs_y;
        c->width = c->prefs_width;
        c->height = c->prefs_height;
        c->maximized = c->prefs_maximized; /* вернуть ▢-состояние, а не считать окно "обычным" */
        c->fullscreen = 0;

        uint32_t frame_values[4] = {
            (uint32_t)c->x, (uint32_t)c->y, c->width, (uint32_t)(c->height + tb)
        };
        xcb_configure_window(conn, c->frame,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, frame_values);

        uint32_t child_pos[2] = { 0, tb };
        xcb_configure_window(conn, c->win,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, child_pos);
        uint32_t child_size[2] = { c->width, c->height };
        xcb_configure_window(conn, c->win,
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, child_size);
    }

    xcb_flush(conn);
    if (!c->fullscreen) window_draw_decoration(conn, screen, c);
    ewmh_update_wm_state(conn, c);
}
