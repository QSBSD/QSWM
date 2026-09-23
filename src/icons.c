#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xcb/xcb.h>

#include "icons.h"
#include "icons_svg.h"
#include "xrender_util.h"

#define ICON_NAME_MAX 80

/* Цвет глифов кнопок декорации ("− □ ×") — используется и в icons_init()
   (создание общего GC), и в icons_draw_button_glyph() ниже. */
#define GLYPH_COLOR_PIXEL 0xd9d9e0u

/* Запись кэша: один pixmap+Picture на пару (имя класса окна, размер в
   px). pixmap/picture == XCB_NONE — тоже валидная запись: означает "для
   этого имени и размера SVG-файл на диске не найден", чтобы не искать
   его повторно на каждой перерисовке декорации/оверлея. */
typedef struct icon_entry {
    char                  name[ICON_NAME_MAX];
    int                   size;
    xcb_pixmap_t          pixmap;
    xcb_render_picture_t  picture;
    struct icon_entry    *next;
} icon_entry_t;

static icon_entry_t *cache = NULL;

static xcb_connection_t *g_conn = NULL;

/* [icons].dir - см. icons_set_custom_dir() ниже. */
static char g_custom_dir[256] = "";

void icons_set_custom_dir(const char *dir) {
    if (!dir) dir = "";
    strncpy(g_custom_dir, dir, sizeof(g_custom_dir) - 1);
    g_custom_dir[sizeof(g_custom_dir) - 1] = '\0';
}

/* Ищет "{g_custom_dir}/{app_class}.svg" и вариант в нижнем регистре -
   определена ниже to_lower_copy(), см. там. */
static int find_custom_dir_path(const char *app_class, char *path, size_t pathsz);

/* GC для глифов кнопок декорации (icons_draw_button) — раньше создавался
   и уничтожался (2 round-trip'а к X-серверу) на КАЖДЫЙ вызов, а вызывается
   он на каждую перерисовку декорации (Expose/resize/drag-release), то
   есть многократно в секунду при интерактивном ресайзе. GC не привязан
   жёстко к конкретному drawable — его можно использовать с любым
   drawable той же глубины и того же root (X11 Protocol, раздел
   "Graphics Contexts"), поэтому один GC, созданный один раз против
   screen->root, годится для отрисовки на любом frame, т.к. все frame
   создаются с тем же screen->root_visual/той же глубиной. */
static xcb_gcontext_t g_button_gc = XCB_NONE;

void icons_init(xcb_connection_t *conn, xcb_screen_t *screen) {
    cache = NULL;
    g_custom_dir[0] = '\0';
    g_conn = conn;
    icons_svg_init(conn, screen, xrender_find_argb32_format(conn));

    g_button_gc = xcb_generate_id(conn);
    uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_LINE_WIDTH;
    uint32_t values[2] = { GLYPH_COLOR_PIXEL, 2 };
    xcb_create_gc(conn, g_button_gc, screen->root, mask, values);
}

static void to_lower_copy(const char *src, char *dst, size_t dstsz) {
    size_t i = 0;
    for (; src[i] && i + 1 < dstsz; i++)
        dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = '\0';
}

/* Ищет "{g_custom_dir}/{app_class}.svg" и вариант в нижнем регистре -
   пользовательский каталог из [icons].dir, проверяется раньше системных
   icon-тем (find_icon_path()). Возвращает 1 и заполняет path, если файл
   реально существует на диске; 0 - каталог не задан или файла там нет. */
static int find_custom_dir_path(const char *app_class, char *path, size_t pathsz) {
    if (!g_custom_dir[0] || !app_class || !app_class[0]) return 0;

    snprintf(path, pathsz, "%s/%s.svg", g_custom_dir, app_class);
    if (access(path, F_OK) == 0) return 1;

    char lower[ICON_NAME_MAX];
    to_lower_copy(app_class, lower, sizeof(lower));
    snprintf(path, pathsz, "%s/%s.svg", g_custom_dir, lower);
    if (access(path, F_OK) == 0) return 1;

    return 0;
}

