#include <stdlib.h>
#include <string.h>
#include "wm_state.h"

static xcb_atom_t intern_atom(xcb_connection_t *conn, const char *name) {
    xcb_intern_atom_cookie_t cookie =
        xcb_intern_atom(conn, 0, (uint16_t)strlen(name), name);
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn, cookie, NULL);
    xcb_atom_t atom = reply ? reply->atom : XCB_ATOM_NONE;
    free(reply);
    return atom;
}

static xcb_atom_t atom_wm_state = XCB_ATOM_NONE;

void wm_state_set(xcb_connection_t *conn, xcb_window_t win, uint32_t state) {
    if (atom_wm_state == XCB_ATOM_NONE)
        atom_wm_state = intern_atom(conn, "WM_STATE");
    if (atom_wm_state == XCB_ATOM_NONE) return;

    uint32_t data[2] = { state, XCB_NONE };
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win, atom_wm_state,
                         atom_wm_state, 32, 2, data);
}
