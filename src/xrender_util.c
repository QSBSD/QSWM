#include <stddef.h>
#include <stdlib.h>
#include "xrender_util.h"

xcb_render_pictformat_t xrender_find_format_for_visual(xcb_connection_t *conn,
                                                        xcb_visualid_t visual) {
    xcb_render_query_pict_formats_cookie_t cookie = xcb_render_query_pict_formats(conn);
    xcb_render_query_pict_formats_reply_t *reply =
        xcb_render_query_pict_formats_reply(conn, cookie, NULL);
    if (!reply) return 0;

    xcb_render_pictformat_t result = 0;
    xcb_render_pictscreen_iterator_t sit =
        xcb_render_query_pict_formats_screens_iterator(reply);
    for (; sit.rem; xcb_render_pictscreen_next(&sit)) {
        xcb_render_pictdepth_iterator_t dit =
            xcb_render_pictscreen_depths_iterator(sit.data);
        for (; dit.rem; xcb_render_pictdepth_next(&dit)) {
            xcb_render_pictvisual_iterator_t vit =
                xcb_render_pictdepth_visuals_iterator(dit.data);
            for (; vit.rem; xcb_render_pictvisual_next(&vit)) {
                if (vit.data->visual == visual) {
                    result = vit.data->format;
                }
            }
        }
    }

    free(reply);
    return result;
}

xcb_render_pictformat_t xrender_find_argb32_format(xcb_connection_t *conn) {
    xcb_render_query_pict_formats_cookie_t cookie = xcb_render_query_pict_formats(conn);
    xcb_render_query_pict_formats_reply_t *reply =
        xcb_render_query_pict_formats_reply(conn, cookie, NULL);
    if (!reply) return 0;

    xcb_render_pictformat_t result = 0;
    xcb_render_pictforminfo_iterator_t it =
        xcb_render_query_pict_formats_formats_iterator(reply);
    for (; it.rem; xcb_render_pictforminfo_next(&it)) {
        xcb_render_pictforminfo_t *f = it.data;
        if (f->type == XCB_RENDER_PICT_TYPE_DIRECT && f->depth == 32 &&
            f->direct.red_shift == 16   && f->direct.red_mask   == 0xff &&
            f->direct.green_shift == 8  && f->direct.green_mask == 0xff &&
            f->direct.blue_shift == 0   && f->direct.blue_mask  == 0xff &&
            f->direct.alpha_shift == 24 && f->direct.alpha_mask == 0xff) {
            result = f->id;
            break;
        }
    }

    free(reply);
    return result;
}

xcb_render_picture_t xrender_create_picture_for_visual(xcb_connection_t *conn,
                                                        xcb_drawable_t drawable,
                                                        xcb_visualid_t visual) {
    xcb_render_pictformat_t fmt = xrender_find_format_for_visual(conn, visual);
    if (!fmt) return XCB_NONE;

    xcb_render_picture_t picture = xcb_generate_id(conn);
    xcb_render_create_picture(conn, picture, drawable, fmt, 0, NULL);
    return picture;
}

xcb_render_color_t xrender_color_rgba(double r, double g, double b, double a) {
    xcb_render_color_t c;
    c.red   = (uint16_t)(r * 65535.0 + 0.5);
    c.green = (uint16_t)(g * 65535.0 + 0.5);
    c.blue  = (uint16_t)(b * 65535.0 + 0.5);
    c.alpha = (uint16_t)(a * 65535.0 + 0.5);
    return c;
}
