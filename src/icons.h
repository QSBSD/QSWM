#ifndef MYWM_ICONS_H
#define MYWM_ICONS_H

#include <xcb/xcb.h>
#include <xcb/render.h>

/* Инициализация кэша иконок и поиска ARGB32 PICTFORMAT (см.
   xrender_util.c). Вызывается один раз при старте WM, до первого
   window_manage()/alttab_draw(). */
void icons_init(xcb_connection_t *conn, xcb_screen_t *screen);

/* Каталог из [icons].dir config.conf: для ЛЮБОГО app_class сначала
   ищется "{dir}/{app_class}.svg" (и вариант в нижнем регистре), раньше
   системных icon-тем. dir копируется по значению; dir == NULL или "" -
   проверка каталога отключается. */
void icons_set_custom_dir(const char *dir);

/* Рисует SVG-иконку приложения app_class (обычно WM_CLASS окна) в
   Picture dst, вписывая её в квадрат [x, y, size, size]. Сначала
   проверяется каталог из icons_set_custom_dir() ([icons].dir в
   config.conf); если там иконки нет, ищется на диске в стандартных
   путях icon-тем (hicolor, Adwaita, breeze, pixmaps). Рендерится через
   nanosvg/nanosvgrast в ARGB32-буфер нужного размера, заливается в
   offscreen pixmap и компонуется в dst через xcb_render_composite()
   (PictOpOver). Результат (pixmap + Picture) кэшируется по паре (имя,
   размер) — включая отрицательный результат, чтобы не бить по диску
   повторно при каждой перерисовке.

   Возвращает 1, если иконка найдена и отрисована. Возвращает 0, если
   подходящий SVG не найден — в этом случае вызывающий код (window.c /
   altTab.c) сам рисует запасную заглушку-квадрат. */
int icons_draw(xcb_render_picture_t dst, const char *app_class,
               int16_t x, int16_t y, uint16_t size);

/* Глифы кнопок декорации ("− □ ×"). Это простая геометрия (линия,
   прямоугольник, две диагонали), поэтому рисуются напрямую примитивами
   XCB core rendering (xcb_poly_line/xcb_poly_rectangle) в drawable —
   без XRender и без текста/шрифтов, в отличие от icons_draw() выше,
   который рендерит SVG-иконки приложений через nanosvg в offscreen
   pixmap и компонует их через RENDER. */
typedef enum {
    ICON_BTN_MINIMIZE,
    ICON_BTN_MAXIMIZE,
    ICON_BTN_CLOSE
} icon_button_t;

/* Рисует глиф кнопки which поверх drawable (окно или pixmap), центрируя
   его в квадрате [x, y, size, size]. conn/drawable — то, во что рисуем;
   функция сама создаёт и уничтожает временный GC цвета #d9d9e0.
   Возвращает 1 при успехе, 0 при недопустимом which или size. */
int icons_draw_button(xcb_connection_t *conn, xcb_drawable_t drawable,
                       icon_button_t which, int16_t x, int16_t y, uint16_t size);

/* Освобождает кэш иконок (все Picture и pixmap XRender). Вызывается при
   завершении работы WM. */
void icons_cleanup(void);

#endif
