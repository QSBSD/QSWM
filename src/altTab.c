#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include "keysyms_min.h"
#include <xcb/render.h>
#include "altTab.h"
#include "altTab_render.h"
#include "window.h"
#include "config.h"
#include "config_numlock.h"
#include "xrender_util.h"
#include "ewmh.h"
#include "wm.h"

#define MAX_ITEMS   64   /* верхняя граница на число одновременно открытых окон */

static xcb_key_symbols_t *keysyms       = NULL;
static xcb_keycode_t      tab_keycode   = 0;
static xcb_keycode_t      alt_l_keycode = 0;
static xcb_keycode_t      alt_r_keycode = 0;
static xcb_keycode_t      esc_keycode   = 0;

static xcb_window_t overlay      = XCB_NONE;
static xcb_render_picture_t overlay_picture = XCB_NONE; /* живёт от alttab_open()
    до alttab_close(): раньше alttab_draw() создавала/уничтожала бы Picture на
    каждый вызов, а внутри одной сессии Alt-Tab это происходит на каждое
    нажатие Tab при циклическом переключении, хотя overlay-окно и его
    размер за это время не меняются */
static int          active       = 0;
static int          selected     = 0;
/* Снимок хранит xcb_window_t, а не client_t* напрямую: пока оверлей открыт,
   грабится только клавиатура (см. alttab_open), указатель мыши остаётся
   свободен — окно можно закрыть кликом по кнопке ×, и client_t будет
   освобождён window_unmanage() ещё до отпускания Alt. Указатель на такую
   структуру был бы use-after-free; xcb_window_t безопасно резолвится через
   client_find_by_window() непосредственно перед использованием, и если
   окно уже закрыто — резолвится в NULL. */
static xcb_window_t snapshot[MAX_ITEMS];
static int          snapshot_count = 0;
static uint16_t     overlay_w, overlay_h;

/* Вертикальная позиция оверлея из [appearance].alttab_position (пункт 6).
   0 = center (по умолчанию), 1 = top, 2 = bottom. Горизонтально оверлей
   всегда центрирован — ТЗ и config.conf.example не описывают отдельного
   горизонтального смещения. */
typedef enum { ALTTAB_POS_CENTER = 0, ALTTAB_POS_TOP, ALTTAB_POS_BOTTOM } alttab_pos_t;
static alttab_pos_t alttab_position = ALTTAB_POS_CENTER;

/* Цвет фона оверлея, [appearance].alttab_bg_color — чёрный по умолчанию
   (не серый), см. alttab_set_bg_color()/alttab_bg_color(). */
static double alttab_bg_r = 0.0, alttab_bg_g = 0.0, alttab_bg_b = 0.0;

void alttab_set_bg_color(double r, double g, double b) {
    alttab_bg_r = r;
    alttab_bg_g = g;
    alttab_bg_b = b;
}

xcb_render_color_t alttab_bg_color(void) {
    return xrender_color_rgba(alttab_bg_r, alttab_bg_g, alttab_bg_b, 1.0);
}

/* Раньше сетку можно было переключить в режим с подписями WM_CLASS под
   иконками. Подписи убраны насовсем — это было единственное место в
   проекте, где требовался рендер текста; теперь alt-tab и весь
   остальной WM обходятся без шрифтовой библиотеки вообще, только
   заливки и альфа-блит SVG-иконок через XRender. Сетка всегда строго
   "только иконки". */

void alttab_set_position(const char *position) {
    if (!position) { alttab_position = ALTTAB_POS_CENTER; return; }
    if (strcasestr(position, "top"))         alttab_position = ALTTAB_POS_TOP;
    else if (strcasestr(position, "bottom")) alttab_position = ALTTAB_POS_BOTTOM;
    else                                      alttab_position = ALTTAB_POS_CENTER;
}

xcb_window_t alttab_overlay_window(void) {
    return active ? overlay : (xcb_window_t)XCB_NONE;
}

/* Грабим Tab+Alt на root сразу в четырёх вариантах — с учётом того, что
   NumLock/CapsLock тоже являются модификаторами и иначе перехватят граб. */
