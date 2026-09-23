#ifndef MYWM_ICONS_SVG_H
#define MYWM_ICONS_SVG_H

#include <xcb/xcb.h>
#include <xcb/render.h>

/* Инициализирует контекст, используемый icons_svg_render() (соединение,
   экран, PICTFORMAT ARGB32). Вызывается из icons_init(). */
void icons_svg_init(xcb_connection_t *conn, xcb_screen_t *screen,
                     xcb_render_pictformat_t argb32_format);

/* Рендерит SVG-файл path в ARGB32 pixmap+Picture size×size через nanosvg
   (пункт 7 ТЗ). Возвращает 1 при успехе. */
int icons_svg_render(const char *path, int size,
                      xcb_pixmap_t *out_pixmap, xcb_render_picture_t *out_picture);

#endif
