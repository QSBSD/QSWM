#define _POSIX_C_SOURCE 200809L /* strnlen() требует POSIX.1-2008 при -std=c11 */
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/render.h>
#include "window.h"
#include "wm_state.h"
#include "snap.h"
#include "window_drag.h"
#include "ewmh.h"

static client_t *clients = NULL;

client_t *client_find_by_frame(xcb_window_t frame) {
    for (client_t *c = clients; c; c = c->next)
        if (c->frame == frame) return c;
    return NULL;
}

client_t *client_find_by_window(xcb_window_t win) {
    for (client_t *c = clients; c; c = c->next)
        if (c->win == win) return c;
    return NULL;
}

client_t *window_get_clients(void) {
    return clients;
}

static void client_add(client_t *c) {
    c->next = clients;
    clients = c;
}

static void client_remove(client_t *c) {
    if (clients == c) { clients = c->next; return; }
    for (client_t *p = clients; p; p = p->next) {
        if (p->next == c) { p->next = c->next; return; }
    }
}

/* Читает свойство WM_CLASS управляемого окна win и копирует в out
   class-часть (вторую NUL-строку вида "instance\0class\0"), которая
   обычно совпадает с именем файла SVG-иконки приложения (пункт 7,
   см. icons.c). Если свойство отсутствует или его не удалось прочитать,
   out остаётся пустой строкой — icons_draw() в этом случае вернёт 0,
   и вызывающий код нарисует запасную заглушку. */
static void fetch_wm_class(xcb_connection_t *conn, xcb_window_t win, char *out, size_t outsz) {
    out[0] = '\0';

    xcb_get_property_cookie_t cookie =
        xcb_get_property(conn, 0, win, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 0, 256);
    xcb_get_property_reply_t *reply = xcb_get_property_reply(conn, cookie, NULL);
    if (!reply) return;

    int len = xcb_get_property_value_length(reply);
    if (len > 0) {
        const char *data = (const char *)xcb_get_property_value(reply);
        size_t inst_len = strnlen(data, (size_t)len);
        const char *class_part = (inst_len + 1 < (size_t)len) ? data + inst_len + 1 : data;
        size_t remaining = (size_t)len - (size_t)(class_part - data);
        size_t class_len = strnlen(class_part, remaining);

        if (class_len >= outsz) class_len = outsz - 1;
        memcpy(out, class_part, class_len);
        out[class_len] = '\0';
    }

    free(reply);
}

/* Баг №7: пассивный грэб клика (баг №1) ставился на клиентское окно
   один раз при window_manage() и никогда не снимался — в том числе
   пока это окно было АКТИВНЫМ/сфокусированным. Между ButtonPress и
   ButtonRelease указатель при этом всегда оказывался под АКТИВНЫМ
   грэбом QSWM (X допускает только один активный grab одновременно),
   и owner_events=1 гарантирует лишь нормальную доставку самих
   press/release/motion — но если тулкит внутри клика сам пытается
   поставить свой собственный XGrabPointer (так по стандартной схеме
   GTK/Qt отличают клик от начала drag и отслеживают клик вне меню,
   почти всегда — контекстные меню по ПКМ, drag в списках/иконках),
   этот grab у приложения молча проваливается (AlreadyGrabbed), пока
   активен наш. Простой clic по выделению элемента ещё проходит
   (drag-select работает на press+motion без грэба со стороны
   приложения), а открытие/контекстное меню — нет: именно это и
   наблюдалось ("выделяется, но не нажимается").
   Реальные WM (см. напр. monsterwm-xcb) снимают этот грэб именно с
   окна, ставшего активным, и возвращают его окну, потерявшему фокус —
   грэб должен работать только для клика ПО НЕФОКУСНОМУ окну (чтобы
   поднять/сфокусировать), а не мешать уже сфокусированному окну
   работать с собственными кликами. */
