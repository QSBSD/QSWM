#include "window_drag_priv.h"
#include "window_drag.h"

/* см. window_drag_priv.h — состояние текущего drag'а (move или resize),
   общее для window_button_press / window_motion_notify /
   window_button_release (пункт 5) */
drag_t g_window_drag = { DRAG_NONE, NULL, EDGE_NONE, 0, 0, 0, 0, 0, 0 };

/* Клиент уничтожается (window_unmanage/window_withdraw) — если он прямо
   сейчас "в руках" у активного move/resize, обнуляем g_window_drag, иначе
   g_window_drag.client останется висячим указателем (use-after-free в
   window_motion_notify()/window_button_release()). */
void window_drag_client_removed(xcb_connection_t *conn, client_t *removed_client) {
    if (g_window_drag.client != removed_client) return;

    if (g_window_drag.mode != DRAG_NONE) {
        xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
        xcb_flush(conn);
    }
    g_window_drag.mode = DRAG_NONE;
    g_window_drag.client = NULL;
    g_window_drag.edges = EDGE_NONE;
}
