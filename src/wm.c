#define _POSIX_C_SOURCE 200809L /* waitpid()/poll() требуют POSIX при -std=c11 */
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <poll.h>
#include <sys/wait.h>
#include <xcb/xcb.h>
#include "wm.h"
#include "window.h"
#include "altTab.h"
#include "altTab_render.h"
#include "config.h"
#include "config_load.h"
#include "config_keys.h"
#include "config_exec.h"
#include "config_numlock.h"
#include "icons.h"
#include "snap.h"
#include "cursor.h"
#include "ewmh.h"

/* Конфигурация хоткеев/оформления, загруженная из ~/.config/QSWM/config.conf
   (пункт 6). Живёт на протяжении всего цикла wm_run(). */
static wm_config_t g_config;

void screen_get_size(xcb_connection_t *conn, xcb_screen_t *screen,
                      uint16_t *out_w, uint16_t *out_h) {
    uint16_t w = screen->width_in_pixels;
    uint16_t h = screen->height_in_pixels;

    xcb_get_geometry_cookie_t gc = xcb_get_geometry(conn, screen->root);
    xcb_get_geometry_reply_t *geom = xcb_get_geometry_reply(conn, gc, NULL);
    if (geom) {
        w = geom->width;
        h = geom->height;
        free(geom);
    }

    *out_w = w;
    *out_h = h;
}

/* Выполняет действие, привязанное к нажатой комбинации клавиш. Action
   применяется к окну, находящемуся в фокусе (см. window_focused_client). */
static void wm_dispatch_action(xcb_connection_t *conn, xcb_screen_t *screen,
                                wm_action_t action, const char *exec_cmd) {
    client_t *c;
    switch (action) {
    case ACTION_CLOSE:
        c = window_focused_client(conn);
        if (c) window_close(conn, c);
        break;
    case ACTION_MAXIMIZE:
        c = window_focused_client(conn);
        if (c) window_toggle_maximize(conn, screen, c);
        break;
    case ACTION_MINIMIZE:
        c = window_focused_client(conn);
        if (c) window_toggle_minimize(conn, screen, c);
        break;
    case ACTION_FULLSCREEN:
        c = window_focused_client(conn);
        if (c) window_toggle_fullscreen(conn, screen, c);
        break;
    case ACTION_LAUNCHER:
        config_spawn_launcher(&g_config);
        break;
    case ACTION_EXEC:
        config_spawn_command(exec_cmd);
        break;
    default:
        break;
    }
}

/* config_spawn_launcher()/config_run_autostart() делают fork()+exec() и
   никогда не вызывают waitpid() за завершившимся ребёнком. Без этого
   каждый запуск launcher'а/autostart оставляет defunct-процесс до самого
   выхода из QSWM. SIG_IGN на SIGCHLD поручает системе (see waitpid(2),
   "POSIX.1-2001") автоматически подчищать зомби за нас. */
static void wm_reap_children(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NOCLDWAIT;
    sigaction(SIGCHLD, &sa, NULL);
}

/* Управляет уже отображёнными top-level окнами, существовавшими на момент
   запуска QSWM (например, при перезапуске WM внутри уже идущей X-сессии).
   Без этого прохода такие окна остаются без декорации и вне списка
   clients, пока их не переоткроют — обычный WM подхватывает их через
   QueryTree по root при старте, а не только через будущие MapRequest. */
static void wm_manage_existing(xcb_connection_t *conn, xcb_screen_t *screen) {
    xcb_query_tree_cookie_t qc = xcb_query_tree(conn, screen->root);
    xcb_query_tree_reply_t *qr = xcb_query_tree_reply(conn, qc, NULL);
    if (!qr) return;

    xcb_window_t *children = xcb_query_tree_children(qr);
    int n = xcb_query_tree_children_length(qr);

    for (int i = 0; i < n; i++) {
        xcb_get_window_attributes_cookie_t ac = xcb_get_window_attributes(conn, children[i]);
        xcb_get_window_attributes_reply_t *ar = xcb_get_window_attributes_reply(conn, ac, NULL);
        if (!ar) continue;

        int manage = !ar->override_redirect && ar->map_state == XCB_MAP_STATE_VIEWABLE;
        free(ar);
        if (manage) window_manage(conn, screen, children[i]);
    }

    free(qr);
}

