#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xcb_cursor.h>
#include "cursor.h"

/* Устанавливает стандартный курсор-стрелку (glyph XC_left_ptr=68 из
   встроенного X-шрифта "cursor") на root-окно. Предыдущий WM (Openbox)
   сам задавал курсор root-окну, а QSWM при захвате root им не
   занимался. Используется только базовый xcb (без libxcursor). */
static void cursor_set_glyph(xcb_connection_t *conn, xcb_screen_t *screen) {
    xcb_font_t font = xcb_generate_id(conn);
    xcb_open_font(conn, font, strlen("cursor"), "cursor");

    xcb_cursor_t cursor = xcb_generate_id(conn);
    xcb_create_glyph_cursor(conn, cursor, font, font,
                             68, 69,           /* XC_left_ptr, mask-глиф сразу за ним */
                             0, 0, 0,          /* цвет переднего плана: чёрный */
                             0xffff, 0xffff, 0xffff); /* цвет фона: белый */

    uint32_t mask = XCB_CW_CURSOR;
    uint32_t values[1] = { cursor };
    xcb_change_window_attributes(conn, screen->root, mask, values);

    xcb_free_cursor(conn, cursor);
    xcb_close_font(conn, font);
}

/* xcb_cursor_context_new()/xcb_cursor_load_cursor() резолвят тему и
   размер курсора сами, через уже установленные системой X-ресурсы
   (Xcursor.theme/Xcursor.size в RESOURCE_MANAGER) и/или XCURSOR_PATH -
   этот источник задаёт xrdb/сессионный менеджер/DE при логине, а не
   QSWM, поэтому здесь ничего форсировать не нужно: просто пробуем
   загрузить "left_ptr" из того, что уже действует в системе, и если
   не вышло (темы нет вовсе) - откатываемся на встроенный глиф. */
void cursor_set_default(xcb_connection_t *conn, xcb_screen_t *screen) {
    xcb_cursor_context_t *ctx = NULL;
    if (xcb_cursor_context_new(conn, screen, &ctx) >= 0 && ctx) {
        xcb_cursor_t cursor = xcb_cursor_load_cursor(ctx, "left_ptr");
        if (cursor != XCB_CURSOR_NONE) {
            uint32_t mask = XCB_CW_CURSOR;
            uint32_t values[1] = { cursor };
            xcb_change_window_attributes(conn, screen->root, mask, values);
            xcb_free_cursor(conn, cursor);
            xcb_cursor_context_free(ctx);
            return;
        }
        xcb_cursor_context_free(ctx);
    }

    cursor_set_glyph(conn, screen);
}
