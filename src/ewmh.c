#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include "ewmh.h"
#include "window.h"

typedef struct {
    xcb_atom_t net_supported;
    xcb_atom_t net_client_list;
    xcb_atom_t net_client_list_stacking;
    xcb_atom_t net_active_window;
    xcb_atom_t net_supporting_wm_check;
    xcb_atom_t net_wm_name;
    xcb_atom_t net_number_of_desktops;
    xcb_atom_t net_current_desktop;
    xcb_atom_t net_wm_state;
    xcb_atom_t net_wm_state_maximized_vert;
    xcb_atom_t net_wm_state_maximized_horz;
    xcb_atom_t net_wm_state_fullscreen;
    xcb_atom_t net_wm_state_hidden;
    xcb_atom_t utf8_string;
} ewmh_atoms_t;

static ewmh_atoms_t atoms;
static xcb_window_t check_window = XCB_NONE;

/* Инкрементальный кэш _NET_CLIENT_LIST/_NET_CLIENT_LIST_STACKING - см.
   ewmh_client_added()/ewmh_client_removed() ниже. */
static xcb_window_t *g_client_cache = NULL;
static int g_client_count = 0;
static int g_client_capacity = 0;

static xcb_atom_t intern(xcb_connection_t *conn, const char *name) {
    xcb_intern_atom_cookie_t cookie =
        xcb_intern_atom(conn, 0, (uint16_t)strlen(name), name);
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn, cookie, NULL);
    xcb_atom_t atom = reply ? reply->atom : XCB_ATOM_NONE;
    free(reply);
    return atom;
}

void ewmh_init(xcb_connection_t *conn, xcb_screen_t *screen) {
    atoms.net_supported               = intern(conn, "_NET_SUPPORTED");
    atoms.net_client_list             = intern(conn, "_NET_CLIENT_LIST");
    atoms.net_client_list_stacking    = intern(conn, "_NET_CLIENT_LIST_STACKING");
    atoms.net_active_window           = intern(conn, "_NET_ACTIVE_WINDOW");
    atoms.net_supporting_wm_check     = intern(conn, "_NET_SUPPORTING_WM_CHECK");
    atoms.net_wm_name                 = intern(conn, "_NET_WM_NAME");
    atoms.net_number_of_desktops      = intern(conn, "_NET_NUMBER_OF_DESKTOPS");
    atoms.net_current_desktop         = intern(conn, "_NET_CURRENT_DESKTOP");
    atoms.net_wm_state                = intern(conn, "_NET_WM_STATE");
    atoms.net_wm_state_maximized_vert = intern(conn, "_NET_WM_STATE_MAXIMIZED_VERT");
    atoms.net_wm_state_maximized_horz = intern(conn, "_NET_WM_STATE_MAXIMIZED_HORZ");
    atoms.net_wm_state_fullscreen     = intern(conn, "_NET_WM_STATE_FULLSCREEN");
    atoms.net_wm_state_hidden         = intern(conn, "_NET_WM_STATE_HIDDEN");
    atoms.utf8_string                 = intern(conn, "UTF8_STRING");

    /* _NET_SUPPORTING_WM_CHECK (wm-spec §Root Window Properties): окно-
       "маячок" 1x1, никогда не мапится. Его наличие с валидным именем -
       то, как сторонние тулзы отличают "здесь работает EWMH-совместимый
       WM" от "WM вообще нет". */
    check_window = xcb_generate_id(conn);
    xcb_create_window(conn, XCB_COPY_FROM_PARENT, check_window, screen->root,
                       -1, -1, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                       screen->root_visual, 0, NULL);

    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, check_window,
                         atoms.net_supporting_wm_check, XCB_ATOM_WINDOW, 32, 1, &check_window);
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root,
                         atoms.net_supporting_wm_check, XCB_ATOM_WINDOW, 32, 1, &check_window);

    static const char wm_name[] = "QSWM";
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, check_window,
                         atoms.net_wm_name, atoms.utf8_string, 8,
                         (uint32_t)(sizeof(wm_name) - 1), wm_name);

    xcb_atom_t supported[] = {
        atoms.net_supported,
        atoms.net_client_list,
        atoms.net_client_list_stacking,
        atoms.net_active_window,
        atoms.net_supporting_wm_check,
        atoms.net_wm_name,
        atoms.net_number_of_desktops,
        atoms.net_current_desktop,
        atoms.net_wm_state,
        atoms.net_wm_state_maximized_vert,
        atoms.net_wm_state_maximized_horz,
        atoms.net_wm_state_fullscreen,
        atoms.net_wm_state_hidden,
    };
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root,
                         atoms.net_supported, XCB_ATOM_ATOM, 32,
                         sizeof(supported) / sizeof(supported[0]), supported);

    /* QSWM не поддерживает несколько виртуальных столов - объявляем один,
       текущий, чтобы пейджеры не показывали пустое/некорректное число. */
    uint32_t one = 1, zero = 0;
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root,
                         atoms.net_number_of_desktops, XCB_ATOM_CARDINAL, 32, 1, &one);
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root,
                         atoms.net_current_desktop, XCB_ATOM_CARDINAL, 32, 1, &zero);

    ewmh_update_client_list(conn, screen);
    ewmh_set_active_window(conn, screen, XCB_NONE);
}

