#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <xcb/xcb.h>

/* SVG рендерится через nanosvg/nanosvgrast (однозаголовочные библиотеки,
   перенесены из ar_simplified_xcb-3/svgrender.c) вместо librsvg — без
   зависимости от glib/gdk-pixbuf. NANOSVG*_IMPLEMENTATION определяются
   ровно один раз, здесь. */
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

#include "icons_svg.h"

static xcb_connection_t       *g_conn   = NULL;
static xcb_screen_t           *g_screen = NULL;
static xcb_render_pictformat_t g_argb32_format = 0;

void icons_svg_init(xcb_connection_t *conn, xcb_screen_t *screen,
                     xcb_render_pictformat_t argb32_format) {
    g_conn = conn;
    g_screen = screen;
    g_argb32_format = argb32_format;
}

/* Растеризует уже распарсенный NSVGimage в квадратный ARGB32-буфер
   size×size (вписывая с сохранением пропорций и центрированием, как в
   svgrender.c). nsvgRasterize пишет прямой (не премультиплицированный)
   RGBA прямо в буфер; после проходим по нему и премультиплицируем
   альфой, переставляя байты в порядок, который PICTFORMAT ARGB32 ждёт
   на little-endian (B,G,R,A: 32-битное слово 0xAARRGGBB в памяти
   младшим байтом вперёд). Возвращает malloc'нутый буфер, который
   вызывающий код обязан освободить через free(). */
static unsigned char *rasterize_nsvg_image(NSVGimage *image, int size, int *out_stride) {
    if (!image || image->width <= 0 || image->height <= 0) return NULL;

    NSVGrasterizer *rast = nsvgCreateRasterizer();
    if (!rast) return NULL;

    int stride = size * 4;
    unsigned char *data = calloc((size_t)stride, (size_t)size);
    if (!data) {
        nsvgDeleteRasterizer(rast);
        return NULL;
    }

    float scale = (float)size / (image->width > image->height ? image->width : image->height);
    float tx = (size / scale - image->width) / 2.0f;
    float ty = (size / scale - image->height) / 2.0f;

    nsvgRasterize(rast, image, tx, ty, scale, data, size, size, stride);
    nsvgDeleteRasterizer(rast);

    for (int y = 0; y < size; y++) {
        unsigned char *row = data + (size_t)y * stride;
        for (int x = 0; x < size; x++) {
            unsigned char *px = row + (size_t)x * 4;
            unsigned char r = px[0], g = px[1], b = px[2], a = px[3];
            px[0] = (unsigned char)((b * a) / 255);
            px[1] = (unsigned char)((g * a) / 255);
            px[2] = (unsigned char)((r * a) / 255);
            px[3] = a;
        }
    }

    *out_stride = stride;
    return data;
}

/* Заливает ARGB32-буфер data (size×size, premultiplied, B,G,R,A) в
   новый offscreen pixmap глубины 32 и оборачивает его в XRender Picture
   формата g_argb32_format, готовую для xcb_render_composite(). */
static int create_pixmap_from_argb(const unsigned char *data, int stride, int size,
                                    xcb_pixmap_t *out_pixmap, xcb_render_picture_t *out_picture) {
    if (!g_argb32_format) return 0;

    xcb_pixmap_t pixmap = xcb_generate_id(g_conn);
    xcb_create_pixmap(g_conn, 32, pixmap, g_screen->root, (uint16_t)size, (uint16_t)size);

    xcb_gcontext_t gc = xcb_generate_id(g_conn);
    xcb_create_gc(g_conn, gc, pixmap, 0, NULL);
    xcb_put_image(g_conn, XCB_IMAGE_FORMAT_Z_PIXMAP, pixmap, gc,
                  (uint16_t)size, (uint16_t)size, 0, 0, 0, 32,
                  (uint32_t)stride * (uint32_t)size, data);
    xcb_free_gc(g_conn, gc);

    xcb_render_picture_t picture = xcb_generate_id(g_conn);
    xcb_render_create_picture(g_conn, picture, pixmap, g_argb32_format, 0, NULL);

    *out_pixmap = pixmap;
    *out_picture = picture;
    return 1;
}

int icons_svg_render(const char *path, int size,
                      xcb_pixmap_t *out_pixmap, xcb_render_picture_t *out_picture) {
    NSVGimage *image = nsvgParseFromFile(path, "px", 96.0f);
    if (!image) return 0;

    int stride = 0;
    unsigned char *data = rasterize_nsvg_image(image, size, &stride);
    nsvgDelete(image);
    if (!data) return 0;

    int ok = create_pixmap_from_argb(data, stride, size, out_pixmap, out_picture);
    free(data);
    return ok;
}