/* Ищет .svg иконку приложения по имени класса окна (WM_CLASS) в
   стандартных путях icon-тем. Пробует имя как есть и в нижнем регистре,
   т.к. WM_CLASS обычно "CamelCase" (Firefox), а файлы иконок — в
   нижнем регистре (firefox.svg).

   НЕИСПРАВНОСТЬ: раньше здесь были только Linux-пути /usr/share/...
   На FreeBSD иконки, поставленные через pkg/ports, лежат под
   /usr/local/share/... — /usr/share/ там принадлежит базовой системе
   и icon-тем не содержит вовсе. В результате абсолютно ни один путь
   не совпадал, find_icon_path() всегда возвращал 0, и для любого окна
   рисовалась заглушка-квадрат вместо иконки. /usr/local теперь идёт
   первым (актуально для FreeBSD), Linux-пути остаются следом. */
static int find_icon_path(const char *app_class, char *path, size_t pathsz) {
    if (!app_class || !app_class[0]) return 0;

    char lower[ICON_NAME_MAX];
    to_lower_copy(app_class, lower, sizeof(lower));

    static const char *templates[] = {
        "/usr/local/share/icons/hicolor/scalable/apps/%s.svg",
        "/usr/local/share/icons/hicolor/scalable/apps/%s-symbolic.svg",
        "/usr/local/share/icons/Adwaita/scalable/apps/%s.svg",
        "/usr/local/share/icons/Adwaita/scalable/apps/%s-symbolic.svg",
        "/usr/local/share/icons/breeze/apps/48/%s.svg",
        "/usr/local/share/icons/hicolor/48x48/apps/%s.svg",
        "/usr/local/share/pixmaps/%s.svg",
        "/usr/share/icons/hicolor/scalable/apps/%s.svg",
        "/usr/share/icons/hicolor/scalable/apps/%s-symbolic.svg",
        "/usr/share/icons/Adwaita/scalable/apps/%s.svg",
        "/usr/share/icons/breeze/apps/48/%s.svg",
        "/usr/share/pixmaps/%s.svg",
    };

    for (size_t i = 0; i < sizeof(templates) / sizeof(templates[0]); i++) {
        snprintf(path, pathsz, templates[i], app_class);
        if (access(path, F_OK) == 0) return 1;

        snprintf(path, pathsz, templates[i], lower);
        if (access(path, F_OK) == 0) return 1;
    }

    /* Иконки в домашнем каталоге пользователя (например, из flatpak или
       темы, поставленной вручную) — на них /usr/local/share не
       распространяется, поэтому отдельный проход. */
    const char *home = getenv("HOME");
    if (home && home[0]) {
        static const char *home_templates[] = {
            "%s/.local/share/icons/hicolor/scalable/apps/%s.svg",
            "%s/.icons/hicolor/scalable/apps/%s.svg",
        };
        for (size_t i = 0; i < sizeof(home_templates) / sizeof(home_templates[0]); i++) {
            snprintf(path, pathsz, home_templates[i], home, app_class);
            if (access(path, F_OK) == 0) return 1;

            snprintf(path, pathsz, home_templates[i], home, lower);
            if (access(path, F_OK) == 0) return 1;
        }
    }

    return 0;
}

static icon_entry_t *cache_lookup(const char *app_class, int size, int *found) {
    for (icon_entry_t *e = cache; e; e = e->next) {
        if (e->size == size && strncmp(e->name, app_class, ICON_NAME_MAX) == 0) {
            *found = 1;
            return e;
        }
    }
    *found = 0;
    return NULL;
}

static icon_entry_t *cache_store(const char *app_class, int size,
                                  xcb_pixmap_t pixmap, xcb_render_picture_t picture) {
    icon_entry_t *e = calloc(1, sizeof(icon_entry_t));
    if (!e) return NULL;
    strncpy(e->name, app_class, sizeof(e->name) - 1);
    e->size = size;
    e->pixmap = pixmap;
    e->picture = picture;
    e->next = cache;
    cache = e;
    return e;
}