void window_grab_click_to_focus(xcb_connection_t *conn, xcb_window_t win) {
    xcb_grab_button(conn, 1, win,
                     XCB_EVENT_MASK_BUTTON_PRESS,
                     XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
                     XCB_NONE, XCB_NONE,
                     XCB_BUTTON_INDEX_ANY, XCB_MOD_MASK_ANY);
}

void window_ungrab_click_to_focus(xcb_connection_t *conn, xcb_window_t win) {
    xcb_ungrab_button(conn, XCB_BUTTON_INDEX_ANY, win, XCB_MOD_MASK_ANY);
}

/* Единая точка смены активного окна (фокус + _NET_ACTIVE_WINDOW +
   грэб клика, баг №7): снимает грэб с нового фокуса и возвращает его
   старому, потерявшему фокус. c == NULL — снять фокус со всех (клик по
   столу/закрытие последнего окна). */
void window_set_focus(xcb_connection_t *conn, xcb_screen_t *screen, client_t *c) {
    client_t *prev = window_focused_client(conn);
    if (prev && prev != c)
        window_grab_click_to_focus(conn, prev->win);

    if (c) {
        window_ungrab_click_to_focus(conn, c->win);
        xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, c->win, XCB_CURRENT_TIME);
        ewmh_set_active_window(conn, screen, c->win);
    } else {
        xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, screen->root, XCB_CURRENT_TIME);
        ewmh_set_active_window(conn, screen, XCB_NONE);
    }
}

/* focus-follows-mouse: переход указателя на другое управляемое окно
   делает его активным, если оно ещё не было в фокусе — без клика.
   e->mode/e->detail фильтруются так же, как в других минималистичных
   WM (dwm, i3): EnterNotify приходит не только от реального движения
   мыши, но и синтетически при xcb_grab_pointer/ungrab (клик-грэбы
   window_grab_click_to_focus, обычные XCB_GRAB_MODE_* при drag) и при
   переходах между родителем/потомком одной и той же frame (detail
   Inferior/Ancestor) — ни то, ни другое не должно дёргать фокус. */
void window_handle_enter_notify(xcb_connection_t *conn, xcb_screen_t *screen,
                                 xcb_enter_notify_event_t *e) {
    if (e->mode != XCB_NOTIFY_MODE_NORMAL) return;
    if (e->detail == XCB_NOTIFY_DETAIL_INFERIOR) return;

    client_t *c = client_find_by_frame(e->event);
    if (!c) c = client_find_by_window(e->event);
    if (!c) return;

    client_t *focused = window_focused_client(conn);
    if (focused == c) return; /* уже активно — незачем дёргать фокус повторно */

    window_set_focus(conn, screen, c);
}

/* Баг №6: если фокус ввода X-сервера указывал на removed_win (только что
   удалённый клиент), переводим его на другое управляемое окно (голова
   списка clients — им же обычно последним пользовались) или, если
   клиентов больше нет, на root. Раньше фокус так и оставался висеть на
   уже несуществующем окне, и хоткеи close/maximize/minimize переставали
   действовать, пока пользователь не кликал по другому окну мышью. */
static void refocus_after_removal(xcb_connection_t *conn, xcb_screen_t *screen,
                                   xcb_window_t removed_win) {
    xcb_get_input_focus_cookie_t fc = xcb_get_input_focus(conn);
    xcb_get_input_focus_reply_t *fr = xcb_get_input_focus_reply(conn, fc, NULL);
    if (!fr) return;
    xcb_window_t focused = fr->focus;
    free(fr);

    if (focused != removed_win) return; /* фокус был не на этом окне — не трогаем */

    if (clients) {
        uint32_t stack_mode = XCB_STACK_MODE_ABOVE;
        xcb_configure_window(conn, clients->frame, XCB_CONFIG_WINDOW_STACK_MODE, &stack_mode);
        /* removed_win к этому моменту уже удалён из clients и его frame
           уничтожен вызывающей стороной, поэтому window_focused_client()
           внутри window_set_focus() ничего для него не найдёт (client_t
           уже нет) и просто не будет пытаться вернуть ему грэб клика. */
        window_set_focus(conn, screen, clients);
    } else {
        window_set_focus(conn, screen, NULL);
    }
}

