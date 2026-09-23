#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include "config_load.h"
#include "config_defaults.h"
#include "config_parse.h"

/* ---- разбор текстового формата config.conf ---------------------
   Разбор комбинаций клавиш ("mod+q" -> модификаторы+keysym) вынесен в
   config_parse.c (config_trim/config_streq_ci/config_parse_combo). */

static wm_action_t action_from_key(const char *key) {
    if (config_streq_ci(key, "altTab"))    return ACTION_ALTTAB;
    if (config_streq_ci(key, "close"))     return ACTION_CLOSE;
    if (config_streq_ci(key, "maximize"))  return ACTION_MAXIMIZE;
    if (config_streq_ci(key, "minimize"))  return ACTION_MINIMIZE;
    if (config_streq_ci(key, "launcher"))  return ACTION_LAUNCHER;
    if (config_streq_ci(key, "fullscreen")) return ACTION_FULLSCREEN;
    return ACTION_NONE;
}

/* Заменяет в cfg->bindings биндинг с данным action (если уже есть -
   переопределяем значение по умолчанию) либо добавляет новый. */
static void set_binding(wm_config_t *cfg, wm_action_t action,
                         uint16_t mods, xcb_keysym_t keysym) {
    for (int i = 0; i < cfg->binding_count; i++) {
        if (cfg->bindings[i].action == action) {
            cfg->bindings[i].modifiers = mods;
            cfg->bindings[i].keysym = keysym;
            return;
        }
    }
    if (cfg->binding_count < CONFIG_MAX_BINDINGS) {
        cfg->bindings[cfg->binding_count].modifiers = mods;
        cfg->bindings[cfg->binding_count].keysym = keysym;
        cfg->bindings[cfg->binding_count].keycode = 0;
        cfg->bindings[cfg->binding_count].action = action;
        cfg->binding_count++;
    }
}

/* Добавляет биндинг ACTION_EXEC (произвольная команда) - в отличие от
   set_binding() не переопределяет существующие по совпадению action,
   т.к. таких биндингов может быть много одновременно (по одному на
   каждую строку секции [exec]). */
static void add_exec_binding(wm_config_t *cfg, uint16_t mods,
                              xcb_keysym_t keysym, const char *cmd) {
    if (cfg->binding_count >= CONFIG_MAX_BINDINGS) return;
    keybinding_t *kb = &cfg->bindings[cfg->binding_count];
    kb->modifiers = mods;
    kb->keysym = keysym;
    kb->keycode = 0;
    kb->action = ACTION_EXEC;
    strncpy(kb->command, cmd, sizeof(kb->command) - 1);
    kb->command[sizeof(kb->command) - 1] = '\0';
    cfg->binding_count++;
}

/* Ищет "mod = ..." в секции [hotkeys], не трогая остальной разбор.
   Нужен отдельным проходом ДО разбора самих биндингов: раньше mod_mask
   был обычной переменной цикла и применялся только к строкам,
   встреченным ПОСЛЕ "mod = ...", - если пользователь в конфиге сначала
   перечислял хоткеи "mod+..." и лишь потом указывал сам mod, эти хоткеи
   молча получали дефолтный XCB_MOD_MASK_1 вместо заданного модификатора.
   Двухпроходный разбор снимает эту зависимость от порядка строк. */
static uint16_t find_mod_mask(FILE *f) {
    uint16_t mod_mask = XCB_MOD_MASK_1; /* значение по умолчанию для "mod" */
    char line[256];
    char section[32] = "";

    while (fgets(line, sizeof(line), f)) {
        if (!strchr(line, '\n') && !feof(f)) {
            int ch;
            while ((ch = fgetc(f)) != EOF && ch != '\n') { }
            continue;
        }

        char *s = config_trim(line);
        if (*s == '\0' || *s == '#' || *s == ';') continue;

        if (*s == '[') {
            char *end = strchr(s, ']');
            if (end) {
                *end = '\0';
                strncpy(section, s + 1, sizeof(section) - 1);
                section[sizeof(section) - 1] = '\0';
            }
            continue;
        }

        if (!config_streq_ci(section, "hotkeys")) continue;

        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = config_trim(s);
        char *val = config_trim(eq + 1);

        if (config_streq_ci(key, "mod")) {
            uint16_t m = config_modmask_from_name(val);
            if (m) mod_mask = m;
        }
    }

    rewind(f);
    return mod_mask;
}

