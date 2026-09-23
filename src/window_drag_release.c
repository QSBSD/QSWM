#include <xcb/xcb.h>
#include "window.h"
#include "window_drag_priv.h"

void window_button_release(xcb_connection_t *conn, xcb_screen_t *screen, xcb_button_release_event_t *e) {
    (void)e;
    if (g_window_drag.mode == DRAG_NONE) return;

    client_t *c = g_window_drag.client;

    xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
    xcb_flush(conn);

    g_window_drag.mode = DRAG_NONE;
    g_window_drag.client = NULL;
    g_window_drag.edges = EDGE_NONE;

    /* при resize ширина titlebar изменилась — перерисовываем декорацию
       с актуальным размером и положением кнопок */
    if (c) window_draw_decoration(conn, screen, c);
}
