#include <xcb/xcb.h>
#include <xcb/render.h>
#include "window.h"
#include "window_drag.h"
#include "icons.h"
#include "xrender_util.h"

/* Рисует titlebar: фон, SVG-иконку приложения (пункт 7, слева или справа
   в зависимости от [appearance].icon_left, пункт 6) и глифы кнопок
   − ▢ × (пункт 3, icons_draw_button()). Размеры titlebar_height/
   button_size — из [appearance] (window_set_appearance(), пункт 6). */
void window_draw_decoration(xcb_connection_t *conn, xcb_screen_t *screen, client_t *c) {
    uint16_t tb = window_titlebar_height();
    uint16_t bs = window_button_size();

    /* Picture создаётся один раз поверх drawable c->frame и живёт всё
       время жизни клиента. Она не привязана к фиксированному размеру
       буфера, а рисует по текущей геометрии drawable, поэтому при
       resize/maximize/snap её не нужно ни пересоздавать, ни уведомлять
       о новой ширине — окно и так уже переконфигурировано к этому
       моменту (см. вызовы window_draw_decoration в других модулях). */
    if (c->deco_picture == XCB_NONE) {
        c->deco_picture = xrender_create_picture_for_visual(conn, c->frame, screen->root_visual);
        if (c->deco_picture == XCB_NONE) return;
    }

    xcb_render_color_t bg = window_titlebar_color(); /* [appearance].titlebar_color, чёрный по умолчанию */
    xcb_rectangle_t bg_rect = { 0, 0, c->width, tb };
    xcb_render_fill_rectangles(conn, XCB_RENDER_PICT_OP_OVER, c->deco_picture, bg, 1, &bg_rect);

    /* Иконка приложения (пункт 7): реальный SVG, найденный по WM_CLASS и
       отрендеренный nanosvg в offscreen pixmap, скомпонованный через
       XRender (icons.c). Сторона — по icon_left. Если подходящего файла
       нет — заглушка-квадрат. */
    int16_t icon_y = (int16_t)((tb - bs) / 2);
    int16_t icon_x = window_icon_left()
                          ? BUTTON_MARGIN
                          : (int16_t)(c->width - BUTTON_MARGIN - bs);
    if (!icons_draw(c->deco_picture, c->app_class, icon_x, icon_y, bs)) {
        xcb_render_color_t fallback = xrender_color_rgba(0.30, 0.30, 0.34, 1.0);
        xcb_rectangle_t icon_rect = { icon_x, icon_y, bs, bs };
        xcb_render_fill_rectangles(conn, XCB_RENDER_PICT_OP_OVER, c->deco_picture, fallback, 1, &icon_rect);
    }

    xcb_rectangle_t rc, rm, rn;
    window_drag_button_rects(c->width, &rc, &rm, &rn);

    /* Глифы кнопок − □ ×: простая геометрия (линия, рамка, диагонали)
       через голый XCB (icons_draw_button(), icons.c) — без RENDER и без
       текста, в отличие от SVG-иконки приложения выше, которая идёт
       через XRender Picture. */
    icons_draw_button(conn, c->frame, ICON_BTN_MINIMIZE, rn.x, rn.y, bs);
    icons_draw_button(conn, c->frame, ICON_BTN_MAXIMIZE, rm.x, rm.y, bs);
    icons_draw_button(conn, c->frame, ICON_BTN_CLOSE,    rc.x, rc.y, bs);

    xcb_flush(conn);
}

void window_expose(xcb_connection_t *conn, xcb_screen_t *screen, xcb_expose_event_t *e) {
    client_t *c = client_find_by_frame(e->window);
    if (!c) return;

    /* Отложенный авто-maximize нового окна (window_manage(), window_client.c) -
       применяем его на первом Expose, когда клиент уже успел отрисовать
       свой первый кадр на "естественном" размере, а не раньше. Само
       window_toggle_maximize() ниже уже перерисует декорацию и сделает
       flush, так что до конца этой функции больше ничего делать не нужно. */
    if (c->pending_auto_maximize) {
        c->pending_auto_maximize = 0;
        window_toggle_maximize(conn, screen, c);
        return;
    }

    if (c->fullscreen || c->snapped) return; /* в fullscreen/snap titlebar скрыт, рисовать нечего */
    window_draw_decoration(conn, screen, c);
}