int config_load(const char *path, wm_config_t *cfg) {
    config_defaults(cfg);

    char resolved[512];
    if (!path) {
        const char *home = getenv("HOME");
        if (!home) home = "";
        snprintf(resolved, sizeof(resolved), "%s/.config/QSWM/config.conf", home);
        path = resolved;
    }

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    uint16_t mod_mask = find_mod_mask(f); /* один проход заранее - см. комментарий выше */

    char line[256];
    char section[32] = "";

    while (fgets(line, sizeof(line), f)) {
        /* Строка длиннее буфера: fgets не дочитал до '\n', остаток
           попал бы в следующий вызов и разобрался бы как отдельная
           "строка" (мусор). Дочитываем и отбрасываем хвост целиком,
           текущую строку тоже пропускаем как некорректную. */
        if (!strchr(line, '\n') && !feof(f)) {
            int ch;
            while ((ch = fgetc(f)) != EOF && ch != '\n') { }
            continue;
        }

        char *s = config_trim(line);
        if (*s == '\0' || *s == '#' || *s == ';') continue;

        if (*s == '[') {
            char *end = strchr(s, ']');
            if (end) {
                *end = '\0';
                strncpy(section, s + 1, sizeof(section) - 1);
                section[sizeof(section) - 1] = '\0';
            }
            continue;
        }

        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = config_trim(s);
        char *val = config_trim(eq + 1);

        if (config_streq_ci(section, "hotkeys")) {
            if (config_streq_ci(key, "mod")) {
                uint16_t m = config_modmask_from_name(val);
                if (m) mod_mask = m;
                continue;
            }
            wm_action_t action = action_from_key(key);
            if (action == ACTION_NONE) continue;

            uint16_t mods;
            xcb_keysym_t keysym;
            if (config_parse_combo(val, mod_mask, &mods, &keysym) == 0)
                set_binding(cfg, action, mods, keysym);
        } else if (config_streq_ci(section, "appearance")) {
            /* atoi() отрицательного/мусорного значения, приведённое напрямую
               к uint16_t, заворачивается в огромное число (напр. -1 -> 65535)
               - тогда декорация рисуется битой на весь экран. Разумные
               границы: titlebar 8..200px, кнопка 4..128px; всё вне диапазона
               отбрасываем, оставляя уже действующее значение (по умолчанию
               из config_defaults() или ранее заданное этим же конфигом). */
            if (config_streq_ci(key, "titlebar_height")) {
                int v = atoi(val);
                if (v >= 8 && v <= 200) cfg->titlebar_height = (uint16_t)v;
            }
            else if (config_streq_ci(key, "button_size")) {
                int v = atoi(val);
                if (v >= 4 && v <= 128) cfg->button_size = (uint16_t)v;
            }
            else if (config_streq_ci(key, "icon_left")) cfg->icon_left = config_streq_ci(val, "true");
            else if (config_streq_ci(key, "alttab_icon_size")) {
                /* Масштаб интерфейса Alt+Tab: размер иконки/ячейки, px.
                   Границы аналогичны button_size выше — отбрасываем
                   мусор/отрицательные значения, оставляя действующее. */
                int v = atoi(val);
                if (v >= 16 && v <= 256) cfg->alttab_icon_size = (uint16_t)v;
            }
            else if (config_streq_ci(key, "alttab_position")) {
                strncpy(cfg->alttab_position, val, sizeof(cfg->alttab_position) - 1);
                cfg->alttab_position[sizeof(cfg->alttab_position) - 1] = '\0';
            }
            else if (config_streq_ci(key, "titlebar_color")) {
                double r, g, b;
                if (config_parse_hex_color(val, &r, &g, &b) == 0) {
                    cfg->titlebar_color_r = r;
                    cfg->titlebar_color_g = g;
                    cfg->titlebar_color_b = b;
                }
            }
            else if (config_streq_ci(key, "alttab_bg_color")) {
                double r, g, b;
                if (config_parse_hex_color(val, &r, &g, &b) == 0) {
                    cfg->alttab_bg_r = r;
                    cfg->alttab_bg_g = g;
                    cfg->alttab_bg_b = b;
                }
            }
        } else if (config_streq_ci(section, "exec")) {
            /* "combo = shell-команда", напр. "A-Z = okr". Комбинация
               парсится тем же config_parse_combo(), что и [hotkeys], с
               учётом текущего "mod" (двухпроходный mod_mask сверху). */
            uint16_t mods;
            xcb_keysym_t keysym;
            if (config_parse_combo(key, mod_mask, &mods, &keysym) == 0)
                add_exec_binding(cfg, mods, keysym, val);
        } else if (config_streq_ci(section, "icons")) {
            /* "dir = /каталог" - единственный ключ секции: каталог, где
               для ЛЮБОГО приложения ищется "{app_class}.svg"/нижний
               регистр (см. icons_set_custom_dir() в icons.c). */
            if (config_streq_ci(key, "dir")) {
                strncpy(cfg->icon_dir, val, sizeof(cfg->icon_dir) - 1);
                cfg->icon_dir[sizeof(cfg->icon_dir) - 1] = '\0';
            }
        } else if (config_streq_ci(section, "general")) {
            if (config_streq_ci(key, "launcher_cmd")) {
                strncpy(cfg->launcher_cmd, val, sizeof(cfg->launcher_cmd) - 1);
                cfg->launcher_cmd[sizeof(cfg->launcher_cmd) - 1] = '\0';
            }
        }
    }

    fclose(f);
    return 0;
}