void ewmh_cleanup(xcb_connection_t *conn) {
    if (check_window != XCB_NONE) {
        xcb_destroy_window(conn, check_window);
        check_window = XCB_NONE;
    }
    free(g_client_cache);
    g_client_cache = NULL;
    g_client_count = 0;
    g_client_capacity = 0;
}

/* Инкрементальное поддержание _NET_CLIENT_LIST/_NET_CLIENT_LIST_STACKING.
   Раньше единственным способом обновить эти свойства был полный ресинк
   (ewmh_update_client_list() ниже): на каждый MapRequest/DestroyNotify/
   UnmapNotify WM заново обходил весь связный список клиентов (window.h/
   window_client.c), считал длину, аллоцировал новый массив, копировал в
   него все окна - хотя реально изменилось только ОДНО окно. При большом
   числе одновременно открытых окон и частом их открытии/закрытии
   (типичный сценарий - множество терминалов/диалогов) это O(n) работы и
   одна malloc/free-пара на КАЖДОЕ такое событие. ewmh_client_added()/
   ewmh_client_removed() вместо этого держат собственный растущий массив
   g_client_cache и правят его точечно: добавление - O(1) амортизированно
   (реаллок только когда capacity исчерпана), удаление - O(n) на поиск
   нужного окна (без этого никак, порядок в массиве не индексирован по
   xcb_window_t), но без единой лишней аллокации на само удаление
   (последний элемент просто занимает место удалённого - порядок
   _NET_CLIENT_LIST спецификацией не фиксирован, см. комментарий ниже
   про _NET_CLIENT_LIST_STACKING). */

static void ewmh_write_client_list_props(xcb_connection_t *conn, xcb_screen_t *screen) {
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root,
                         atoms.net_client_list, XCB_ATOM_WINDOW, 32,
                         (uint32_t)g_client_count, g_client_cache);
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root,
                         atoms.net_client_list_stacking, XCB_ATOM_WINDOW, 32,
                         (uint32_t)g_client_count, g_client_cache);
}

void ewmh_update_client_list(xcb_connection_t *conn, xcb_screen_t *screen) {
    int count = 0;
    for (client_t *c = window_get_clients(); c; c = c->next) count++;

    xcb_window_t *wins = NULL;
    if (count > 0) {
        wins = malloc(sizeof(xcb_window_t) * (size_t)count);
        if (!wins) return; /* OOM - пропускаем обновление, не падаем */
        int i = 0;
        for (client_t *c = window_get_clients(); c; c = c->next)
            wins[i++] = c->win;
    }

    /* Порядок обхода connected-списка (LIFO по времени добавления) не
       обязан совпадать с реальным Z-order на экране - для _NET_CLIENT_LIST
       порядок не специфицирован, а вот _NET_CLIENT_LIST_STACKING формально
       должен идти bottom-to-top. QSWM не ведёт отдельный стек Z-порядка,
       поэтому используем тот же список для обоих свойств: это не хуже,
       чем полное отсутствие свойства, но не идеально точная стековая
       информация для тулз, которые её действительно используют. */
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root,
                         atoms.net_client_list, XCB_ATOM_WINDOW, 32, (uint32_t)count, wins);
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root,
                         atoms.net_client_list_stacking, XCB_ATOM_WINDOW, 32, (uint32_t)count, wins);

    /* Инкрементальный кэш (ewmh_client_added/removed выше) должен
       остаться синхронным с тем, что реально отправлено выше - иначе
       следующий add/remove продолжит работать со старым содержимым.
       ewmh_update_client_list() сейчас вызывается только из ewmh_init()
       (когда клиентов ещё 0), но остаётся публичной функцией полного
       ресинка на будущее, поэтому кэш пересобирается и здесь тоже. */
    free(g_client_cache);
    g_client_cache = wins;
    g_client_count = count;
    g_client_capacity = count;
}