void window_manage(xcb_connection_t *conn, xcb_screen_t *screen, xcb_window_t win) {
    xcb_get_geometry_cookie_t gc = xcb_get_geometry(conn, win);
    xcb_get_geometry_reply_t *geom = xcb_get_geometry_reply(conn, gc, NULL);

    int16_t  x = geom ? geom->x      : 0;
    int16_t  y = geom ? geom->y      : 0;
    uint16_t w = geom ? geom->width  : 640;
    uint16_t h = geom ? geom->height : 480;
    free(geom);

    xcb_window_t frame = xcb_generate_id(conn);
    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t values[2] = {
        screen->black_pixel,
        XCB_EVENT_MASK_EXPOSURE |
        XCB_EVENT_MASK_BUTTON_PRESS |
        XCB_EVENT_MASK_BUTTON_RELEASE |
        XCB_EVENT_MASK_POINTER_MOTION |
        /* focus-follows-mouse (window_handle_enter_notify()): без этого
           переход указателя на неактивное окно не фокусирует его — нужен
           клик, как раньше. */
        XCB_EVENT_MASK_ENTER_WINDOW |
        /* без этого DestroyNotify/UnmapNotify дочернего окна (оно теперь
           потомок frame, а не root) не долетают до wm_run() — окно
           закрывается на стороне приложения, а его frame остаётся висеть,
           потому что window_unmanage() никогда не вызывается. */
        XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
    };

    xcb_create_window(conn, XCB_COPY_FROM_PARENT, frame, screen->root,
                       x, y, w, (uint16_t)(h + window_titlebar_height()), 1,
                       XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual,
                       mask, values);

    uint32_t border_width = 0;
    xcb_configure_window(conn, win, XCB_CONFIG_WINDOW_BORDER_WIDTH, &border_width);

    xcb_reparent_window(conn, win, frame, 0, window_titlebar_height());

    /* Пассивный grab кнопки на самом клиентском окне (баг №1): без него
       клик в рабочей области окна уходит напрямую приложению и никогда
       не долетает до WM — окно не фокусируется и не поднимается, пока
       пользователь не кликнет по titlebar. owner_events=1 и оба режима
       ASYNC (без freeze) означают, что клиент всё равно получит тот же
       клик как обычно — WM только "подсматривает" его первым, в
       window_button_press() (window_drag.c). Ставим его здесь как
       "окно ещё не в фокусе" — как только ниже вызовем window_set_focus()
       на этом же окне, грэб снимется (баг №7, см. window_set_focus()). */
    window_grab_click_to_focus(conn, win);

    xcb_map_window(conn, win);
    xcb_map_window(conn, frame);

    client_t *c = calloc(1, sizeof(client_t));
    if (!c) return; /* OOM: окно останется неуправляемым, но WM не падает */
    c->win = win;
    c->frame = frame;
    c->x = x; c->y = y;
    c->width = w; c->height = h;
    c->maximized = 0;
    c->min_width = MIN_WIDTH;
    c->min_height = MIN_HEIGHT;
    c->max_width = 0;
    c->max_height = 0;
    window_fetch_normal_hints(conn, c); /* WM_NORMAL_HINTS: PMinSize/PMaxSize, если клиент их выставил */
    fetch_wm_class(conn, win, c->app_class, sizeof(c->app_class));
    client_add(c);
    wm_state_set(conn, win, WM_STATE_NORMAL);
    ewmh_update_wm_state(conn, c);
    ewmh_client_added(conn, screen, win);

    /* Новое окно открывается развёрнутым (кнопка □ между "-" и "×" в
       titlebar / хоткей maximize) - так же, как если бы пользователь сам
       нажал её сразу после появления окна. saved_x/y/width/height внутри
       window_toggle_maximize() запоминают исходную геометрию (x/y/w/h
       выше), поэтому повторное нажатие □ вернёт окно к его "естественному"
       размеру, как обычно.
       Сам вызов отложен до первого Expose (window_expose(),
       window_decoration.c) - см. pending_auto_maximize в window.h: если
       максимизировать прямо здесь, клиент ещё не успел закончить
       собственную инициализацию (GLFW/LWJGL-игры вроде Minecraft кэшируют
       размер окна для пересчёта координат курсора именно в этот момент),
       и resize, прилетевший настолько рано, там не переучитывается —
       клики потом мимо, пока не переключить fullscreen/maximize туда-обратно. */
    c->pending_auto_maximize = 1;

    /* без этого свежесозданное окно не получает клавиатурный ввод, пока
       пользователь не переключится на него через Alt+Tab. window_set_focus()
       (баг №7) заодно снимает с этого окна грэб клика, поставленный чуть
       выше, и возвращает грэб предыдущему сфокусированному окну. */
    window_set_focus(conn, screen, c);

    xcb_flush(conn);
    window_draw_decoration(conn, screen, c);
}

