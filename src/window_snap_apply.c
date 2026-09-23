#include <stdlib.h>
#include <xcb/xcb.h>
#include "window.h"
#include "wm.h"

/* Применяет/снимает снэппинг окна (Meta+стрелки / Ctrl+Tab, см. snap.c).
   mask==0 — открепить: возвращаем геометрию, сохранённую перед первым
   снэппингом. mask!=0 — одна сторона (LEFT/RIGHT/TOP/BOTTOM) даёт
   половину экрана, две смежные стороны — четверть. Если окно сейчас
   maximized или fullscreen, сначала аккуратно восстанавливаем его через
   уже существующие toggle-функции, чтобы не смешивать три независимых
   "полноразмерных" состояния. */
void window_apply_snap(xcb_connection_t *conn, xcb_screen_t *screen, client_t *c, int mask) {
    if (c->minimized) return;
    uint16_t tb = window_titlebar_height();

    if (mask == 0) {
        if (!c->snapped) return; /* нечего открепить */
        c->x = c->presnap_x;
        c->y = c->presnap_y;
        c->width = c->presnap_width;
        c->height = c->presnap_height;
        c->snapped = 0;

        if (c->presnap_maximized) {
            /* Окно было развёрнуто (□) до того, как его сдвинули
               Meta+стрелками - открепление (Ctrl+Tab) должно вернуть его
               в развёрнутое состояние, а не к "естественному" размеру,
               который presnap_* хранят лишь как промежуточный шаг.
               window_toggle_maximize() сам применит геометрию,
               перерисует декорацию, обновит EWMH и сделает flush -
               обычный путь ниже по функции для этого случая не нужен. */
            window_toggle_maximize(conn, screen, c);
            return;
        }
    } else {
        int was_maximized = 0;
        if (!c->snapped) {
            /* Первое применение снэппинга в этой серии (ещё не в снэпе) -
               запоминаем "был развёрнут" ДО восстановления чуть ниже,
               иначе Ctrl+Tab потом вернёт окно к его исходному маленькому
               размеру вместо разворота на весь экран. */
            was_maximized = c->maximized;
        }

        if (c->maximized)  window_toggle_maximize(conn, screen, c);
        if (c->fullscreen) window_toggle_fullscreen(conn, screen, c);

        if (!c->snapped) {
            c->presnap_x = c->x;
            c->presnap_y = c->y;
            c->presnap_width = c->width;
            c->presnap_height = c->height;
            c->presnap_maximized = was_maximized;
        }

        uint16_t scr_w, scr_h;
        screen_get_size(conn, screen, &scr_w, &scr_h);

        uint16_t half_w = (uint16_t)(scr_w / 2);
        uint16_t half_h = (uint16_t)(scr_h / 2);

        int16_t  nx = 0, ny = 0;
        uint16_t nw = scr_w, nh = scr_h;

        if (mask & SNAP_LEFT)       { nx = 0;                    nw = half_w; }
        else if (mask & SNAP_RIGHT) { nx = (int16_t)half_w;      nw = (uint16_t)(scr_w - half_w); }

        if (mask & SNAP_TOP)        { ny = 0;                    nh = half_h; }
        else if (mask & SNAP_BOTTOM){ ny = (int16_t)half_h;      nh = (uint16_t)(scr_h - half_h); }

        c->x = nx;
        c->y = ny;
        c->width = nw;
        /* Снэпнутое окно без titlebar — вся высота региона отдаётся
           клиентской области, tb не вычитается. */
        c->height = nh;
        c->snapped = mask;
    }

    /* Снэпнутое окно рисуется без titlebar (как fullscreen): frame и
       клиентская область занимают весь регион снэппинга. Открепление
       (mask==0) возвращает и c->snapped=0, и titlebar. */
    uint16_t frame_h = c->snapped ? c->height : (uint16_t)(c->height + tb);
    uint16_t child_y = c->snapped ? 0 : tb;

    uint32_t frame_values[4] = {
        (uint32_t)c->x, (uint32_t)c->y, c->width, frame_h
    };
    xcb_configure_window(conn, c->frame,
        XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
        XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, frame_values);

    uint32_t child_pos[2] = { 0, child_y };
    xcb_configure_window(conn, c->win,
        XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, child_pos);
    uint32_t child_size[2] = { c->width, c->height };
    xcb_configure_window(conn, c->win,
        XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, child_size);

    xcb_flush(conn);
    if (!c->snapped) window_draw_decoration(conn, screen, c);
}