void ewmh_client_added(xcb_connection_t *conn, xcb_screen_t *screen, xcb_window_t win) {
    if (g_client_count == g_client_capacity) {
        int new_capacity = g_client_capacity > 0 ? g_client_capacity * 2 : 4;
        xcb_window_t *grown = realloc(g_client_cache, sizeof(xcb_window_t) * (size_t)new_capacity);
        if (!grown) return; /* OOM - пропускаем обновление, не падаем */
        g_client_cache = grown;
        g_client_capacity = new_capacity;
    }
    g_client_cache[g_client_count++] = win;
    ewmh_write_client_list_props(conn, screen);
}

void ewmh_client_removed(xcb_connection_t *conn, xcb_screen_t *screen, xcb_window_t win) {
    for (int i = 0; i < g_client_count; i++) {
        if (g_client_cache[i] == win) {
            g_client_cache[i] = g_client_cache[g_client_count - 1];
            g_client_count--;
            break;
        }
    }
    ewmh_write_client_list_props(conn, screen);
}

void ewmh_set_active_window(xcb_connection_t *conn, xcb_screen_t *screen, xcb_window_t win) {
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root,
                         atoms.net_active_window, XCB_ATOM_WINDOW, 32, 1, &win);
}

void ewmh_update_wm_state(xcb_connection_t *conn, client_t *c) {
    xcb_atom_t state[4];
    int n = 0;
    if (c->maximized) {
        state[n++] = atoms.net_wm_state_maximized_vert;
        state[n++] = atoms.net_wm_state_maximized_horz;
    }
    if (c->fullscreen) state[n++] = atoms.net_wm_state_fullscreen;
    if (c->minimized)  state[n++] = atoms.net_wm_state_hidden;

    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, c->win,
                         atoms.net_wm_state, XCB_ATOM_ATOM, 32, (uint32_t)n, state);
}

/* _NET_WM_STATE action values (wm-spec §_NET_WM_STATE). */
#define _NET_WM_STATE_REMOVE 0
#define _NET_WM_STATE_ADD    1
#define _NET_WM_STATE_TOGGLE 2

void ewmh_handle_client_message(xcb_connection_t *conn, xcb_screen_t *screen,
                                 xcb_client_message_event_t *e) {
    if (e->type == atoms.net_wm_state) {
        client_t *c = client_find_by_window(e->window);
        if (!c) return;

        uint32_t action = e->data.data32[0];
        xcb_atom_t prop1 = (xcb_atom_t)e->data.data32[1];
        xcb_atom_t prop2 = (xcb_atom_t)e->data.data32[2];

        int wants_fullscreen = (prop1 == atoms.net_wm_state_fullscreen ||
                                 prop2 == atoms.net_wm_state_fullscreen);
        if (!wants_fullscreen) return;

        int should_be_fullscreen;
        switch (action) {
            case _NET_WM_STATE_ADD:    should_be_fullscreen = 1; break;
            case _NET_WM_STATE_REMOVE: should_be_fullscreen = 0; break;
            case _NET_WM_STATE_TOGGLE: should_be_fullscreen = !c->fullscreen; break;
            default: return;
        }

        if (should_be_fullscreen != c->fullscreen)
            window_toggle_fullscreen(conn, screen, c);
        return;
    }

    if (e->type != atoms.net_active_window) return;

    client_t *c = client_find_by_window(e->window);
    if (!c) return;

    /* Свёрнутое окно нужно сперва вернуть на экран (frame был unmap'нут
       при minimize) - как и в alttab_handle_key_release(), иначе Raise/
       фокус ниже ничего не покажут. */
    if (c->minimized)
        window_toggle_minimize(conn, screen, c);

    uint32_t stack_mode = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(conn, c->frame, XCB_CONFIG_WINDOW_STACK_MODE, &stack_mode);
    window_set_focus(conn, screen, c);
    xcb_flush(conn);
}
