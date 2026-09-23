#ifndef MYWM_WINDOW_DRAG_PRIV_H
#define MYWM_WINDOW_DRAG_PRIV_H

#include <xcb/xcb.h>
#include "window.h"

/* Внутреннее состояние drag'а (move/resize), общее для
   window_drag_press.c / window_drag_motion.c / window_drag_release.c.
   Не выносится в window_drag.h - это деталь реализации, а не публичный API. */

typedef enum { DRAG_NONE, DRAG_MOVE, DRAG_RESIZE } drag_mode_t;

typedef enum {
    EDGE_NONE   = 0,
    EDGE_LEFT   = 1 << 0,
    EDGE_RIGHT  = 1 << 1,
    EDGE_BOTTOM = 1 << 2
} resize_edge_t;

typedef struct {
    drag_mode_t  mode;
    client_t    *client;
    int          edges;
    int16_t      pointer_start_x, pointer_start_y; /* координаты в root */
    int16_t      frame_start_x, frame_start_y;
    uint16_t     frame_start_w, frame_start_h;      /* клиентская область */
} drag_t;

extern drag_t g_window_drag;

#endif
