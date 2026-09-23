#ifndef MYWM_ALTTAB_RENDER_H
#define MYWM_ALTTAB_RENDER_H

#include <xcb/xcb.h>
#include <xcb/render.h>

#define ALTTAB_CELL_SIZE_DEFAULT 64  /* размер квадрата под иконку по умолчанию */
#define ALTTAB_CELL_GAP  16  /* промежуток между ячейками и от края оверлея */
#define ALTTAB_MAX_ROWS  6   /* список идёт вертикально — до 6 в столбце, дальше перенос вправо */
#define ALTTAB_SEL_BORDER 3  /* толщина контура выделения вокруг активной ячейки, px */

/* Размер ячейки/иконки задаётся во время выполнения из
   [appearance].alttab_icon_size (config.conf) — это и есть "масштаб"
   интерфейса Alt+Tab: от него зависят overlay_w/h (altTab.c) и размер
   иконок с рамкой выделения (altTab_render.c). Вызывается один раз из
   wm_init() после config_load(), до alttab_init(); если не вызвать —
   используется ALTTAB_CELL_SIZE_DEFAULT. */
void alttab_render_set_cell_size(int size);
int  alttab_render_cell_size(void);

/* Число столбцов/строк, которые займёт сетка из item_count иконок при
   ограничении ALTTAB_MAX_ROWS строк в столбце. Нужно alttab_open() для
   расчёта размера окна оверлея. */
void alttab_render_grid_size(int item_count, int *out_rows, int *out_cols);

/* Рисует полупрозрачный фон оверлея размером overlay_w×overlay_h и
   сетку иконок snapshot[0..snapshot_count) (окно с индексом selected
   подсвечено) в Picture overlay_picture. */
void alttab_render_draw(xcb_connection_t *conn, xcb_render_picture_t overlay_picture,
                         uint16_t overlay_w, uint16_t overlay_h,
                         const xcb_window_t *snapshot, int snapshot_count, int selected);

#endif
