#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include "window.h"
#include "wm_state.h"
#include "ewmh.h"

/* Атомы WM_PROTOCOLS/WM_DELETE_WINDOW (пункт 2), интернируются один раз
   при первом обращении и переиспользуются для всех окон. */
static xcb_atom_t atom_wm_protocols     = XCB_ATOM_NONE;
static xcb_atom_t atom_wm_delete_window = XCB_ATOM_NONE;

static xcb_atom_t intern_atom(xcb_connection_t *conn, const char *name) {
    xcb_intern_atom_cookie_t cookie =
        xcb_intern_atom(conn, 0, (uint16_t)strlen(name), name);
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn, cookie, NULL);
    xcb_atom_t atom = reply ? reply->atom : XCB_ATOM_NONE;
    free(reply);
    return atom;
}

static void ensure_close_atoms(xcb_connection_t *conn) {
    if (atom_wm_protocols == XCB_ATOM_NONE)
        atom_wm_protocols = intern_atom(conn, "WM_PROTOCOLS");
    if (atom_wm_delete_window == XCB_ATOM_NONE)
        atom_wm_delete_window = intern_atom(conn, "WM_DELETE_WINDOW");
}

/* Проверяет, объявляет ли окно win поддержку WM_DELETE_WINDOW в своём
   свойстве WM_PROTOCOLS (ICCCM 4.1.2.7). */
static int window_supports_delete(xcb_connection_t *conn, xcb_window_t win) {
    ensure_close_atoms(conn);
    if (atom_wm_protocols == XCB_ATOM_NONE || atom_wm_delete_window == XCB_ATOM_NONE)
        return 0;

    xcb_get_property_cookie_t cookie = xcb_get_property(
        conn, 0, win, atom_wm_protocols, XCB_ATOM_ATOM, 0, 64);
    xcb_get_property_reply_t *reply = xcb_get_property_reply(conn, cookie, NULL);
    if (!reply) return 0;

    int supports = 0;
    if (reply->type == XCB_ATOM_ATOM && reply->format == 32) {
        xcb_atom_t *atoms = (xcb_atom_t *)xcb_get_property_value(reply);
        int count = xcb_get_property_value_length(reply) / (int)sizeof(xcb_atom_t);
        for (int i = 0; i < count; i++) {
            if (atoms[i] == atom_wm_delete_window) { supports = 1; break; }
        }
    }
    free(reply);
    return supports;
}

/* Закрывает окно — вызывается по клику на кнопку × и по хоткею close
   (пункт 6). Если клиент поддерживает ICCCM WM_DELETE_WINDOW — посылает
   ему ClientMessage (даём приложению шанс спросить подтверждение или
   сохранить данные, как в любом нормальном WM). Реальное закрытие окна
   в этом случае произойдёт по инициативе самого клиента — придёт
   DestroyNotify, который обработает window_unmanage(). Если протокол не
   поддерживается — xcb_kill_client() принудительно рвёт соединение
   клиента с X-сервером (в отличие от xcb_destroy_window, который лишь
   уничтожает одно окно и не гарантирует завершение самого процесса). */
void window_close(xcb_connection_t *conn, client_t *c) {
    if (window_supports_delete(conn, c->win)) {
        xcb_client_message_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.response_type  = XCB_CLIENT_MESSAGE;
        ev.format         = 32;
        ev.window         = c->win;
        ev.type           = atom_wm_protocols;
        ev.data.data32[0] = atom_wm_delete_window;
        ev.data.data32[1] = XCB_CURRENT_TIME;
        xcb_send_event(conn, 0, c->win, XCB_EVENT_MASK_NO_EVENT, (const char *)&ev);
    } else {
        xcb_kill_client(conn, c->win);
    }
    xcb_flush(conn);
}

/* Переключает окно между свёрнутым и обычным состоянием — вызывается по
   клику на кнопку − и по хоткею minimize (пункт 6). Свёрнутое окно
   просто не отображается (frame и клиентское окно unmap), геометрия и
   client_t (включая maximized/snapped/fullscreen) сохраняются как есть,
   чтобы восстановление вернуло окно в то же состояние, в котором оно
   было свёрнуто — раньше maximized-окно сначала требовалось restore'ить
   и только потом сворачивать. */
void window_toggle_minimize(xcb_connection_t *conn, xcb_screen_t *screen, client_t *c) {
    if (!c->minimized) {
        xcb_unmap_window(conn, c->frame);
        c->minimized = 1;
        wm_state_set(conn, c->win, WM_STATE_ICONIC);
    } else {
        xcb_map_window(conn, c->frame);
        c->minimized = 0;
        wm_state_set(conn, c->win, WM_STATE_NORMAL);
        /* titlebar физически отсутствует в fullscreen/snap — рисовать
           там нечего, как и в window_expose(). */
        if (!c->fullscreen && !c->snapped) window_draw_decoration(conn, screen, c);
    }
    ewmh_update_wm_state(conn, c);
    xcb_flush(conn);
}

/* Принудительно сворачивает клиента c, минуя проверку "maximized ->
   return" из window_toggle_minimize() — при Meta+Tab (свернуть все,
   "показать стол") нужно спрятать вообще все окна, включая
   развёрнутые/снэпнутые; их maximized/snapped флаг при этом не
   меняется, чтобы восстановление вернуло прежний режим. */
static void force_minimize(xcb_connection_t *conn, client_t *c) {
    if (c->minimized) return;
    xcb_unmap_window(conn, c->frame);
    c->minimized = 1;
    wm_state_set(conn, c->win, WM_STATE_ICONIC);
    ewmh_update_wm_state(conn, c);
}

void window_minimize_all(xcb_connection_t *conn, xcb_screen_t *screen) {
    (void)screen;
    for (client_t *c = window_get_clients(); c; c = c->next)
        force_minimize(conn, c);
    xcb_flush(conn);
}