int icons_draw(xcb_render_picture_t dst, const char *app_class,
               int16_t x, int16_t y, uint16_t size) {
    if (!app_class || !app_class[0] || !g_conn) return 0;
    if (size == 0) return 0;

    int found = 0;
    icon_entry_t *e = cache_lookup(app_class, size, &found);

    if (!found) {
        char path[512];
        xcb_pixmap_t pixmap = XCB_NONE;
        xcb_render_picture_t picture = XCB_NONE;
        if (find_custom_dir_path(app_class, path, sizeof(path))) {
            icons_svg_render(path, size, &pixmap, &picture);
        } else if (find_icon_path(app_class, path, sizeof(path))) {
            icons_svg_render(path, size, &pixmap, &picture);
        }
        e = cache_store(app_class, size, pixmap, picture);
    }

    if (!e || e->picture == XCB_NONE) return 0;

    xcb_render_composite(g_conn, XCB_RENDER_PICT_OP_OVER, e->picture, XCB_NONE, dst,
                          0, 0, 0, 0, x, y, size, size);
    return 1;
}

/* Глифы кнопок декорации — "− □ ×" геометрически простые (одна линия,
   прямоугольник, две диагонали), поэтому рисуются напрямую примитивами
   XCB core rendering, без XRender и без шрифтов/текста вообще. Отступ
   INSET от краёв квадрата [x,y,size,size] — чтобы глиф не упирался в
   рамку кнопки. */

static int icons_draw_button_glyph(xcb_connection_t *conn, xcb_drawable_t drawable,
                                    xcb_gcontext_t gc, icon_button_t which,
                                    int16_t x, int16_t y, uint16_t size) {
    uint16_t inset = (uint16_t)(size / 4);
    int16_t  x0 = (int16_t)(x + inset);
    int16_t  y0 = (int16_t)(y + inset);
    int16_t  x1 = (int16_t)(x + size - inset);
    int16_t  y1 = (int16_t)(y + size - inset);

    switch (which) {
    case ICON_BTN_MINIMIZE: {
        int16_t ym = (int16_t)(y + size / 2);
        xcb_point_t pts[2] = { { x0, ym }, { x1, ym } };
        xcb_poly_line(conn, XCB_COORD_MODE_ORIGIN, drawable, gc, 2, pts);
        return 1;
    }
    case ICON_BTN_MAXIMIZE: {
        xcb_rectangle_t rect = { x0, y0, (uint16_t)(x1 - x0), (uint16_t)(y1 - y0) };
        xcb_poly_rectangle(conn, drawable, gc, 1, &rect);
        return 1;
    }
    case ICON_BTN_CLOSE: {
        xcb_point_t diag1[2] = { { x0, y0 }, { x1, y1 } };
        xcb_point_t diag2[2] = { { x0, y1 }, { x1, y0 } };
        xcb_poly_line(conn, XCB_COORD_MODE_ORIGIN, drawable, gc, 2, diag1);
        xcb_poly_line(conn, XCB_COORD_MODE_ORIGIN, drawable, gc, 2, diag2);
        return 1;
    }
    default:
        return 0;
    }
}

int icons_draw_button(xcb_connection_t *conn, xcb_drawable_t drawable,
                       icon_button_t which, int16_t x, int16_t y, uint16_t size) {
    if (which < 0 || which > ICON_BTN_CLOSE) return 0;
    if (size == 0) return 0;
    if (g_button_gc == XCB_NONE) return 0; /* icons_init() ещё не вызван */

    return icons_draw_button_glyph(conn, drawable, g_button_gc, which, x, y, size);
}

void icons_cleanup(void) {
    if (g_button_gc != XCB_NONE) {
        xcb_free_gc(g_conn, g_button_gc);
        g_button_gc = XCB_NONE;
    }

    icon_entry_t *e = cache;
    while (e) {
        icon_entry_t *next = e->next;
        if (e->picture != XCB_NONE) xcb_render_free_picture(g_conn, e->picture);
        if (e->pixmap  != XCB_NONE) xcb_free_pixmap(g_conn, e->pixmap);
        free(e);
        e = next;
    }
    cache = NULL;
}
