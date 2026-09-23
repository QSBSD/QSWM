#ifndef MYWM_WINDOW_H
#define MYWM_WINDOW_H

#include <xcb/xcb.h>
#include <xcb/render.h>

/* Значения по умолчанию (пункт 3), используются пока config.conf не
   загружен или не переопределяет их — см. window_set_appearance(). */
#define TITLEBAR_HEIGHT_DEFAULT 24
#define BUTTON_SIZE_DEFAULT     16
#define BUTTON_MARGIN   4
#define BUTTON_GAP      4

#define RESIZE_MARGIN   6   /* ширина зоны захвата края для ресайза, px */
#define MIN_WIDTH       120
#define MIN_HEIGHT      80

#define APP_CLASS_MAX   80  /* WM_CLASS окна — используется для подбора SVG-иконки (icons.c) */

/* Биты региона снэппинга (Meta+стрелки, см. snap.c). Комбинация LEFT|TOP
   и т.п. даёт четверть экрана, одиночный LEFT/RIGHT — половину. mask==0
   в window_apply_snap() означает "открепить" (Ctrl+Tab). */
#define SNAP_LEFT   (1 << 0)
#define SNAP_RIGHT  (1 << 1)
#define SNAP_TOP    (1 << 2)
#define SNAP_BOTTOM (1 << 3)

typedef struct client {
    xcb_window_t   win;      /* исходное окно приложения */
    xcb_window_t   frame;    /* окно-декорация (родитель) */
    int16_t        x, y;     /* позиция frame на экране */
    uint16_t       width, height; /* размер клиентской области (без titlebar) */
    char           app_class[APP_CLASS_MAX]; /* WM_CLASS (class-часть), для icons_draw() */
    xcb_render_picture_t deco_picture; /* Picture поверх frame (см. draw_decoration в
                                       window.c), создаётся один раз и живёт всё время
                                       жизни клиента: переиспользуется между
                                       перерисовками вместо создания заново на каждый
                                       Expose/resize/клик. Рисует по текущей геометрии
                                       drawable, поэтому изменение ширины окна не
                                       требует отдельного уведомления. */
    int            maximized;
    int            minimized;
    int            fullscreen;
    int16_t        saved_x, saved_y;
    uint16_t       saved_width, saved_height; /* геометрия до maximize (window_maximize.c) */
    /* НАЙДЕННЫЙ БАГ: window_fullscreen.c раньше писал/читал ЭТИ ЖЕ поля
       saved_x/y/width/height, что и window_maximize.c, да ещё и сохранял
       их условно ("if (!c->maximized)"). Если окно уже было maximized
       (это дефолт для новых окон - см. pending_auto_maximize) и по нему
       нажали F11, fullscreen ничего не сохранял (условие ложно), а при
       повторном F11 подставлял сюда geometry, оставшуюся с САМОГО
       ПЕРВОГО maximize - "естественный" маленький размер окна при
       старте, а не тот maximized-размер, что был виден непосредственно
       перед входом в fullscreen. Внешне это выглядело как "F11 обратно
       разворачивает окно в его исходное маленькое состояние" (репорт с
       Minecraft/GLFW: после автоматического maximize у нового окна
       клики внутри не попадали, ровно до первого переключения F11
       туда-обратно, после чего процент попадания клика обычно
       восстанавливается - GLFW пересчитывает своё внутреннее
       отображение курсора на очередном настоящем resize; но раз F11
       "туда-обратно" возвращал окно не в maximized, а в исходный
       маленький размер, второй баг маскировал первый и создавал
       впечатление, что дело именно в размере).
       Теперь у fullscreen — отдельные поля ниже, которые всегда
       сохраняют ТЕКУЩУЮ (а не "естественную") геометрию и флаг
       maximized перед входом в fullscreen, и полностью восстанавливают
       оба при выходе. */
    int16_t        prefs_x, prefs_y;
    uint16_t       prefs_width, prefs_height; /* геометрия непосредственно до входа в fullscreen */
    int            prefs_maximized; /* окно было maximized до fullscreen - восстановить это состояние, а не saved_* от maximize */
    int            snapped;      /* 0 или маска SNAP_* — текущий регион снэппинга */
    int16_t        presnap_x, presnap_y;
    uint16_t       presnap_width, presnap_height; /* геометрия до снэппинга (Ctrl+Tab восстанавливает её) */
    int            presnap_maximized; /* окно было развёрнуто (□) до Meta+стрелок - Ctrl+Tab должен вернуть его в maximized, а не к "естественному" размеру */
    /* ICCCM WM_NORMAL_HINTS (PMinSize/PMaxSize), см. window_client.c:
       fetch_normal_hints(). min_width/min_height не бывают меньше
       MIN_WIDTH/MIN_HEIGHT (это по-прежнему абсолютный пол WM). max_*
       == 0 означает "не задано клиентом" (нет верхнего предела). Резайз
       (window_drag_motion.c), ConfigureRequest (window_configure.c) и
       maximize/fullscreen (window_maximize.c/window_fullscreen.c) обязаны
       эти границы учитывать — иначе, например, окно калькулятора с
       фиксированным размером (min==max) можно растянуть/сжать мышью так,
       что само приложение перестанет что-либо перерисовывать корректно. */
    uint16_t       min_width, min_height;
    uint16_t       max_width, max_height; /* 0 = без ограничения сверху */
    /* Авто-maximize нового окна (window_manage()) откладывается до первого
       Expose (window_expose(), window_decoration.c) вместо немедленного
       вызова прямо при создании: некоторые клиенты (GLFW/LWJGL-игры вроде
       Minecraft) кэшируют размер окна для пересчёта координат курсора в
       момент своей инициализации, и resize, прилетевший ДО того как клиент
       её закончил, там не переучитывается — клики после этого мимо, пока
       пользователь вручную не переключит fullscreen/maximize туда-обратно.
       Обычный юзер этой задержки в один кадр не заметит. */
    int            pending_auto_maximize;
    struct client *next;
} client_t;