static void grab_tab(xcb_connection_t *conn, xcb_screen_t *screen) {
    uint16_t numlock = config_numlock_mask();
    uint16_t extra[] = {0, XCB_MOD_MASK_LOCK, numlock,
                         (uint16_t)(XCB_MOD_MASK_LOCK | numlock)};
    for (size_t i = 0; i < sizeof(extra) / sizeof(extra[0]); i++) {
        xcb_grab_key(conn, 1, screen->root, XCB_MOD_MASK_1 | extra[i], tab_keycode,
                     XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    }
}

void alttab_init(xcb_connection_t *conn, xcb_screen_t *screen) {
    keysyms = xcb_key_symbols_alloc(conn);
    if (!keysyms) return;

    xcb_keycode_t *kc;

    kc = xcb_key_symbols_get_keycode(keysyms, XK_Tab);
    if (kc) { tab_keycode = kc[0]; free(kc); }

    kc = xcb_key_symbols_get_keycode(keysyms, XK_Alt_L);
    if (kc) { alt_l_keycode = kc[0]; free(kc); }

    kc = xcb_key_symbols_get_keycode(keysyms, XK_Alt_R);
    if (kc) { alt_r_keycode = kc[0]; free(kc); }

    kc = xcb_key_symbols_get_keycode(keysyms, XK_Escape);
    if (kc) { esc_keycode = kc[0]; free(kc); }

    if (tab_keycode) grab_tab(conn, screen);
    xcb_flush(conn);
}

/* Собирает снимок текущих управляемых окон и открывает оверлей по центру
   экрана. Возвращает 0 при неудаче (нет окон или не удалось перехватить
   клавиатуру), 1 при успехе. */
static int alttab_open(xcb_connection_t *conn, xcb_screen_t *screen) {
    snapshot_count = 0;
    for (client_t *c = window_get_clients(); c && snapshot_count < MAX_ITEMS; c = c->next)
        snapshot[snapshot_count++] = c->win;

    if (snapshot_count == 0) return 0;

    xcb_grab_keyboard_cookie_t gc = xcb_grab_keyboard(
        conn, 0, screen->root, XCB_CURRENT_TIME,
        XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    xcb_grab_keyboard_reply_t *gr = xcb_grab_keyboard_reply(conn, gc, NULL);
    int ok = gr && gr->status == XCB_GRAB_STATUS_SUCCESS;
    free(gr);
    if (!ok) return 0;

    int rows, cols;
    alttab_render_grid_size(snapshot_count, &rows, &cols);
    int cell_size = alttab_render_cell_size();
    overlay_w = (uint16_t)(cols * (cell_size + ALTTAB_CELL_GAP) + ALTTAB_CELL_GAP);
    overlay_h = (uint16_t)(rows * (cell_size + ALTTAB_CELL_GAP) + ALTTAB_CELL_GAP);

    /* screen->width_in_pixels/height_in_pixels — снимок с момента
       xcb_connect() (см. аналогичный комментарий в window.c); берём
       актуальную геометрию root, иначе после смены разрешения оверлей
       позиционируется по устаревшим размерам экрана. */
    uint16_t scr_w, scr_h;
    screen_get_size(conn, screen, &scr_w, &scr_h);

    int16_t ox = (int16_t)((scr_w - overlay_w) / 2);
    int16_t oy;
    switch (alttab_position) {
        case ALTTAB_POS_TOP:    oy = 40; break;
        case ALTTAB_POS_BOTTOM: oy = (int16_t)(scr_h - overlay_h - 40); break;
        default:                oy = (int16_t)((scr_h - overlay_h) / 2); break;
    }

    overlay = xcb_generate_id(conn);
    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
    uint32_t values[3] = {
        screen->black_pixel,
        1, /* override-redirect: мимо reparenting/декораций, поверх всего */
        XCB_EVENT_MASK_EXPOSURE
    };
    xcb_create_window(conn, XCB_COPY_FROM_PARENT, overlay, screen->root,
                       ox, oy, overlay_w, overlay_h, 0,
                       XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual,
                       mask, values);

    uint32_t stack_mode = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(conn, overlay, XCB_CONFIG_WINDOW_STACK_MODE, &stack_mode);
    xcb_map_window(conn, overlay);
    xcb_flush(conn);

    overlay_picture = xrender_create_picture_for_visual(conn, overlay, screen->root_visual);

    selected = 0;
    active = 1;
    alttab_render_draw(conn, overlay_picture, overlay_w, overlay_h, snapshot, snapshot_count, selected);
    return 1;
}

static void alttab_close(xcb_connection_t *conn) {
    if (!active) return;
    xcb_ungrab_keyboard(conn, XCB_CURRENT_TIME);
    if (overlay_picture != XCB_NONE) {
        xcb_render_free_picture(conn, overlay_picture);
        overlay_picture = XCB_NONE;
    }
    if (overlay != XCB_NONE) {
        xcb_destroy_window(conn, overlay);
        overlay = XCB_NONE;
    }
    active = 0;
    xcb_flush(conn);
}

void alttab_handle_key_press(xcb_connection_t *conn, xcb_screen_t *screen,
                              xcb_key_press_event_t *e) {
    if (!tab_keycode) return;

    if (!active) {
        if (e->detail == tab_keycode && (e->state & XCB_MOD_MASK_1))
            alttab_open(conn, screen);
        return;
    }

    /* Оверлей уже открыт — клавиатура перехвачена целиком (grab), сюда
       приходят все нажатия вне зависимости от исходных грабов. */
    if (e->detail == tab_keycode) {
        selected = (selected + 1) % snapshot_count;
        alttab_render_draw(conn, overlay_picture, overlay_w, overlay_h, snapshot, snapshot_count, selected);
    } else if (esc_keycode && e->detail == esc_keycode) {
        alttab_close(conn); /* отмена без смены фокуса */
    }
}

void alttab_handle_key_release(xcb_connection_t *conn, xcb_screen_t *screen,
                                xcb_key_release_event_t *e) {
    if (!active) return;
    if (e->detail != alt_l_keycode && e->detail != alt_r_keycode) return;

    client_t *chosen = (selected >= 0 && selected < snapshot_count)
                            ? client_find_by_window(snapshot[selected]) : NULL;

    alttab_close(conn);

    if (chosen) {
        /* Свёрнутое окно (frame был unmap'нут при minimize) нужно сперва
           вернуть на экран — иначе поднятие/фокус ниже ничего не покажут. */
        if (chosen->minimized)
            window_toggle_minimize(conn, screen, chosen);

        uint32_t stack_mode = XCB_STACK_MODE_ABOVE;
        xcb_configure_window(conn, chosen->frame, XCB_CONFIG_WINDOW_STACK_MODE, &stack_mode);
        window_set_focus(conn, screen, chosen);
        xcb_flush(conn);
    }
}

void alttab_expose(xcb_connection_t *conn, xcb_screen_t *screen, xcb_expose_event_t *e) {
    (void)screen; /* оверлей уже открыт, геометрия зафиксирована в alttab_open() */
    if (!active || e->window != overlay) return;
    alttab_render_draw(conn, overlay_picture, overlay_w, overlay_h, snapshot, snapshot_count, selected);
}
