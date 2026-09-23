#ifndef MYWM_XRENDER_UTIL_H
#define MYWM_XRENDER_UTIL_H

#include <xcb/xcb.h>
#include <xcb/render.h>

/* PICTFORMAT, совместимый с данным visual экрана — нужен, чтобы создать
   Picture прямо поверх уже существующего drawable (окна с этим
   visual'ом), см. xrender_create_picture_for_visual() ниже. Возвращает
   0, если подходящий формат не найден. */
xcb_render_pictformat_t xrender_find_format_for_visual(xcb_connection_t *conn,
                                                        xcb_visualid_t visual);

/* Стандартный 32-битный ARGB-формат (8/8/8/8, alpha в старшем байте,
   premultiplied по соглашению XRender) — для Picture поверх pixmap с
   растеризованными SVG-иконками (см. icons.c). Возвращает 0, если
   сервер его не поддерживает. */
xcb_render_pictformat_t xrender_find_argb32_format(xcb_connection_t *conn);

/* Находит формат под visual и создаёт Picture прямо на drawable (обычно
   окно) с этим visual'ом — типовая пара вызовов, нужная и для titlebar
   (window.c), и для оверлея alt-tab (altTab.c). Возвращает XCB_NONE,
   если подходящий формат не найден; id самой Picture генерируется
   внутри и возвращается вызывающему для последующего
   xcb_render_free_picture(). */
xcb_render_picture_t xrender_create_picture_for_visual(xcb_connection_t *conn,
                                                        xcb_drawable_t drawable,
                                                        xcb_visualid_t visual);

/* Переводит цвет в формате с плавающей точкой [0.0, 1.0] в
   xcb_render_color_t (компоненты 0..65535, которого ждёт RENDER).
   Значения вне диапазона не проверяются — вызывающий код передаёт
   только литеральные константы. */
xcb_render_color_t xrender_color_rgba(double r, double g, double b, double a);

#endif
