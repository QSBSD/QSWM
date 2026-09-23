#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <xcb/xcb.h>
#include "window.h"

/* Раскладка WM_SIZE_HINTS (ICCCM 4.1.2.3), как её кладёт Xlib/toolkit-и в
   свойство WM_NORMAL_HINTS: 18 полей по 4 байта, все int32_t кроме flags
   и win_gravity (uint32_t) - для наших целей знак не важен, читаем как
   int32_t и приводим к uint16_t только положительные разумные значения.
   Ручной разбор вместо линковки xcb-icccm - это единственное поле хинтов,
   которое здесь нужно, заводить лишнюю библиотечную зависимость ради
   него не стоило. */
#define HINTS_FLAG_MIN_SIZE 16  /* PMinSize */
#define HINTS_FLAG_MAX_SIZE 32  /* PMaxSize */

#define HINTS_OFF_FLAGS      0
#define HINTS_OFF_MIN_WIDTH  20
#define HINTS_OFF_MIN_HEIGHT 24
#define HINTS_OFF_MAX_WIDTH  28
#define HINTS_OFF_MAX_HEIGHT 32
#define HINTS_PROP_LEN_32BIT 18 /* длина всей структуры в 4-байтных словах */

static int32_t read_i32(const uint8_t *base, int offset_bytes) {
    int32_t v;
    memcpy(&v, base + offset_bytes, sizeof(v));
    return v;
}

/* Заполняет c->min_width/min_height/max_width/max_height из WM_NORMAL_HINTS
   окна c->win. Если свойство отсутствует или клиент не выставил
   PMinSize/PMaxSize - оставляет соответствующее поле как есть (min уже
   инициализирован на MIN_WIDTH/MIN_HEIGHT вызывающей стороной, max == 0
   означает "нет верхнего предела"). Вызывается один раз из
   window_manage(). */
void window_fetch_normal_hints(xcb_connection_t *conn, client_t *c) {
    xcb_get_property_cookie_t cookie = xcb_get_property(
        conn, 0, c->win, XCB_ATOM_WM_NORMAL_HINTS, XCB_ATOM_WM_SIZE_HINTS,
        0, HINTS_PROP_LEN_32BIT);
    xcb_get_property_reply_t *reply = xcb_get_property_reply(conn, cookie, NULL);
    if (!reply) return;

    int len = xcb_get_property_value_length(reply);
    if (reply->format != 32 || len < HINTS_OFF_MAX_HEIGHT + 4) {
        free(reply);
        return; /* свойства нет, либо оно короче, чем нужные нам поля */
    }

    const uint8_t *data = xcb_get_property_value(reply);
    int32_t flags = read_i32(data, HINTS_OFF_FLAGS);

    if (flags & HINTS_FLAG_MIN_SIZE) {
        int32_t mw = read_i32(data, HINTS_OFF_MIN_WIDTH);
        int32_t mh = read_i32(data, HINTS_OFF_MIN_HEIGHT);
        if (mw > c->min_width && mw <= UINT16_MAX) c->min_width = (uint16_t)mw;
        if (mh > c->min_height && mh <= UINT16_MAX) c->min_height = (uint16_t)mh;
    }
    if (flags & HINTS_FLAG_MAX_SIZE) {
        int32_t xw = read_i32(data, HINTS_OFF_MAX_WIDTH);
        int32_t xh = read_i32(data, HINTS_OFF_MAX_HEIGHT);
        /* max < min по вине самого клиента - в этом случае лучше не
           навязывать верхнюю границу вообще, чем зажать окно в
           противоречивый диапазон. */
        if (xw > 0 && xw <= UINT16_MAX && xw >= c->min_width) c->max_width = (uint16_t)xw;
        if (xh > 0 && xh <= UINT16_MAX && xh >= c->min_height) c->max_height = (uint16_t)xh;
    }

    free(reply);
}

void window_clamp_size(const client_t *c, uint16_t *w, uint16_t *h) {
    if (*w < c->min_width) *w = c->min_width;
    if (*h < c->min_height) *h = c->min_height;
    if (c->max_width  && *w > c->max_width)  *w = c->max_width;
    if (c->max_height && *h > c->max_height) *h = c->max_height;
}
