#ifndef MYWM_CONFIG_H
#define MYWM_CONFIG_H

#include <xcb/xcb.h>

#define CONFIG_MAX_BINDINGS 128
#define CONFIG_CMD_MAX      256

typedef enum {
    ACTION_NONE = 0,
    ACTION_ALTTAB,   /* владеет altTab.c, здесь только для полноты парсинга */
    ACTION_CLOSE,
    ACTION_MAXIMIZE,
    ACTION_MINIMIZE,
    ACTION_LAUNCHER,
    ACTION_FULLSCREEN,
    ACTION_EXEC      /* произвольная shell-команда, [exec] в config.conf */
} wm_action_t;

typedef struct {
    uint16_t      modifiers; /* маска XCB_MOD_MASK_* без учёта Lock/Mod2 */
    xcb_keysym_t  keysym;
    xcb_keycode_t keycode;   /* заполняется в config_grab_keys() */
    wm_action_t   action;
    char          command[CONFIG_CMD_MAX]; /* для ACTION_EXEC */
} keybinding_t;

typedef struct {
    keybinding_t bindings[CONFIG_MAX_BINDINGS];
    int          binding_count;

    /* [appearance] — применяется в window.c (декорации, пункт 3) и
       altTab.c (позиция оверлея, пункт 4) через window_set_appearance()/
       alttab_set_position(), вызываемые из wm_init() (пункт 6) */
    uint16_t titlebar_height;
    uint16_t button_size;
    int      icon_left;
    char     alttab_position[32];
    /* Масштаб интерфейса Alt+Tab: размер квадрата под иконку в сетке,
       px, [appearance].alttab_icon_size. От него зависят также размер
       окна оверлея и рамка выделения (alttab_render_set_cell_size()
       в wm_init()). */
    uint16_t alttab_icon_size;

    /* Цвет фона titlebar и фона оверлея Alt+Tab, [appearance].titlebar_color /
       [appearance].alttab_bg_color, формат "#RRGGBB". По умолчанию — чёрный
       (config_defaults()), не серый. Компоненты в [0.0, 1.0] для
       xrender_color_rgba(). */
    double   titlebar_color_r, titlebar_color_g, titlebar_color_b;
    double   alttab_bg_r, alttab_bg_g, alttab_bg_b;

    /* команда, которую запускает биндинг launcher */
    char launcher_cmd[CONFIG_CMD_MAX];

    /* [icons] - пользовательские SVG-иконки по WM_CLASS, задаются как
       "ИмяКласса = /путь/к/файлу.svg". Хранятся как параллельные массивы
       (а не icon_override_t из icons.h), чтобы config.h не тянул за
       собой icons.h - см. icons_set_overrides() в wm.c:wm_init(), где
       эти два массива конвертируются в icon_override_t[] перед вызовом. */
    /* [icons].dir - каталог, в котором для ЛЮБОГО приложения ищется
       "{app_class}.svg" (и вариант в нижнем регистре), раньше системных
       icon-тем. Пусто - каталог не задан, эта проверка пропускается.
       См. icons_set_custom_dir() в icons.c. */
    char icon_dir[256];
} wm_config_t;

/* Функции конфигурации разнесены по модулям:
   - config_defaults.h — config_defaults()
   - config_load.h     — config_load()
   - config_keys.h      — config_grab_keys(), config_find_binding()
   - config_exec.h      — config_spawn_launcher(), config_run_autostart()
   - config_numlock.h   — config_detect_numlock(), config_numlock_mask()
   Этот заголовок содержит только общие типы, чтобы каждый модуль мог
   подключать его без циклических зависимостей. */

#endif