/* Применяет секцию [appearance] из config.conf (пункт 6). Вызывается
   один раз из wm_init() после config_load(), до первого window_manage().
   titlebar_height/button_size == 0 -> оставить текущее значение
   (значение по умолчанию, если вызывается впервые). */
void window_set_appearance(uint16_t titlebar_height, uint16_t button_size, int icon_left);
/* Цвет фона titlebar, [appearance].titlebar_color ("#RRGGBB"); по
   умолчанию чёрный — см. appearance-структуру в window_appearance.c. */
void window_set_titlebar_color(double r, double g, double b);
xcb_render_color_t window_titlebar_color(void);

/* Текущие значения из [appearance] (см. window_set_appearance выше) —
   нужны window_drag.c для расчёта зон захвата ресайза/кнопок. */
uint16_t window_titlebar_height(void);
uint16_t window_button_size(void);
int      window_icon_left(void);

/* Перерисовывает декорацию (titlebar, иконка, кнопки) клиента c —
   вызывается и из window.c (resize/maximize/...), и из window_drag.c
   (после окончания перетаскивания). */
void window_draw_decoration(xcb_connection_t *conn, xcb_screen_t *screen, client_t *c);

void window_manage(xcb_connection_t *conn, xcb_screen_t *screen, xcb_window_t win);
void window_unmanage(xcb_connection_t *conn, xcb_screen_t *screen, xcb_window_t win);

/* Переводит клиента в ICCCM WithdrawnState по UnmapNotify (клиент сам
   спрятал окно, без DestroyNotify) — см. window.c. */
void window_withdraw(xcb_connection_t *conn, xcb_screen_t *screen, xcb_window_t win);
void window_configure(xcb_connection_t *conn, xcb_screen_t *screen, xcb_configure_request_event_t *e);

/* Отвечает клиенту synthetic ConfigureNotify с его текущей (не изменённой)
   геометрией — используется, когда WM отклоняет ConfigureRequest окна в
   snapped/maximized/fullscreen (см. window_configure.c). */
