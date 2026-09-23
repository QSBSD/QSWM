#ifndef MYWM_ALTTAB_H
#define MYWM_ALTTAB_H

#include <xcb/xcb.h>
#include <xcb/render.h>

/* Цвет фона оверлея Alt+Tab, [appearance].alttab_bg_color ("#RRGGBB");
   по умолчанию чёрный. Вызывается один раз из wm_init() после
   config_load(). */
void alttab_set_bg_color(double r, double g, double b);
xcb_render_color_t alttab_bg_color(void);

/* Применяет [appearance].alttab_position из config.conf (пункт 6).
   Поддерживаемые значения: "center" (по умолчанию), "top", "bottom" —
   остальные подстроки трактуются как "center". Вызывается один раз из
   wm_init() после config_load(), до alttab_init(). */
void alttab_set_position(const char *position);

/* Регистрирует грабы клавиши Tab (с модификатором Alt) на root-окне.
   Вызывается один раз при старте WM, после wm_init(). */
void alttab_init(xcb_connection_t *conn, xcb_screen_t *screen);

/* Обрабатывает KeyPress. Если Tab нажат вместе с Alt и оверлей ещё не
   открыт — открывает его (grab-keyboard + сетка иконок). Если оверлей
   уже открыт — очередное нажатие Tab двигает выделение на следующее
   окно. Esc отменяет переключение без смены фокуса. */
void alttab_handle_key_press(xcb_connection_t *conn, xcb_screen_t *screen,
                              xcb_key_press_event_t *e);

/* Обрабатывает KeyRelease. Отпускание Alt подтверждает выбор: оверлей
   закрывается, выбранное окно поднимается и получает фокус. */
void alttab_handle_key_release(xcb_connection_t *conn, xcb_screen_t *screen,
                                xcb_key_release_event_t *e);

/* Перерисовывает содержимое оверлея (вызывается из wm_run по Expose). */
void alttab_expose(xcb_connection_t *conn, xcb_screen_t *screen, xcb_expose_event_t *e);

/* Окно оверлея, если он сейчас открыт, иначе XCB_NONE. Используется в
   wm.c, чтобы отличить Expose оверлея от Expose декораций окон. */
xcb_window_t alttab_overlay_window(void);

#endif