void window_unmanage(xcb_connection_t *conn, xcb_screen_t *screen, xcb_window_t win) {
    client_t *c = client_find_by_window(win);
    if (!c) return;

    if (c->deco_picture != XCB_NONE) xcb_render_free_picture(conn, c->deco_picture);
    xcb_destroy_window(conn, c->frame);
    snap_client_removed(c); /* до free(c) — иначе held_client в snap.c повиснет */
    window_drag_client_removed(conn, c); /* до free(c) — иначе g_window_drag.client повиснет */
    client_remove(c);
    free(c);
    ewmh_client_removed(conn, screen, win);
    xcb_flush(conn);

    refocus_after_removal(conn, screen, win);
}

/* Баг №2: клиент спрятал окно по своей инициативе (UnmapNotify) без
   DestroyNotify — например, переход в WithdrawnState или сворачивание в
   трей. ICCCM прямо требует реагировать на такой UnmapNotify как на
   переход в Withdrawn. Раньше case для XCB_UNMAP_NOTIFY в wm_run() вообще
   отсутствовал: client_t/frame оставались висеть навсегда — "призрачное"
   окно в alt-tab, в списке клиентов и т.д. */
void window_withdraw(xcb_connection_t *conn, xcb_screen_t *screen, xcb_window_t win) {
    client_t *c = client_find_by_window(win);
    if (!c) return;

    /* Возвращаем win root'у, ПРЕЖДЕ чем уничтожать frame — иначе
       xcb_destroy_window(frame) заберёт с собой и всё ещё дочерний win,
       хотя приложение не собиралось его уничтожать (оно может позже
       снова смапить то же окно, и тогда придёт новый MapRequest). */
    xcb_reparent_window(conn, win, screen->root, c->x, (int16_t)(c->y + window_titlebar_height()));
    wm_state_set(conn, win, WM_STATE_WITHDRAWN);

    if (c->deco_picture != XCB_NONE) xcb_render_free_picture(conn, c->deco_picture);
    xcb_destroy_window(conn, c->frame);
    snap_client_removed(c); /* до free(c) — иначе held_client в snap.c повиснет */
    window_drag_client_removed(conn, c); /* до free(c) — иначе g_window_drag.client повиснет */
    client_remove(c);
    free(c);
    ewmh_client_removed(conn, screen, win);
    xcb_flush(conn);

    refocus_after_removal(conn, screen, win);
}

/* Клиент, на чьё окно сейчас указывает фокус ввода X-сервера. Используется
   диспетчером хоткеев в wm.c: close/maximize/minimize применяются к
   текущему сфокусированному окну (пункт 6). */
client_t *window_focused_client(xcb_connection_t *conn) {
    xcb_get_input_focus_cookie_t fc = xcb_get_input_focus(conn);
    xcb_get_input_focus_reply_t *fr = xcb_get_input_focus_reply(conn, fc, NULL);
    if (!fr) return NULL;

    xcb_window_t focused = fr->focus;
    free(fr);

    client_t *c = client_find_by_window(focused);
    if (!c) c = client_find_by_frame(focused);
    return c;
}