int wm_init(xcb_connection_t *conn, xcb_screen_t *screen) {
    uint32_t mask = XCB_CW_EVENT_MASK;
    uint32_t values[1] = {
        XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
        XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
        XCB_EVENT_MASK_STRUCTURE_NOTIFY
    };

    xcb_void_cookie_t cookie = xcb_change_window_attributes_checked(
        conn, screen->root, mask, values);
    xcb_generic_error_t *err = xcb_request_check(conn, cookie);
    if (err) {
        free(err);
        return -1; /* root уже слушает другой WM */
    }

    config_load(NULL, &g_config);          /* NULL -> ~/.config/QSWM/config.conf */

    /* До первого xcb_grab_key()/xcb_grab_button() — иначе граб на 4
       "лишних" модификатора будет опираться на запасной XCB_MOD_MASK_2
       вместо реальной раскладки (см. config_detect_numlock()). */
    config_detect_numlock(conn);

    cursor_set_default(conn, screen); /* иначе курсор невидим над столом */

    /* Пункт 6: [appearance] из config.conf реально влияет на отрисовку,
       а не только парсится — размеры декорации и раскладка иконки/кнопок
       в window.c, позиция оверлея в altTab.c. */
    window_set_appearance(g_config.titlebar_height, g_config.button_size,
                           g_config.icon_left);
    window_set_titlebar_color(g_config.titlebar_color_r, g_config.titlebar_color_g,
                               g_config.titlebar_color_b);
    alttab_set_position(g_config.alttab_position);
    alttab_set_bg_color(g_config.alttab_bg_r, g_config.alttab_bg_g, g_config.alttab_bg_b);
    alttab_render_set_cell_size(g_config.alttab_icon_size);

    ewmh_init(conn, screen);                /* _NET_SUPPORTED и т.п. - см. ewmh.c */
    icons_init(conn, screen);              /* кэш SVG-иконок (пункт 7) */

    /* [icons].dir из config.conf: каталог, где для ЛЮБОГО приложения
       сначала ищется "{app_class}.svg", раньше системных icon-тем. */
    if (g_config.icon_dir[0])
        icons_set_custom_dir(g_config.icon_dir);

    alttab_init(conn, screen);
    snap_init(conn, screen);               /* Meta+стрелки / Meta+Tab / Ctrl+Tab */
    config_grab_keys(conn, screen, &g_config);

    wm_reap_children();                    /* без этого launcher/autostart копят зомби */
    wm_manage_existing(conn, screen);      /* окна, уже открытые до старта QSWM */
    config_run_autostart();                /* ~/.config/QSWM/autostart, если есть */

    xcb_flush(conn);
    return 0;
}

/* Раскладка/маппинг клавиатуры сменились (setxkbmap, xmodmap и т.п.).
   Раньше это событие вообще не обрабатывалось: keycode для всех наших
   грабов (config_grab_keys/alttab_init/snap_init) резолвились один раз
   при старте и оставались привязаны к старой раскладке, поэтому хоткеи,
   Alt+Tab и Meta+стрелки переставали срабатывать до перезапуска QSWM.
   MAPPING_MODIFIER - пересчитываем маску NumLock (какой Mod-бит на неё
   сейчас назначен могло тоже поменяться). MAPPING_KEYBOARD - keysym-ы
   переехали на другие keycode: снимаем все грабы клавиш с root (QSWM -
   единственный их владелец, поэтому xcb_ungrab_key(ANY) безопасен) и
   грабим заново той же функцией инициализации - она сама заново
   резолвит keysym->keycode через свежесозданный xcb_key_symbols_t. */
static void wm_handle_mapping_notify(xcb_connection_t *conn, xcb_screen_t *screen,
                                      xcb_mapping_notify_event_t *e) {
    if (e->request == XCB_MAPPING_MODIFIER || e->request == XCB_MAPPING_KEYBOARD)
        config_detect_numlock(conn);

    if (e->request != XCB_MAPPING_KEYBOARD) return;

    xcb_ungrab_key(conn, XCB_GRAB_ANY, screen->root, XCB_MOD_MASK_ANY);
    alttab_init(conn, screen);
    snap_init(conn, screen);
    config_grab_keys(conn, screen, &g_config);
    xcb_flush(conn);
}

/* Обрабатывает одно X-событие. Вынесено из wm_run() отдельной функцией,
   чтобы главный цикл мог вычерпывать очередь событий через
   xcb_poll_for_event() внутри poll()-цикла (см. wm_run) вместо блокирующего
   xcb_wait_for_event() - тот не даёт шанса дождаться self-pipe с сигналом
   завершения. */
