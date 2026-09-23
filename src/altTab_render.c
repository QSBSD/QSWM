#include <xcb/xcb.h>
#include <xcb/render.h>
#include "altTab_render.h"
#include "altTab.h"
#include "window.h"
#include "icons.h"
#include "xrender_util.h"

static int alttab_cell_size = ALTTAB_CELL_SIZE_DEFAULT;

void alttab_render_set_cell_size(int size) {
    alttab_cell_size = (size > 0) ? size : ALTTAB_CELL_SIZE_DEFAULT;
}

int alttab_render_cell_size(void) {
    return alttab_cell_size;
}

void alttab_render_grid_size(int item_count, int *out_rows, int *out_cols) {
    int rows = item_count < ALTTAB_MAX_ROWS ? item_count : ALTTAB_MAX_ROWS;
    if (rows < 1) rows = 1;
    int cols = (item_count + rows - 1) / rows;
    *out_rows = rows;
    *out_cols = cols;
}

/* Прямоугольник ячейки с индексом idx внутри сетки. Высота строки —
   alttab_render_cell_size(). */
static void cell_rect(int idx, int rows, int *out_x, int *out_y) {
    int col = idx / rows;
    int row = idx % rows;
    int size = alttab_cell_size;
    *out_x = ALTTAB_CELL_GAP + col * (size + ALTTAB_CELL_GAP);
    *out_y = ALTTAB_CELL_GAP + row * (size + ALTTAB_CELL_GAP);
}

void alttab_render_draw(xcb_connection_t *conn, xcb_render_picture_t overlay_picture,
                         uint16_t overlay_w, uint16_t overlay_h,
                         const xcb_window_t *snapshot, int snapshot_count, int selected) {
    if (overlay_picture == XCB_NONE) return;

    /* Фон оверлея — полностью непрозрачный [appearance].alttab_bg_color
       (чёрный по умолчанию). Окно изначально имеет BackPixel=
       screen->black_pixel, а PictOpOver блендит новую заливку с тем,
       что в буфере окна уже есть, — при повторных перерисовках (Tab
       внутри одной сессии) это фактический фон предыдущего кадра, так
       что итоговый цвет остаётся стабильным. */
    xcb_render_color_t bg = alttab_bg_color();
    xcb_rectangle_t bg_rect = { 0, 0, overlay_w, overlay_h };
    xcb_render_fill_rectangles(conn, XCB_RENDER_PICT_OP_OVER, overlay_picture, bg, 1, &bg_rect);

    int rows, cols;
    alttab_render_grid_size(snapshot_count, &rows, &cols);
    (void)cols;

    for (int i = 0; i < snapshot_count; i++) {
        client_t *c = client_find_by_window(snapshot[i]);
        if (!c) continue; /* окно закрыли кликом мыши, пока оверлей открыт */

        int x, y;
        cell_rect(i, rows, &x, &y);

        if (i == selected) {
            /* Контур, а не сплошная заливка: раньше здесь рисовался белый
               прямоугольник под иконкой (x-3,y-3, size+6), и там, где SVG
               прозрачный, сквозь него было видно белую заливку -
               выделение выглядело как "залито белым", а не как рамка.
               Рисуем только внешнее кольцо толщиной ALTTAB_SEL_BORDER —
               четыре тонкие полосы по периметру ячейки, внутренняя область
               остаётся фоном оверлея, и иконка поверх ничем не перекрыта. */
            xcb_render_color_t white = xrender_color_rgba(1.0, 1.0, 1.0, 1.0);
            int16_t bx = (int16_t)(x - ALTTAB_SEL_BORDER);
            int16_t by = (int16_t)(y - ALTTAB_SEL_BORDER);
            uint16_t bs = (uint16_t)(alttab_cell_size + 2 * ALTTAB_SEL_BORDER);
            xcb_rectangle_t border_rects[4] = {
                { bx, by, bs, ALTTAB_SEL_BORDER },                                   /* верх */
                { bx, (int16_t)(by + bs - ALTTAB_SEL_BORDER), bs, ALTTAB_SEL_BORDER }, /* низ */
                { bx, by, ALTTAB_SEL_BORDER, bs },                                   /* лево */
                { (int16_t)(bx + bs - ALTTAB_SEL_BORDER), by, ALTTAB_SEL_BORDER, bs } /* право */
            };
            xcb_render_fill_rectangles(conn, XCB_RENDER_PICT_OP_OVER, overlay_picture, white, 4, border_rects);
        }

        /* Иконка приложения (пункт 7): SVG через nanosvg/XRender (icons.c),
           найденная по WM_CLASS окна. Если не найдена — заглушка. */
        if (!icons_draw(overlay_picture, c->app_class, (int16_t)x, (int16_t)y, (uint16_t)alttab_cell_size)) {
            xcb_render_color_t fallback = xrender_color_rgba(0.30, 0.30, 0.34, 1.0);
            xcb_rectangle_t icon_rect = { (int16_t)x, (int16_t)y, (uint16_t)alttab_cell_size, (uint16_t)alttab_cell_size };
            xcb_render_fill_rectangles(conn, XCB_RENDER_PICT_OP_OVER, overlay_picture, fallback, 1, &icon_rect);
        }
    }

    xcb_flush(conn);
}