void window_send_synthetic_configure(xcb_connection_t *conn, client_t *c);
void window_button_press(xcb_connection_t *conn, xcb_screen_t *screen, xcb_button_press_event_t *e);
void window_motion_notify(xcb_connection_t *conn, xcb_motion_notify_event_t *e);
void window_button_release(xcb_connection_t *conn, xcb_screen_t *screen, xcb_button_release_event_t *e);
void window_expose(xcb_connection_t *conn, xcb_screen_t *screen, xcb_expose_event_t *e);

/* focus-follows-mouse: фокусирует окно/frame, на которое перешёл
   указатель, если оно ещё не в фокусе (window_client.c). */
void window_handle_enter_notify(xcb_connection_t *conn, xcb_screen_t *screen,
                                 xcb_enter_notify_event_t *e);

/* Действия окна, вызываемые как по кнопкам декорации, так и по хоткеям
   из config.conf (пункт 6, диспетчеризация в wm.c). */
void window_close(xcb_connection_t *conn, client_t *c);
void window_toggle_maximize(xcb_connection_t *conn, xcb_screen_t *screen, client_t *c);
void window_toggle_minimize(xcb_connection_t *conn, xcb_screen_t *screen, client_t *c);
void window_toggle_fullscreen(xcb_connection_t *conn, xcb_screen_t *screen, client_t *c);

/* Снэппинг окна по Meta+стрелки (см. snap.c). mask — комбинация битов
   SNAP_LEFT/RIGHT/TOP/BOTTOM: одна сторона -> половина экрана, две
   смежные (напр. LEFT|TOP) -> четверть. mask==0 открепляет окно и
   возвращает его к геометрии до снэппинга (Ctrl+Tab). Если окно было
   maximized/fullscreen — эти режимы сначала снимаются. */
void window_apply_snap(xcb_connection_t *conn, xcb_screen_t *screen, client_t *c, int mask);

/* Ограничивает ширину/высоту (через w,h) диапазоном c->min_width..c->max_width
   и c->min_height..c->max_height (WM_NORMAL_HINTS, см. window.h). Общая
   точка для window_drag_motion.c (интерактивный ресайз мышью),
   window_configure.c (ConfigureRequest) и window_maximize.c/
   window_fullscreen.c (чтобы не растягивать окно с фиксированным
   максимальным размером за его пределы). */
void window_clamp_size(const client_t *c, uint16_t *w, uint16_t *h);

/* Читает WM_NORMAL_HINTS (PMinSize/PMaxSize) окна c->win и заполняет
   c->min_width/min_height/max_width/max_height. Вызывается один раз из
   window_manage(), до первой отрисовки/клампа размера. Реализация —
   window_hints.c. */
void window_fetch_normal_hints(xcb_connection_t *conn, client_t *c);

/* Сворачивает все текущие управляемые окна (Meta+Tab, "показать стол"),
   независимо от их maximized/snapped состояния — в отличие от
   window_toggle_minimize() не восстанавливает уже свёрнутые. */
void window_minimize_all(xcb_connection_t *conn, xcb_screen_t *screen);

/* Окно, которое сейчас в фокусе X-сервера (по нему выполняются хоткеи
   close/maximize/minimize). Возвращает NULL, если фокус не на клиенте. */
client_t *window_focused_client(xcb_connection_t *conn);

/* Пассивный грэб клика "по нефокусному окну" (клик-фокус) и его снятие —
   см. window_client.c, баг №7. */
void window_grab_click_to_focus(xcb_connection_t *conn, xcb_window_t win);
void window_ungrab_click_to_focus(xcb_connection_t *conn, xcb_window_t win);

/* Единая точка смены активного окна: фокус + _NET_ACTIVE_WINDOW + грэб
   клика (баг №7). Всегда использовать вместо ручных
   xcb_set_input_focus()+ewmh_set_active_window(). c == NULL — снять
   фокус со всех окон. */
void window_set_focus(xcb_connection_t *conn, xcb_screen_t *screen, client_t *c);

client_t *client_find_by_frame(xcb_window_t frame);
client_t *client_find_by_window(xcb_window_t win);

/* Голова связного списка управляемых окон (для altTab.c: снимок списка
   при открытии оверлея). Список принадлежит window.c, не изменять. */
client_t *window_get_clients(void);

#endif