static void wm_handle_event(xcb_connection_t *conn, xcb_screen_t *screen,
                             xcb_generic_event_t *ev) {
        switch (ev->response_type & ~0x80) {
        case XCB_MAP_REQUEST: {
            xcb_map_request_event_t *e = (xcb_map_request_event_t *)ev;
            window_manage(conn, screen, e->window);
            break;
        }
        case XCB_CONFIGURE_REQUEST: {
            xcb_configure_request_event_t *e =
                (xcb_configure_request_event_t *)ev;
            window_configure(conn, screen, e);
            break;
        }
        case XCB_DESTROY_NOTIFY: {
            xcb_destroy_notify_event_t *e =
                (xcb_destroy_notify_event_t *)ev;
            window_unmanage(conn, screen, e->window);
            break;
        }
        case XCB_UNMAP_NOTIFY: {
            /* баг №2: клиент спрятал окно сам, без DestroyNotify —
               ICCCM требует перевести его в WithdrawnState */
            xcb_unmap_notify_event_t *e =
                (xcb_unmap_notify_event_t *)ev;
            window_withdraw(conn, screen, e->window);
            break;
        }
        case XCB_BUTTON_PRESS:
            window_button_press(conn, screen, (xcb_button_press_event_t *)ev);
            break;
        case XCB_MOTION_NOTIFY:
            window_motion_notify(conn, (xcb_motion_notify_event_t *)ev);
            break;
        case XCB_BUTTON_RELEASE:
            window_button_release(conn, screen, (xcb_button_release_event_t *)ev);
            break;
        case XCB_ENTER_NOTIFY:
            window_handle_enter_notify(conn, screen, (xcb_enter_notify_event_t *)ev);
            break;
        case XCB_EXPOSE: {
            xcb_expose_event_t *e = (xcb_expose_event_t *)ev;
            if (e->count == 0) { /* рисуем один раз на серию Expose */
                if (e->window == alttab_overlay_window())
                    alttab_expose(conn, screen, e);
                else
                    window_expose(conn, screen, e);
            }
            break;
        }
        case XCB_KEY_PRESS: {
            xcb_key_press_event_t *e = (xcb_key_press_event_t *)ev;
            const keybinding_t *kb = config_find_binding(&g_config, e->state, e->detail);
            if (kb)
                wm_dispatch_action(conn, screen, kb->action, kb->command);
            else if (!snap_handle_key_press(conn, screen, e))
                alttab_handle_key_press(conn, screen, e);
            break;
        }
        case XCB_KEY_RELEASE:
            snap_handle_key_release((xcb_key_release_event_t *)ev);
            alttab_handle_key_release(conn, screen, (xcb_key_release_event_t *)ev);
            break;
        case XCB_MAPPING_NOTIFY:
            wm_handle_mapping_notify(conn, screen, (xcb_mapping_notify_event_t *)ev);
            break;
        case XCB_CLIENT_MESSAGE:
            ewmh_handle_client_message(conn, screen, (xcb_client_message_event_t *)ev);
            break;
        default:
            break;
        }
}

void wm_run(xcb_connection_t *conn, xcb_screen_t *screen, int exit_fd) {
    int xcb_fd = xcb_get_file_descriptor(conn);
    struct pollfd fds[2];
    fds[0].fd = xcb_fd;
    fds[0].events = POLLIN;
    fds[1].fd = exit_fd;
    fds[1].events = POLLIN;

    for (;;) {
        /* Сначала вычерпываем всё, что xcb уже вычитала из сокета в свой
           внутренний буфер - poll() не разбудит нас повторно на данные,
           которые уже лежат в этом буфере, а не в самом файловом
           дескрипторе. */
        xcb_generic_event_t *ev;
        int handled_any = 0;
        while ((ev = xcb_poll_for_event(conn))) {
            /* Сжатие MotionNotify (подтормаживание при перетаскивании окна
               мышью): грэб указателя в window_drag_press.c ставится с
               обычным XCB_EVENT_MASK_POINTER_MOTION (не *_HINT), поэтому
               X-сервер шлёт координату на КАЖДОЕ микро-перемещение мыши. На
               мыши с высокой частотой опроса (500-1000 Гц) события успевают
               скопиться в буфере xcb быстрее, чем этот цикл успевает их
               разгрести по одному xcb_configure_window на событие - а
               поскольку они применяются строго по очереди, окно на экране
               некоторое время "тащится" по уже устаревшим промежуточным
               координатам, заметно отставая от текущего положения курсора.
               window_motion_notify() использует только e->root_x/root_y
               самого события (не дельту от предыдущего вызова - см. её
               реализацию), поэтому для СЕРИИ подряд идущих MotionNotify
               достаточно применить одно последнее, отбросив все
               промежуточные без обработки - конечный визуальный результат
               тот же, но без накопленной задержки. Событие, найденное СРАЗУ
               после серии (не MotionNotify), не должно потеряться - оно
               обрабатывается отдельным, обычным проходом на следующей
               итерации этого же while. */
            if ((ev->response_type & ~0x80) == XCB_MOTION_NOTIFY) {
                xcb_generic_event_t *next;
                while ((next = xcb_poll_for_event(conn)) &&
                       (next->response_type & ~0x80) == XCB_MOTION_NOTIFY) {
                    free(ev);
                    ev = next;
                }
                wm_handle_event(conn, screen, ev); /* последнее накопленное движение */
                free(ev);
                handled_any = 1;
                if (!next) continue; /* очередь пуста - ждём следующего пробуждения poll() */
                ev = next; /* не-motion событие сразу за серией - обрабатываем как обычно ниже */
            }

            wm_handle_event(conn, screen, ev);
            free(ev);
            handled_any = 1;
        }
        if (handled_any) xcb_flush(conn);

        if (xcb_connection_has_error(conn)) return; /* X-сервер разорвал соединение */

        int pr = poll(fds, 2, -1);
        if (pr < 0) {
            if (errno == EINTR) continue;
            return;
        }

        if (fds[1].revents & POLLIN) return; /* SIGTERM/SIGINT через self-pipe (main.c) */
        /* fds[0].revents & POLLIN -> следующая итерация вычерпает события через xcb_poll_for_event() */
    }
}

void wm_cleanup(xcb_connection_t *conn) {
    icons_cleanup();
    ewmh_cleanup(conn);
    /* освобождение прочих глобальных ресурсов WM при выходе */
}
